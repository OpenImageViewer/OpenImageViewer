#pragma once
#include <StopWatch.h>
#include <MathUtil.h>


namespace OIV
{
    class AdaptiveMotion
    {
        LLUtils::StopWatch stopwatch;
        double fStep;
        double fAccelerationFactor;
        double fDeclerationFactor;
        double fTime = 0.0;

    public:
        AdaptiveMotion(double baseStep = 1.0, double acceleration = 1.0, double deceleration = 1.0)
        {
            fStep = baseStep;
            fAccelerationFactor = acceleration;
            fDeclerationFactor = deceleration;
        }

        double Add(double amount)
        {
            using namespace LLUtils;
            const double DeltaDirection = Math::Sign(amount);
            fTime -= stopwatch.GetElapsedTimeReal(StopWatch::Seconds) * DeltaDirection * fDeclerationFactor;
            stopwatch.Start();
            fTime = DeltaDirection > 0 ? std::max(fTime, 0.0) : std::min(fTime, 0.0);
            fTime += amount  * fStep;
            return GetVelocity(fTime);
        }

    private:
        double GetAcceleration(double time) const
        {
            return fAccelerationFactor;
        }

        double GetVelocity(double time) const
        {
            //Velocity = Acceleration * Time^2
            return  std::abs(GetAcceleration(time) * time * time) * LLUtils::Math::Sign(time);
        }
    };
}