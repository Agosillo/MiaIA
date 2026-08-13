#pragma once

#include "../../Core/Model/Network.h"
#include "../../Core/Public/ConnectionInspectionSnapshot.h"
#include "../../Core/Public/ForwardTraceSnapshot.h"
#include "../../Core/Public/NetworkSnapshot.h"
#include "../../Core/Public/NeuronInspectionSnapshot.h"

#include <cstddef>
#include <cstdint>

namespace MiaIA::Engine
{
    class NetworkInspector
    {
    public:
        static Core::NetworkSnapshot Snapshot(
            const Core::Network& network);

        static Core::NetworkOverviewSnapshot Overview(
            const Core::Network& network);

        static bool TryGetNeuron(
            const Core::Network& network,
            std::uint64_t neuronId,
            Core::NeuronSnapshot& result);

        static bool TryGetConnection(
            const Core::Network& network,
            std::uint64_t connectionId,
            Core::ConnectionSnapshot& result);

        static bool TryInspectNeuron(
            const Core::Network& network,
            std::uint64_t neuronId,
            std::size_t maximumConnectionsPerDirection,
            Core::NeuronInspectionSnapshot& result);

        static bool TryGetNeuronRelationships(
            const Core::Network& network,
            std::uint64_t neuronId,
            const Core::NeuronRelationshipPageRequest& request,
            Core::NeuronRelationshipPageSnapshot& result);

        static bool TryInspectConnection(
            const Core::Network& network,
            std::uint64_t connectionId,
            Core::ConnectionInspectionSnapshot& result);

        static bool TraceForward(
            const Core::Network& network,
            const std::vector<double>& inputs,
            Core::ForwardTraceSnapshot& result);

        static bool TryGetForwardTraceContributions(
            const Core::Network& network,
            const std::vector<double>& inputs,
            std::uint64_t neuronId,
            const Core::ForwardTraceContributionPageRequest& request,
            Core::ForwardTraceContributionPageSnapshot& result);

        static bool TryGetLayer(
            const Core::Network& network,
            std::uint64_t layerId,
            Core::LayerSnapshot& result);
    };
}
