#pragma once

#include "../../Core/Model/Network.h"

#include <string>

namespace MiaIA::Engine
{
    class NetworkEditor
    {
    public:

        static void Clear(Core::Network& network);

        static bool AddLayer(
            Core::Network& network,
            std::uint64_t id,
            const std::string& name,
            std::uint64_t order);

        static bool AddNeuron(
            Core::Network& network,
            std::uint64_t layerId,
            std::uint64_t neuronId,
            double bias,
            double activation);

        static bool AddConnection(
            Core::Network& network,
            std::uint64_t id,
            std::uint64_t fromNeuron,
            std::uint64_t toNeuron,
            double weight);

        static bool RemoveConnection(
            Core::Network& network,
            std::uint64_t id);

        static bool RemoveNeuron(
            Core::Network& network,
            std::uint64_t neuronId);

        static bool RemoveLayer(
            Core::Network& network,
            std::uint64_t layerId);
    };
}
