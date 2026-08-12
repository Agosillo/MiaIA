#pragma once

#include "NetworkSnapshot.h"

#include <cstdint>
#include <string>

namespace MiaIA::Core
{
    struct NeuronContextSnapshot
    {
        NeuronSnapshot Neuron;
        std::uint64_t LayerId{};
        std::string LayerName;
        std::uint64_t LayerOrder{};
        ActivationType LayerActivation{ ActivationType::Sigmoid };
    };
}
