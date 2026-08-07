#pragma once

namespace MiaIA::Core
{
    enum class TrainingRunStopReason
    {
        StepLimitReached,
        SessionCompleted,
        StepFailed
    };
}
