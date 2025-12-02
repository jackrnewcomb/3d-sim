/*
Author: Jack Newcomb
Class: ECE6122
Last Date Modified: 11/30/2025

Description:

Implementation for the ECE_UAV class. Provides the physics update loop implementation, as well as the threadFunction
that workers will call to update the sim

*/

#include "ECE_UAV.hpp"

ECE_UAV::ECE_UAV(const glm::vec3 &startPos)
{
    // Initialize the position
    mPosition = startPos;
}

void ECE_UAV::start()
{
    // set isRunning and assign the worker to threadFunction
    mIsRunning.store(true);
    mWorker = std::thread(threadFunction, this);
}

void ECE_UAV::stop()
{
    // set isRunning to false
    mIsRunning.store(false);
}

void ECE_UAV::join()
{
    // join, if joinable
    if (mWorker.joinable())
    {
        mWorker.join();
    }
}

void ECE_UAV::update(float dt, float elapsedSinceStart)
{
    // Initialize the total force to 0, we'll append to it shortly
    glm::vec3 totalForce(0.0f);

    // get gravity vector
    glm::vec3 gravityForce = glm::vec3(0.0f, 0.0f, mGravity);

    // If we are on the ground for the first 5 seconds, no movement, just return
    if (elapsedSinceStart < mWaitSeconds)
    {
        return;
    }

    if (!mInSphereMode)
    {
        // Check if we've entered the sphere, and update mInSphereMode accordingly
        glm::vec3 toSphere = mSphereCenter - getPosition();
        float distFromSphere = glm::length(toSphere);
        if (distFromSphere <= mSphereRadius)
        {
            mInSphereMode = true;
        }

        // Get the desired velocity, force, and acceleration vectors to ascend toward the sphere
        glm::vec3 dir = (distFromSphere > 1e-6f) ? (toSphere / distFromSphere) : glm::vec3(0.0f, 0.0f, 1.0f);
        glm::vec3 desiredVelocity = dir * mAscentSpeed;
        glm::vec3 desiredAcceleration = (desiredVelocity - getVelocity()) / std::max(dt, 1e-4f);
        glm::vec3 reqForce = mMass * desiredAcceleration - gravityForce; // gravity is added separately

        // clamp to mMaxForce (can't exceed 20N)
        reqForce = fixMagnitude(reqForce, mMaxForce);

        // Apply the resultant force
        totalForce += reqForce;
    }
    else
    {
        // Now that we're in sphere mode, our basic strategy is to randomly select a point on the sphere to travel to,
        // and a velocity between 2-10 m/s. We will travel toward that point, with that velocity, until we reach it,
        // then repeat

        // Get the vector/distance magnitude to the randomly selected target point
        glm::vec3 toTarget = mSphereTarget - getPosition();
        float dist = glm::length(toTarget);

        // Determine the desired velocity
        glm::vec3 desiredVel = glm::normalize(toTarget) * mSphereSpeed;

        // determine the resultant acceleration and force
        glm::vec3 desiredAcc = (desiredVel - getVelocity()) / std::max(dt, 1e-4f);
        glm::vec3 reqForce = fixMagnitude(mMass * desiredAcc, mMaxForce);

        // Apply the resultant force
        totalForce += reqForce;

        // If we reached the point we were traveling to, get a new point and velocity
        if (dist < 0.1f)
        {
            mSphereTarget = mSphereCenter + randomPointOnSphere();
            mSphereSpeed = glm::linearRand(mMinSpeed, mMaxSpeed);
        }
    }

    // Get new acceleration, position, and velocity from the force we just calculated
    glm::vec3 newAcc = totalForce / mMass;
    glm::vec3 newPos = getPosition() + getVelocity() * dt + 0.5f * newAcc * dt * dt;
    glm::vec3 newVel = getVelocity() + newAcc * dt;

    // If we're in sphere mode, we need to make alterations to ensure the UAV remains on the surface of the sphere
    if (mInSphereMode)
    {
        // Ensure position is on the sphere surface
        glm::vec3 radial = glm::normalize(newPos - mSphereCenter);
        newPos = mSphereCenter + radial * mSphereRadius;

        // make velocity tangent to sphere
        newVel -= glm::dot(newVel, radial) * radial;
        float speed = glm::clamp(glm::length(newVel), mMinSpeed, mMaxSpeed);
        newVel = glm::normalize(newVel) * speed;
    }

    // commit new physical state
    {
        std::lock_guard<std::mutex> lk(mMtx);
        mPosition = newPos;
        mVelocity = newVel;
        mAcceleration = newAcc;
    }
}

void threadFunction(ECE_UAV *pUAV)
{
    // Worker will run with 10 ms updates
    using clock = std::chrono::steady_clock;
    const std::chrono::milliseconds dtMs(10);
    pUAV->setTime(clock::now());
    auto last = clock::now();

    while (pUAV->getIsRunning())
    {
        auto now = clock::now();
        std::chrono::duration<float> elapsed = now - pUAV->getTime();
        float timeSinceStart = elapsed.count();

        std::chrono::duration<float> frame_dt = now - last;
        float dt = frame_dt.count();
        if (dt <= 0.0f)
            dt = 0.01f; // fallback
        last = now;

        // call update
        pUAV->update(dt, timeSinceStart);

        // sleep to hit 10ms update rate
        std::this_thread::sleep_for(dtMs);
    }
}
