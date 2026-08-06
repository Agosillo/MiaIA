#include <cmath>
#include "NetworkWeights.h"
#include "../Topology/NetworkTopology.h"
#include <iostream>


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

        NetworkTopology topology(network);

        Core::Connection* connection =
            topology.FindConnection(connectionId);

        if (connection == nullptr)
        {
            return false;
        }

        connection->Weight = weight;

        return true;
    }


    bool NetworkWeights::GetWeight(
        const Core::Network& network,
        std::uint64_t connectionId,
        double& weight)
    {
        const NetworkTopology topology(network);

        const Core::Connection* connection =
            topology.FindConnection(connectionId);

        if (connection == nullptr)
        {
            return false;
        }

        weight = connection->Weight;

        return true;
    }
}