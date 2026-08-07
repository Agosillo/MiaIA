#pragma once

#include <cstdint>

namespace MiaIA::Core
{
    struct NeuronGradientSnapshot
    {
        std::uint64_t Id{};
        std::uint64_t LayerOrder{};
        double ActivationGradient{};
        double PreActivationGradient{};
        double BiasGradient{};
    };
}
