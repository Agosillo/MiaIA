#pragma once

#include <cstdint>

namespace MiaIA::Core
{
    struct Neuron
    {
        std::uint64_t Id{};
        double Bias{};
        double Activation{};
    };
}
