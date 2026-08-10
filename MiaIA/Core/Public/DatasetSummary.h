#pragma once

#include <cstddef>
#include <string>

namespace MiaIA::Core
{
    struct DatasetSummary
    {
        std::string Name;
        std::string Source;
        std::size_t SampleCount{};
        std::size_t InputCount{};
        std::size_t TargetCount{};
        bool HasHeader{ true };
    };
}
