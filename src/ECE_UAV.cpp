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

        return;
    }

    // compute vector to ascend target
    glm::vec3 toSphere = mSphereCenter - getPosition();
    float distFromSphere = glm::length(toSphere);

    if (distFromSphere <= mSphereRadius)
    {
        mInSphereMode = true;
    }

    if (!mInSphereMode)
    {
        // ASCEND phase: compute desired velocity towards ascendTarget with maxAscendSpeed
        glm::vec3 dir = (distFromSphere > 1e-6f) ? (toSphere / distFromSphere) : glm::vec3(0.0f, 0.0f, 1.0f);
        glm::vec3 desiredVelocity = dir * mAscentSpeed;
        // desired acceleration to reach v_des in one dt (simple PD-ish)
        glm::vec3 desiredAcceleration = (desiredVelocity - getVelocity()) / std::max(dt, 1e-4f);

        // required force = m * a_des + gravity compensation
        glm::vec3 reqForce = mMass * desiredAcceleration - gravityForce; // gravity is added separately (see below)
        // clamp to maxForce magnitude
        reqForce = clampMagnitude(reqForce, mMaxForce);

        totalForce += reqForce;
    }
    else
    {
        // radial vector
        glm::vec3 radial = glm::normalize(getPosition() - mSphereCenter);

        // random tangent perturbation
        glm::vec3 randomVec(mDist(mGenerator) - 0.5f, mDist(mGenerator) - 0.5f, mDist(mGenerator) - 0.5f);
        glm::vec3 tangent = randomVec - glm::dot(randomVec, radial) * radial;
        tangent = glm::normalize(tangent);

        // desired speed along tangent
        float speed = glm::clamp(glm::length(getVelocity()), mMinSpeed, mMaxSpeed);
        glm::vec3 desiredVel = glm::normalize(getVelocity() + 0.1f * tangent) * speed;

        totalForce += mMass * (desiredVel - getVelocity()) / std::max(dt, 1e-4f);
    }

    glm::vec3 newAcc = totalForce / mMass;
    glm::vec3 newPos = getPosition() + getVelocity() * dt + 0.5f * newAcc * dt * dt;
    glm::vec3 newVel = getVelocity() + newAcc * dt;

    // project onto sphere if in sphere mode
    if (mInSphereMode)
    {
        glm::vec3 radial = glm::normalize(newPos - mSphereCenter);
        newPos = mSphereCenter + radial * mSphereRadius;
        // make velocity tangent to sphere
        newVel -= glm::dot(newVel, radial) * radial;
        float speed = glm::clamp(glm::length(newVel), mMinSpeed, mMaxSpeed);
        newVel = glm::normalize(newVel) * speed;
    }

    auto velMag = glm::length(newVel);

    // Quick sanity check that the UAVs are moving at the right speed when traveling up to the sphere
    if (!mInSphereMode)
    {
        auto wiggleRoom = 0.15f;
        if (velMag > (mAscentSpeed + wiggleRoom))
        {
            std::cerr << "An erroneous ascent speed was detected: " << velMag << "\n";
        }
    }

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
