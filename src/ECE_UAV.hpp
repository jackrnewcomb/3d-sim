/*
Author: Jack Newcomb
Class: ECE6122
Last Date Modified: 11/30/2025

Description:

Header for the ECE_UAV class. Provides thread-safe getters and setters for relevant members, the requested start()
method, and a physics update() method

*/

#pragma once

#include <atomic>
#include <chrono>
#include <glm/glm.hpp>
#include <glm/gtc/random.hpp>
#include <mutex>
#include <random>
#include <thread>

class ECE_UAV
{
  public:
    /**
     * @brief The ECE_UAV constructor. Takes in a starting position
     *
     * @param Initial position
     */
    ECE_UAV(const glm::vec3 &startPos);

    /**
     * @brief Starts the UAV execution by assigning the std::thread worker to threadFunction
     */
    void start();

    /**
     * @brief Stops the UAV execution, sets member mIsRunning to false
     */
    void stop();

    /**
     * @brief Joins worker thread if joinable
     */
    void join();

    /**
     * @brief Thread-safe position getter
     *
     * @returns Current position
     */
    glm::vec3 getPosition()
    {
        std::lock_guard<std::mutex> lk(mMtx);
        return mPosition;
    }

    /**
     * @brief Thread-safe velocity getter
     *
     * @returns Current velocity
     */
    glm::vec3 getVelocity()
    {
        std::lock_guard<std::mutex> lk(mMtx);
        return mVelocity;
    }

    /**
     * @brief Thread-safe mIsRunning getter
     *
     * @returns Bool representing whether the UAV execution is running
     */
    bool getIsRunning()
    {
        std::lock_guard<std::mutex> lk(mMtx);
        return mIsRunning;
    }

    /**
     * @brief Thread-safe sphere-mode status getter
     *
     * @returns Bool representing whether we have entered sphere mode
     */
    bool getIsInSphereMode()
    {
        std::lock_guard<std::mutex> lk(mMtx);
        return mInSphereMode;
    }

    /**
     * @brief Thread-safe time getter
     *
     * @returns Current time
     */
    std::chrono::steady_clock::time_point getTime()
    {
        std::lock_guard<std::mutex> lk(mMtx);
        return mStartTime;
    }

    /**
     * @brief Thread-safe size getter
     *
     * @returns Current size (doesn't change so it doesn't really matter)
     */
    float getSize()
    {
        std::lock_guard<std::mutex> lk(mMtx);
        return mSize;
    }

    /**
     * @brief Mutex getter
     *
     * @returns mutex
     */
    std::mutex &getMutex()
    {
        return mMtx;
    }

    /**
     * @brief Thread-safe time setter
     *
     * @param New time
     */
    void setTime(const std::chrono::steady_clock::time_point &newTime)
    {
        std::lock_guard<std::mutex> lk(mMtx);
        mStartTime = newTime;
    }

    /**
     * @brief Swaps the velocity for 2 UAVs (used for elastic collisions)
     *
     * @param New velocity
     */
    void swapVelocity(ECE_UAV &other)
    {
        // caller should lock both mutexes in a consistent order to avoid deadlock
        std::swap(mVelocity, other.mVelocity);
    }

    /**
     * @brief The general physics update loop. Performs all calculations to update force, acceleration, velocity, and
     * position
     *
     * @param The time delta
     * @param How much time has elapsed since simulation start (required for 5 second wait period)
     */
    void update(float dt, float elapsedSinceStart);

  private:
    /**
     * @brief A random point generator that outputs a random point on the surface of the sphere. Useful in picking paths
     * for the UAVs
     */
    glm::vec3 randomPointOnSphere()
    {
        float theta = mDist(mGenerator) * 2.0f * 3.141592; // azimuth
        float phi = acos(2.0f * mDist(mGenerator) - 1.0f); // inclination
        float x = mSphereRadius * sin(phi) * cos(theta);
        float y = mSphereRadius * sin(phi) * sin(theta);
        float z = mSphereRadius * cos(phi);
        return glm::vec3(x, y, z);
    }

    /**
     * @brief Ensures the magnitude of any vector doesn't exceed a maximum (helpful for clamping max ascent velocity and
     * max force)
     *
     * @param Any vector
     * @param A max magnitude, above which the vector should be clamped to the max
     *
     * @returns Output vector
     */
    glm::vec3 fixMagnitude(const glm::vec3 &v, float maxLen)
    {
        float len2 = glm::dot(v, v);
        if (len2 <= maxLen * maxLen)
            return v;
        float inv = 1.0f / std::sqrt(len2);
        return v * (maxLen * inv);
    }

    // Physical parameters
    float mMass = 1.0f;      // kg
    float mMaxForce = 20.0f; // N, max force a UAV can exert
    float mGravity = 10.0f;  // N
    float mSize = 0.20f;     // m, UAV size cube

    // Position, velocity, and acceleration
    glm::vec3 mPosition = glm::vec3(0.0f);     // m
    glm::vec3 mVelocity = glm::vec3(0.0f);     // m/s
    glm::vec3 mAcceleration = glm::vec3(0.0f); // m/s^2

    // Thread things
    std::thread mWorker;
    std::atomic<bool> mIsRunning{false};
    std::mutex mMtx;

    // Simulation parameters
    glm::vec3 mSphereCenter = glm::vec3(0.0f, 50.0f, 0.0f);
    float mSphereRadius = 10.0f; // m
    float mWaitSeconds = 5.0f;   // s
    float mAscentSpeed = 2.0f;   // m/s
    float mMinSpeed = 2.0f;      // m/s. Minimum speed once reaching the sphere
    float mMaxSpeed = 10.0f;     // m/s. Maximum speed once reaching the sphere
    bool mInSphereMode = false;  // indicates whether the uav has reached the surface of the sphere

    // Random generators
    std::default_random_engine mGenerator;
    std::uniform_real_distribution<float> mDist{0.0f, 1.0f};

    std::chrono::steady_clock::time_point mStartTime;

    glm::vec3 mSphereTarget; // current point on the sphere
    float mSphereSpeed;      // speed to move along sphere
};

/**
 * @brief Provides kinematics updates every 10 ms
 *
 * @param A pointer to a ECE_UAV
 */
void threadFunction(ECE_UAV *uav);
