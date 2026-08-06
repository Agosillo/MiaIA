#pragma once

#include "../../Core/Model/Network.h"

namespace MiaIA::Engine
{
    class NetworkInput
    {
    public:

        static bool SetActivation(
            Core::Network& network,
            std::uint64_t neuronId,
            double activation);
    };
}
