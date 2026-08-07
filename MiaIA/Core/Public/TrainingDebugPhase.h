#pragma once

namespace MiaIA::Core
{
    enum class TrainingDebugPhase
    {
        Idle,
        BeforeForward,
        ForwardComplete,
        BackwardComplete,
        UpdateComplete,
        Verified,
        Committed
    };
}
