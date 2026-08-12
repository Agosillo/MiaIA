#pragma once

#include "../../Core/Model/Network.h"
#include "../../Core/Public/DenseNetworkConfiguration.h"

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

        static bool CreateDense(
            Core::Network& network,
            int inputCount,
            int hiddenCount,
            int hiddenLayers,
            int outputCount,
            const Core::DenseNetworkConfiguration& configuration);
    };
}
