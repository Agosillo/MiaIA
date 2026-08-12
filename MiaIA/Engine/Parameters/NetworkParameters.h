#pragma once

#include "../../Core/Model/Network.h"
#include "../../Core/Public/NetworkParameterUpdate.h"

namespace MiaIA::Engine
{
    class NetworkParameters
    {
    public:

        static bool SetBias(
            Core::Network& network,
            std::uint64_t neuronId,
            double bias);

        static bool SetLayerActivation(
            Core::Network& network,
            std::uint64_t layerId,
            Core::ActivationType activation);

        static bool ApplyUpdate(
            Core::Network& network,
            const Core::NetworkParameterUpdate& update,
            Core::NetworkParameterUpdateSnapshot& result);
    };
}
