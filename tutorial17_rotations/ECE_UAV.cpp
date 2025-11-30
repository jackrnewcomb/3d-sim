#include "ECE_UAV.hpp"

ECE_UAV::ECE_UAV(const glm::vec3 &startPos)
{
    mPosition = startPos;
}

void ECE_UAV::start()
{
    mIsRunning.store(true);
    mWorker = std::thread(threadFunction, this);
}

void ECE_UAV::stop()
{
    mIsRunning.store(false);
}

void ECE_UAV::join()
{
    if (mWorker.joinable())
    {
        mWorker.join();
    }
}

void ECE_UAV::update(float dt, float elapsedSinceStart)
{

    glm::vec3 totalForce(0.0f);

    glm::vec3 gravityForce = glm::vec3(0.0f, 0.0f, mGravity);

    if (elapsedSinceStart < mWaitSeconds)
    {
        // std::lock_guard<std::mutex> lk(mMtx);
        // mPosition.z = 0.0f;
        // mVelocity = glm::vec3(0.0f);
        // mAcceleration = glm::vec3(0.0f);
        return;
    }

    // compute vector to ascend target
    glm::vec3 toSphere = mSphereCenter - getPosition();
    float distFromSphere = glm::length(toSphere);

    if (distFromSphere <= mSphereRadius)
    {
        // close enough -> sphere roaming mode
        mInSphereMode = true;
    }

    if (!mInSphereMode)
    {
        // ASCEND phase: compute desired velocity towards ascendTarget with maxAscendSpeed
        glm::vec3 dir = (distFromSphere > 1e-6f) ? (toSphere / distFromSphere) : glm::vec3(0.0f, 0.0f, 1.0f);
        glm::vec3 v_des = dir * mAscentSpeed;
        // desired acceleration to reach v_des in one dt (simple PD-ish)
        glm::vec3 a_des = (v_des - getVelocity()) / std::max(dt, 1e-4f);

        // required force = m * a_des + gravity compensation
        glm::vec3 reqForce = mMass * a_des - gravityForce; // gravity is added separately (see below)
        // clamp to maxForce magnitude
        reqForce = clampMagnitude(reqForce, mMaxForce);

        totalForce += reqForce;
    }
    else
    {
    }

    glm::vec3 newAcc = totalForce / mMass;

    // update position using x = x0 + v0*t + 0.5*a*t^2
    glm::vec3 newPos = getPosition() + getVelocity() * dt + 0.5f * newAcc * dt * dt;

    // update velocity v = v0 + a*t
    glm::vec3 newVel = getVelocity() + newAcc * dt;

    // commit state under lock
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
    const std::chrono::milliseconds dt_ms(10);
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

        // sleep to hit ~10ms update rate
        std::this_thread::sleep_for(dt_ms);
    }
}
