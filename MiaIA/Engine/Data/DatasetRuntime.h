#pragma once

#include <cstddef>

namespace MiaIA::Core
{
    struct Dataset;
    struct Network;
}

namespace MiaIA::Engine
{
    class DatasetRuntime
    {
    public:
        static bool ApplySample(
            const Core::Dataset& dataset,
            std::size_t index,
            Core::Network& network);
    };
}
