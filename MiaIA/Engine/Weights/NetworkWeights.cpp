#include "NetworkWeights.h"

#include <cmath>

namespace MiaIA::Engine
{
    bool NetworkWeights::SetWeight(
        Core::Network& network,
        std::uint64_t connectionId,
        double weight)
    {
        if (!std::isfinite(weight))
        {
            return false;
        }

        for (Core::Connection& connection : network.Connections)
        {
            if (connection.Id == connectionId)
            {
                connection.Weight = weight;
                return true;
            }
        }

        return false;
    }

    bool NetworkWeights::GetWeight(
        const Core::Network& network,
        std::uint64_t connectionId,
        double& weight)
    {
        for (const Core::Connection& connection : network.Connections)
        {
            if (connection.Id == connectionId)
            {
                weight = connection.Weight;
                return true;
            }
        }

        return false;
    }
}