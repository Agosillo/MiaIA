#pragma once

#include "../../Core/Public/LossType.h"
#include "../../Core/Public/OptimizerType.h"
#include "../../Core/Public/TrainingStepSnapshot.h"

#include <cstddef>

namespace MiaIA::Core
{
    struct Dataset;
    struct Network;
}

namespace MiaIA::Engine
{
    class TrainingStepExecutor
    {
    public:
        static bool Run(
            const Core::Dataset& dataset,
            std::size_t sampleIndex,
            Core::Network& network,
            double learningRate,
            Core::LossType lossType,
            Core::OptimizerType optimizerType,
            Core::TrainingStepSnapshot& result);
    };
}
