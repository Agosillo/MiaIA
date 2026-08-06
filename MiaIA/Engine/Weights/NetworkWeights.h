#pragma once

#include "../../Core/Model/Network.h"

namespace MiaIA::Engine
{
    class NetworkWeights
    {
    public:

        static bool SetWeight(
            Core::Network& network,
            std::uint64_t connectionId,
            double weight);

        static bool GetWeight(
            const Core::Network& network,
            std::uint64_t connectionId,
            double& weight);
    };
}
