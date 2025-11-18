#pragma once
// ECE_UAV.hpp  -- drop into your project and #include where needed

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <glm/glm.hpp>
#include <iostream>
#include <mutex>
#include <random>
#include <thread>

struct ECE_UAV
{
    // Physical properties
    float mass = 1.0f;      // kg
    float maxForce = 20.0f; // N (magnitude)
    float gravity = 10.0f;  // N (downward)
    float size_m = 0.20f;   // bounding cube 0.20 m (20 cm)

    // Kinematic state (protected by mutex)
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 velocity = glm::vec3(0.0f);
    glm::vec3 acceleration = glm::vec3(0.0f);

    // Threading
    std::thread worker;
    std::atomic<bool> running{false};
    std::mutex mtx;

    // Behavioral configuration
    glm::vec3 ascendTarget = glm::vec3(0.0f, 50.0f, 0.0f); // NOTE: uses z-up convention, will adapt below
    glm::vec3 sphereCenter = glm::vec3(0.0f, 50.0f, 0.0f); // center of virtual sphere (x,y,z) with z-up
    float sphereRadius = 10.0f;
    float waitSeconds = 5.0f;     // sat on ground
    float sphereDuration = 60.0f; // seconds to roam on sphere surface after reaching it
    float maxAscendSpeed = 2.0f;  // m/s (while ascending)
    float minTangentialSpeed = 2.0f;
    float maxTangentialSpeed = 10.0f;

    // internal random generator for tangential wander
    std::mt19937 rng;
    std::uniform_real_distribution<float> uniform01{0.0f, 1.0f};

    // internal timers
    std::chrono::steady_clock::time_point startTime;

    // Constructor: initial pos
    ECE_UAV(const glm::vec3 &startPos = glm::vec3(0.0f)) : position(startPos), rng(std::random_device{}())
    {
    }

    // Start the internal thread (calls external threadFunction per spec)
    void start();

    // Request stop and join
    void stop()
    {
        running.store(false);
    }
    void join()
    {
        if (worker.joinable())
            worker.join();
    }

    // Thread-safe getters
    glm::vec3 getPosition()
    {
        std::lock_guard<std::mutex> lk(mtx);
        return position;
    }
    glm::vec3 getVelocity()
    {
        std::lock_guard<std::mutex> lk(mtx);
        return velocity;
    }

    // Thread-safe setter for velocity (useful for collision swap)
    void setVelocity(const glm::vec3 &v)
    {
        std::lock_guard<std::mutex> lk(mtx);
        velocity = v;
    }

    // Swap velocity (atomic-ish while holding both UAV mutexes externally)
    void swapVelocity(ECE_UAV &other)
    {
        // caller should lock both mutexes in a consistent order to avoid deadlock
        std::swap(velocity, other.velocity);
    }

    // internal update function (called by the worker thread)
    void updatePhysics(float dt, float elapsedSinceStart);

  private:
    // helper: clamp vector length
    glm::vec3 clampMagnitude(const glm::vec3 &v, float maxLen)
    {
        float len2 = glm::dot(v, v);
        if (len2 <= maxLen * maxLen)
            return v;
        float inv = 1.0f / std::sqrt(len2);
        return v * (maxLen * inv);
    }
};

// External thread function required by spec
inline void threadFunction(ECE_UAV *pUAV)
{
    // Worker will run with 10 ms updates
    using clock = std::chrono::steady_clock;
    const std::chrono::milliseconds dt_ms(10);
    pUAV->running.store(true);
    pUAV->startTime = clock::now();
    auto last = clock::now();

    while (pUAV->running.load())
    {
        auto now = clock::now();
        std::chrono::duration<float> elapsed = now - pUAV->startTime;
        float t_since_start = elapsed.count();

        // compute dt as elapsed between ticks (in seconds)
        std::chrono::duration<float> frame_dt = now - last;
        float dt = frame_dt.count();
        if (dt <= 0.0f)
            dt = 0.01f; // fallback
        last = now;

        // call update
        pUAV->updatePhysics(dt, t_since_start);

        // sleep to hit ~10ms update rate
        std::this_thread::sleep_for(dt_ms);
    }
}

// start() implementation: spawn thread
inline void ECE_UAV::start()
{
    if (running.load())
        return;
    running.store(true);
    worker = std::thread(threadFunction, this);
}

