#pragma once

#include "../Public/LossType.h"
#include "../Public/OptimizerType.h"
#include "../Public/TrainingSessionStatus.h"
#include "../Public/TrainingStepSnapshot.h"

#include <cstddef>
#include <vector>

namespace MiaIA::Core
{
    struct TrainingSession
    {
        TrainingSessionStatus Status{ TrainingSessionStatus::Idle };
        std::size_t EpochCount{};
        std::size_t CurrentEpoch{};
        std::size_t NextSampleIndex{};
        std::size_t SampleCount{};
        double LearningRate{};
        LossType Loss{ LossType::MeanSquaredError };
        OptimizerType Optimizer{
            OptimizerType::StochasticGradientDescent
        };
        std::vector<TrainingStepSnapshot> Steps;
    };
}
