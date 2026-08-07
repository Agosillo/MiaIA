#pragma once

namespace MiaIA::Core
{
    enum class TrainingWorkerStopReason
    {
        None,
        PauseRequested,
        CancelRequested,
        StepFailed
    };
}
