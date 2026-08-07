#pragma once

#include "Sample.h"

#include <cstddef>
#include <string>
#include <vector>

namespace MiaIA::Core
{
    struct Dataset
    {
        std::string Name;
        std::string Source;
        std::size_t InputCount{};
        std::size_t TargetCount{};
        std::vector<Sample> Samples;
    };
}
