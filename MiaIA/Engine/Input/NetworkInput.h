#pragma once

#include "../../Core/Model/Network.h"

#include <vector>

namespace MiaIA::Engine
{
    class NetworkInput
    {
    public:

        static bool SetActivation(
            Core::Network& network,
            std::uint64_t neuronId,
            double activation);

        static bool SetValues(
            Core::Network& network,
            const std::vector<double>& values);
    };
}
