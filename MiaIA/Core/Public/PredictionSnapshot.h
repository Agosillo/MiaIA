#pragma once

#include <vector>

namespace MiaIA::Core
{
    struct PredictionSnapshot
    {
        std::vector<double> Inputs;
        std::vector<double> Outputs;
    };
}
