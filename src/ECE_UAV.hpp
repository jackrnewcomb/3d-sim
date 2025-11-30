#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <glm/glm.hpp>
#include <iostream>
#include <mutex>
#include <random>
#include <thread>

class ECE_UAV
{
  public:
    ECE_UAV(const glm::vec3 &startPos);

    // Start the internal thread (calls external threadFunction per spec)
    void start();

    // Request stop and join
    void stop();
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
};

void threadFunction(ECE_UAV *uav);
