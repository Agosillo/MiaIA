#pragma once

#include "../../Core/Public/PredictionSnapshot.h"

#include <vector>

namespace MiaIA::Core
{
    struct Network;
}

namespace MiaIA::Engine
{
    class PredictionEvaluator
    {
    public:
        static bool Predict(
            Core::Network& network,
            const std::vector<double>& inputs,
            Core::PredictionSnapshot& result);
    };
}
