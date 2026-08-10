#pragma once

namespace MiaIA::Core
{
    enum class TrainingRunStopReason
    {
        StepLimitReached,
        BreakpointHit,
        SessionCompleted,
        StepFailed
    };
}
