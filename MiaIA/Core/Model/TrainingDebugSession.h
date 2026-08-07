#pragma once

#include "Network.h"
#include "../Public/TrainingDebugPhase.h"
#include "../Public/TrainingStepSnapshot.h"

#include <cstddef>

namespace MiaIA::Core
{
    struct TrainingDebugSession
    {
        TrainingDebugPhase Phase{ TrainingDebugPhase::Idle };
        std::size_t SampleIndex{};
        double LearningRate{};
        LossType Loss{ LossType::MeanSquaredError };
        OptimizerType Optimizer{
            OptimizerType::StochasticGradientDescent
        };
        Network CandidateNetwork;
        TrainingStepSnapshot Step;
    };
}
