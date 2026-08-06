#pragma once

#include <cstdint>

namespace MiaIA::Core
{
    struct ConnectionSnapshot
    {
        std::uint64_t Id{};
        std::uint64_t FromNeuron{};
        std::uint64_t ToNeuron{};
        double Weight{};
    };
}
