#pragma once

#include "../../Core/Model/Network.h"

namespace MiaIA::Engine
{
    class NetworkParameters
    {
    public:

        static bool SetBias(
            Core::Network& network,
            std::uint64_t neuronId,
            double bias);
    };
}
