#pragma once

#include "Layer.h"
#include "Connection.h"
#include <vector>

namespace MiaIA::Core
{
    struct Network
    {
        std::vector<Layer> Layers;
        std::vector<Connection> Connections;
    };
}
