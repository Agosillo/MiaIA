#pragma once

#include <vector>

namespace MiaIA::Core
{
    struct Sample
    {
        std::vector<double> Inputs;
        std::vector<double> Targets;
    };
}
