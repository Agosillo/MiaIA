#pragma once

#include "../../Core/Public/LossType.h"
#include "../../Core/Public/SampleGradientSnapshot.h"

#include <cstddef>

namespace MiaIA::Core
{
    struct Dataset;
    struct Network;
}

namespace MiaIA::Engine
{
    class SampleGradientEvaluator
    {
    public:
        static bool Evaluate(
            const Core::Dataset& dataset,
            std::size_t index,
            Core::Network& network,
            Core::LossType type,
            Core::SampleGradientSnapshot& result);
    };
}
