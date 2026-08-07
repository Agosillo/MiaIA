#pragma once

#include <cstddef>
#include <vector>

namespace MiaIA::Core
{
    struct SampleSnapshot
    {
        std::size_t Index{};
        std::vector<double> Inputs;
        std::vector<double> Targets;
    };
}
