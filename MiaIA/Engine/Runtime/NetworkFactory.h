#pragma once

#include "../../Core/Model/Network.h"

namespace MiaIA::Engine
{
    class NetworkFactory
    {
    public:

        static Core::Network Create();

        static Core::Network CreateDense(
            int inputCount,
            int hiddenCount,
            int hiddenLayers,
            int outputCount);
    };
}
