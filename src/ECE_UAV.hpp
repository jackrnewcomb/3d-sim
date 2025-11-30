/*
Author: Jack Newcomb
Class: ECE6122
Last Date Modified: 11/30/2025

Description:

Header for the ECE_UAV class. Provides thread-safe getters and setters for relevant members, the requested start()
method, and a physics update() method

*/

#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/random.hpp>
#include <iostream>
#include <mutex>
#include <random>
#include <thread>

class ECE_UAV
{
  public:
    /**
     * @brief The ECE_UAV constructor. Takes in a starting position
     *
     * @param glm::vec3 initial position
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
     * @brief Starts the UAV execution by assigning the std::thread worker to threadFunction
     */
    void join();

    glm::vec3 getPosition()
    {
        std::lock_guard<std::mutex> lk(mMtx);
        return mPosition;
    }
    glm::vec3 getVelocity()
    {
        std::lock_guard<std::mutex> lk(mMtx);
        return mVelocity;
    }
    bool getIsRunning()
    {
        std::lock_guard<std::mutex> lk(mMtx);
        return mIsRunning;
    }
    bool getIsInSphereMode()
    {
        std::lock_guard<std::mutex> lk(mMtx);
        return mInSphereMode;
    }

    std::chrono::steady_clock::time_point getTime()
    {
        std::lock_guard<std::mutex> lk(mMtx);
        return mStartTime;
    }

    float getSize()
    {
        std::lock_guard<std::mutex> lk(mMtx);
        return mSize;
    }

    std::mutex &getMutex()
    {
        return mMtx;
    }

    void setVelocity(const glm::vec3 &v)
    {
        std::lock_guard<std::mutex> lk(mMtx);
        mVelocity = v;
    }

    void setTime(const std::chrono::steady_clock::time_point &newTime)
    {
        std::lock_guard<std::mutex> lk(mMtx);
        mStartTime = newTime;
    }

    void swapVelocity(ECE_UAV &other)
    {
        // caller should lock both mutexes in a consistent order to avoid deadlock
        std::swap(mVelocity, other.mVelocity);
    }

    // helper: clamp vector length
    glm::vec3 clampMagnitude(const glm::vec3 &v, float maxLen)
    {
        float len2 = glm::dot(v, v);
        if (len2 <= maxLen * maxLen)
            return v;
        float inv = 1.0f / std::sqrt(len2);
        return v * (maxLen * inv);
    }

    void update(float dt, float elapsedSinceStart);

  private:
    float mMass = 1.0f;      // kg
    float mMaxForce = 20.0f; // N, max force a UAV can exert
    float mGravity = 10.0f;  // N
    float mSize = 0.20f;     // m, UAV size cube

    glm::vec3 mPosition = glm::vec3(0.0f);
    glm::vec3 mVelocity = glm::vec3(0.0f);
    glm::vec3 mAcceleration = glm::vec3(0.0f);

    std::thread mWorker;
    std::atomic<bool> mIsRunning{false};
    std::mutex mMtx;

    glm::vec3 mSphereCenter = glm::vec3(0.0f, 50.0f, 0.0f);
    float mSphereRadius = 10.0f; // m
    float mWaitSeconds = 5.0f;   // s
    float mAscentSpeed = 2.0f;   // m/s
    float mMinSpeed = 2.0f;      // m/s
    float mMaxSpeed = 10.0f;     // m/s
    bool mInSphereMode = false;  // indicates whether the uav has reached the surface of the sphere

    std::default_random_engine mGenerator;
    std::uniform_real_distribution<float> mDist{0.0f, 1.0f};

    std::chrono::steady_clock::time_point mStartTime;

    glm::vec3 randomPointOnSphere(float radius, std::mt19937 &gen, std::uniform_real_distribution<float> &dist)
    {
        float theta = dist(gen) * 2.0f * 3.141592; // azimuth
        float phi = acos(2.0f * dist(gen) - 1.0f); // inclination
        float x = radius * sin(phi) * cos(theta);
        float y = radius * sin(phi) * sin(theta);
        float z = radius * cos(phi);
        return glm::vec3(x, y, z);
    }

    glm::vec3 mSphereTarget; // current point on the sphere
    float mSphereSpeed{0.0}; // speed to move along sphere
};

void threadFunction(ECE_UAV *uav);
