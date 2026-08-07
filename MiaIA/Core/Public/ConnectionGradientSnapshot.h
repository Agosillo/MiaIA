#pragma once

#include <cstdint>

namespace MiaIA::Core
{
    struct ConnectionGradientSnapshot
    {
        std::uint64_t Id{};
        std::uint64_t FromNeuron{};
        std::uint64_t ToNeuron{};
        double WeightGradient{};
    };
}
