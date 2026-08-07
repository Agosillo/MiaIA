#pragma once

#include "TrainingRunStopReason.h"
#include "TrainingStepSnapshot.h"

#include <cstddef>
#include <vector>

namespace MiaIA::Core
{
    struct TrainingRunSnapshot
    {
        std::size_t RequestedSteps{};
        std::size_t ExecutedSteps{};
        std::size_t StartEpoch{};
        std::size_t StartSampleIndex{};
        std::size_t EndEpoch{};
        std::size_t EndSampleIndex{};
        TrainingRunStopReason StopReason{
            TrainingRunStopReason::StepLimitReached
        };
        double MeanLossBeforeUpdate{};
        double MeanLossAfterUpdate{};
        std::vector<TrainingStepSnapshot> Steps;
    };
}