// updatePhysics: single time-step physics & control
inline void ECE_UAV::updatePhysics(float dt, float elapsedSinceStart)
{
    // dt in seconds (expected ~0.01)
    // We implement a behavior state machine based on elapsed time:
    //  - [0, waitSeconds): resting on ground (z = 0) -> zero vel/acc
    //  - [waitSeconds, t_reach]: ascend toward ascendTarget with max speed limit
    //  - after reaching near ascendTarget: sphere roaming for sphereDuration

    // local copies
    glm::vec3 curPos;
    glm::vec3 curVel;

    {
        std::lock_guard<std::mutex> lk(mtx);
        curPos = position;
        curVel = velocity;
    }

    // Coordinate conventions in the rest of your code: you said z is up.
    // This code expects sphereCenter/ascendTarget set appropriately (z is up).
    // Behavior variables:
    static const float nearEpsilon = 0.1f; // meters tolerance for "reached"
    static const float radialK = 50.0f;    // radial spring stiffness (N/m)
    static const float dampingK = 5.0f;    // damping for tangential control

    glm::vec3 totalForce(0.0f);

    // gravity force (downwards in z): magnitude = mass * g => given g force 10N
    // given spec: "force of gravity (10 N in the negative z direction)"
    glm::vec3 gravityForce = glm::vec3(0.0f, 0.0f, -gravity);

    if (elapsedSinceStart < waitSeconds)
    {
        // Rest on ground: zero velocity, position z should be clamped to ground (z=0)
        // Keep hold on ground via position reset and zero vel/acc
        std::lock_guard<std::mutex> lk(mtx);
        position.z = std::max(position.z, 0.0f); // ensure not below ground
        velocity = glm::vec3(0.0f);
        acceleration = glm::vec3(0.0f);
        return;
    }

    // After initial wait: Ascend toward ascendTarget until within sphereRadius of sphereCenter+radius along z
    // We'll treat ascendTarget as the point (0,0,50) (center.z = 50). The "point above the ground"
    // Use a two-phase: ascend until within (sphereCenter.z - sphereRadius) +/- tolerance (or closer)
    // Simpler: ascend until distance to ascendTarget <= 0.5 m then switch to sphere mode
    // But spec says "approach the point (0,0,50 m)" then fly along surface of virtual sphere radius 10 m centered on
    // that point. We'll implement: ascend until distance to ascendTarget <= sphereRadius + 0.5

    // compute vector to ascend target
    glm::vec3 toAscend = ascendTarget - curPos;
    float distToAscend = glm::length(toAscend);

    // We will need to know when sphere-mode starts; implement based on elapsedSinceStart and distance:
    bool inSphereMode = false;
    if (distToAscend <= (sphereRadius + 0.5f))
    {
        // close enough -> sphere roaming mode
        inSphereMode = true;
    }

    if (!inSphereMode)
    {
        // ASCEND phase: compute desired velocity towards ascendTarget with maxAscendSpeed
        glm::vec3 dir = (distToAscend > 1e-6f) ? (toAscend / distToAscend) : glm::vec3(0.0f, 0.0f, 1.0f);
        glm::vec3 v_des = dir * maxAscendSpeed;
        // desired acceleration to reach v_des in one dt (simple PD-ish)
        glm::vec3 a_des = (v_des - curVel) / std::max(dt, 1e-4f);

        // required force = m * a_des + gravity compensation
        glm::vec3 reqForce = mass * a_des - gravityForce; // gravity is added separately (see below)
        // clamp to maxForce magnitude
        reqForce = clampMagnitude(reqForce, maxForce);

        totalForce += reqForce;
    }
    else
    {
        // SPHERE-ROAMING phase
        // Want to stay on sphere of radius R centered at sphereCenter
        // Position relative to center:
        glm::vec3 rel = curPos - sphereCenter;
        float r = glm::length(rel);
        if (r < 1e-6f)
        {
            // degenerate: push to radius in some direction
            rel = glm::vec3(0.0f, 0.0f, sphereRadius);
            r = sphereRadius;
        }
        glm::vec3 radialDir = rel / r; // outward radial

        // radial correction force: try to keep r ~ sphereRadius
        float radialError = r - sphereRadius;                       // positive => outside
        glm::vec3 radialForce = -radialK * radialError * radialDir; // spring back to radius

        // Compute current tangential velocity (remove radial component)
        glm::vec3 v_radial = glm::dot(curVel, radialDir) * radialDir;
        glm::vec3 v_tangential = curVel - v_radial;
        float v_tangential_mag = glm::length(v_tangential);

        // pick a target tangential speed (we can vary slowly)
        // Use a simple deterministic pseudo-random target that changes slowly based on time
        float tphase = elapsedSinceStart;
        float rand01 = uniform01(rng);
        float v_target = std::min(
            std::max(minTangentialSpeed + (rand01 * (maxTangentialSpeed - minTangentialSpeed)), minTangentialSpeed),
            maxTangentialSpeed);

        // pick tangential direction: orthonormal vector to radialDir
        // build an arbitrary tangent basis
        glm::vec3 tangent1;
        if (std::abs(radialDir.z) < 0.9f)
            tangent1 = glm::normalize(glm::cross(radialDir, glm::vec3(0, 0, 1)));
        else
            tangent1 = glm::normalize(glm::cross(radialDir, glm::vec3(0, 1, 0)));
        glm::vec3 tangent2 = glm::normalize(glm::cross(radialDir, tangent1));

        // get a slowly varying angle for direction (based on time and RNG)
        float ang = (tphase * 0.5f) + (rand01 * 3.14f);
        glm::vec3 desiredTangentialDir = glm::normalize(std::cos(ang) * tangent1 + std::sin(ang) * tangent2);

        glm::vec3 v_t_des = desiredTangentialDir * v_target;

        // compute acceleration needed for tangential correction
        glm::vec3 a_t = (v_t_des - v_tangential) / std::max(dt, 1e-4f);

        // damping to avoid oscillation
        glm::vec3 damping = -dampingK * v_tangential;

        // combine forces: F = m*(a_t + damping) + radialForce + compensate gravity
        glm::vec3 reqForce = mass * a_t + mass * damping + radialForce - gravityForce;

        // clamp total magnitude
        reqForce = clampMagnitude(reqForce, maxForce);

        totalForce += reqForce;
    }

    // apply physics integration using constant-acc formulas (per spec)
    // Compute acceleration = totalForce / mass
    glm::vec3 newAcc = totalForce / mass;

    // update position using x = x0 + v0*t + 0.5*a*t^2
    glm::vec3 newPos = curPos + curVel * dt + 0.5f * newAcc * dt * dt;

    // update velocity v = v0 + a*t
    glm::vec3 newVel = curVel + newAcc * dt;

    // apply simple ground collision: if below ground (z<0) clamp
    if (newPos.z < 0.0f)
    {
        newPos.z = 0.0f;
        newVel.z = 0.0f;
    }

    // commit state under lock
    {
        std::lock_guard<std::mutex> lk(mtx);
        position = newPos;
        velocity = newVel;
        acceleration = newAcc;
    }
}
