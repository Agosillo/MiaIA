#pragma once

#include "../../Core/Model/Network.h"

namespace MiaIA::Engine
{
    class NetworkFactory
    {
    public:

        static Core::Network Create();

        static bool CreateDense(
            Core::Network& network,
            int inputCount,
            int hiddenCount,
            int hiddenLayers,
            int outputCount);
    };
}
