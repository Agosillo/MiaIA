#pragma once

#include "Layer.h"

#include <vector>

namespace MiaIA::Core
{
    struct Network
    {
        std::vector<Layer> Layers;
    };
}
