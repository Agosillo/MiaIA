#pragma once

namespace MiaIA::Core
{
    enum class TrainingSessionStatus
    {
        Idle,
        Active,
        Running,
        Completed,
        Cancelled
    };
}
