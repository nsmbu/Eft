#ifndef EFT_FRAME_RATE_H_
#define EFT_FRAME_RATE_H_

#include <nw/eft/eft_typeDef.h>

#include <algorithm>
#include <cmath>

namespace nw { namespace eft {

inline f32 _nextAuthoredFrameDistance(f32 count)
{
    if (!std::isfinite(count))
        return 1.0f;

    const f32 whole = std::floor(count);
    const f32 fraction = count - whole;
    return fraction == 0.0f ? 1.0f : 1.0f - fraction;
}

template <typename Function>
inline void _forEachAuthoredFrame(f32 begin, f32 length, Function&& function)
{
    if (length <= 0.0f)
        return;

    const f32 end = begin + length;
    const s32 firstFrame = static_cast<s32>(std::ceil(begin));
    for (s32 frame = firstFrame; static_cast<f32>(frame) < end; ++frame)
        function(frame, static_cast<f32>(frame) - begin);
}

inline f32 _sectionOverlap(f32 begin, f32 length, f32 sectionBegin, f32 sectionEnd)
{
    const f32 overlapBegin = std::max(begin, sectionBegin);
    const f32 overlapEnd = std::min(begin + length, sectionEnd);
    return std::max(0.0f, overlapEnd - overlapBegin);
}

inline void _accumulateSection(f32& value, f32 rate, f32 activeTime)
{
    if (activeTime > 0.0f)
        value += rate * activeTime;
}

template <typename Function>
inline void _forEachCompletedAuthoredFrame(f32 begin, f32 length, Function&& function)
{
    if (length <= 0.0f)
        return;

    const f32 end = begin + length;
    s32 completedFrame = static_cast<s32>(std::floor(begin));
    f32 boundary = static_cast<f32>(completedFrame + 1);
    while (boundary <= end)
    {
        function(completedFrame);
        ++completedFrame;
        boundary = static_cast<f32>(completedFrame + 1);
    }
}

inline f32 _scaledMultiplier(f32 perFrameMultiplier, f32 authoredCount, f32 frameRate)
{
    if (perFrameMultiplier > 0.0f)
        return std::pow(perFrameMultiplier, frameRate);

    f32 result = 1.0f;
    _forEachCompletedAuthoredFrame(authoredCount, frameRate,
        [&](s32) { result *= perFrameMultiplier; });
    return result;
}

inline void _integrateAxis(f32& position, f32& velocity, f32 acceleration,
                           f32 damping, f32 dynamics, f32 authoredCount,
                           f32 frameRate)
{
    if (damping <= 0.0f)
    {
        f32 cursor = authoredCount;
        f32 remaining = frameRate;
        while (remaining > 0.0f)
        {
            const f32 boundary = std::floor(cursor) + 1.0f;
            const f32 distance = boundary - cursor;
            const f32 step = std::min(remaining, distance);

            position += velocity * dynamics * step;
            cursor += step;
            remaining -= step;

            if (step >= distance)
                velocity = damping * velocity + acceleration;
        }
        return;
    }

    if (frameRate == 1.0f)
    {
        position += velocity * dynamics;
        velocity = damping * velocity + acceleration;
        return;
    }

    const f32 scaledDamping = _scaledMultiplier(damping, authoredCount, frameRate);
    f32 velocitySum;
    if (std::fabs(damping - 1.0f) < 0.000001f)
    {
        velocitySum = velocity * frameRate
                    + acceleration * (0.5f * frameRate * (frameRate - 1.0f));
        velocity += acceleration * frameRate;
    }
    else
    {
        const f32 geometricSum = (scaledDamping - 1.0f) / (damping - 1.0f);
        velocitySum = velocity * geometricSum
                    + acceleration / (damping - 1.0f) * (geometricSum - frameRate);
        velocity = scaledDamping * velocity + acceleration * geometricSum;
    }

    position += velocitySum * dynamics;
}

} } // namespace nw::eft

#endif // EFT_FRAME_RATE_H_
