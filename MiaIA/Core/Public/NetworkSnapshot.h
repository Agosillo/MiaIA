#pragma once

#include <cstdint>
#include <vector>

namespace MiaIA::Core
{
    struct NeuronSnapshot
    {
        std::uint64_t Id{};
        double Activation{};
    };

    struct LayerSnapshot
    {
        std::uint64_t Id{};
        std::vector<NeuronSnapshot> Neurons;
    };

    struct NetworkSnapshot
    {
        std::vector<LayerSnapshot> Layers;
    };
}
