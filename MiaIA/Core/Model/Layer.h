#pragma once

#include "Neuron.h"

#include <cstdint>
#include <string>
#include <vector>

namespace MiaIA::Core
{
    enum class ActivationType
    {
        Sigmoid,
        ReLU,
        Tanh,
        Linear
    };

    struct Layer
    {
        std::uint64_t Id{};
        std::string Name;
        std::uint64_t Order{};
        std::vector<Neuron> Neurons;
        ActivationType Activation{ ActivationType::Sigmoid };
    };
}
