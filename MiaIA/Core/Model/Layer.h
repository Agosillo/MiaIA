#pragma once

#include "Neuron.h"

#include <cstdint>
#include <string>
#include <vector>

namespace MiaIA::Core
{
    struct Layer
    {
        std::uint64_t Id{};
        std::string Name;
        std::vector<Neuron> Neurons;
    };
}
