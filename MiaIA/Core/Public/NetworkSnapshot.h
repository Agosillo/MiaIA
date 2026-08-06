#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "ConnectionSnapshot.h"

namespace MiaIA::Core
{
    struct NeuronSnapshot
    {
        std::uint64_t Id{};
        double Activation{};
        double Bias{};
    };

    struct LayerSnapshot
    {
        std::uint64_t Id{};
        std::string Name;
        std::vector<NeuronSnapshot> Neurons;
    };

    struct NetworkSnapshot
    {
        std::vector<LayerSnapshot> Layers;
        std::vector<ConnectionSnapshot> Connections;
    };

}
