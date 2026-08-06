#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "ConnectionSnapshot.h"
#include "../Model/Layer.h"

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
        std::uint64_t Order{};
        std::vector<NeuronSnapshot> Neurons;
        ActivationType Activation{ ActivationType::Sigmoid };
    };

    struct NetworkSnapshot
    {
        std::vector<LayerSnapshot> Layers;
        std::vector<ConnectionSnapshot> Connections;
    };

}
