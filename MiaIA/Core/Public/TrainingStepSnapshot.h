#pragma once

#include "ConnectionUpdateSnapshot.h"
#include "NeuronUpdateSnapshot.h"
#include "OptimizerType.h"
#include "SampleEvaluationSnapshot.h"
#include "SampleGradientSnapshot.h"

#include <cstddef>
#include <vector>

namespace MiaIA::Core
{
    struct TrainingStepSnapshot
    {
        std::size_t SampleIndex{};
        double LearningRate{};
        OptimizerType Optimizer{
            OptimizerType::StochasticGradientDescent
        };
        SampleGradientSnapshot Before;
        std::vector<ConnectionUpdateSnapshot> ConnectionUpdates;
        std::vector<NeuronUpdateSnapshot> NeuronUpdates;
        SampleEvaluationSnapshot After;
    };
}
