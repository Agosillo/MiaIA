#pragma once

#include "../../Core/Public/LossType.h"
#include "../../Core/Public/OptimizerType.h"
#include "../../Core/Public/TrainingEpochSnapshot.h"

namespace MiaIA::Core
{
    struct Dataset;
    struct Network;
}

namespace MiaIA::Engine
{
    class TrainingEpochExecutor
    {
    public:
        static bool Run(
            const Core::Dataset& dataset,
            Core::Network& network,
            double learningRate,
            Core::LossType lossType,
            Core::OptimizerType optimizerType,
            Core::TrainingEpochSnapshot& result);
    };
}
