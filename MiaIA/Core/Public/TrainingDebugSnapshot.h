#pragma once

#include "NetworkSnapshot.h"
#include "TrainingDebugPhase.h"
#include "TrainingStepSnapshot.h"

#include <cstddef>

namespace MiaIA::Core
{
    struct TrainingDebugSnapshot
    {
        TrainingDebugPhase Phase{ TrainingDebugPhase::Idle };
        std::size_t SampleIndex{};
        double LearningRate{};
        LossType Loss{ LossType::MeanSquaredError };
        OptimizerType Optimizer{
            OptimizerType::StochasticGradientDescent
        };
        NetworkSnapshot CandidateNetwork;
        TrainingStepSnapshot Step;
    };
}
