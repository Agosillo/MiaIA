#pragma once

#include "LossType.h"
#include "OptimizerType.h"
#include "TrainingStepSnapshot.h"

#include <cstddef>
#include <vector>

namespace MiaIA::Core
{
    struct TrainingEpochSnapshot
    {
        std::size_t SampleCount{};
        double LearningRate{};
        LossType Loss{ LossType::MeanSquaredError };
        OptimizerType Optimizer{
            OptimizerType::StochasticGradientDescent
        };
        double MeanLossBeforeUpdate{};
        double MeanLossAfterUpdate{};
        std::vector<TrainingStepSnapshot> Steps;
    };
}
