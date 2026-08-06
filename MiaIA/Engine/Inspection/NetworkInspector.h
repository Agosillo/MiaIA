#pragma once

#include "../../Core/Model/Network.h"
#include "../../Core/Public/NetworkSnapshot.h"

#include <cstdint>

namespace MiaIA::Engine
{
    class NetworkInspector
    {
    public:
        static Core::NetworkSnapshot Snapshot(
            const Core::Network& network);

        static bool TryGetNeuron(
            const Core::Network& network,
            std::uint64_t neuronId,
            Core::NeuronSnapshot& result);

        static bool TryGetConnection(
            const Core::Network& network,
            std::uint64_t connectionId,
            Core::ConnectionSnapshot& result);

        static bool TryGetLayer(
            const Core::Network& network,
            std::uint64_t layerId,
            Core::LayerSnapshot& result);
    };
}
