#pragma once

#include "../../Core/Public/DatasetEvaluationSnapshot.h"
#include "../../Core/Public/LossType.h"

namespace MiaIA::Core
{
    struct Dataset;
    struct Network;
}

namespace MiaIA::Engine
{
    class DatasetEvaluator
    {
    public:
        static bool Evaluate(
            const Core::Dataset& dataset,
            const Core::Network& network,
            Core::LossType type,
            Core::DatasetEvaluationSnapshot& result);
    };
}
