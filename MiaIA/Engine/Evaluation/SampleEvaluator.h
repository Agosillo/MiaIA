#pragma once

#include "../../Core/Public/LossType.h"
#include "../../Core/Public/SampleEvaluationSnapshot.h"

#include <cstddef>

namespace MiaIA::Core
{
    struct Dataset;
    struct Network;
}

namespace MiaIA::Engine
{
    class SampleEvaluator
    {
    public:
        static bool Evaluate(
            const Core::Dataset& dataset,
            std::size_t index,
            Core::Network& network,
            Core::LossType type,
            Core::SampleEvaluationSnapshot& result);
    };
}
