#include "NetworkInspector.h"
#include "../Topology/NetworkTopology.h"
#include "../../Core/Execution/SnapshotBuilder.h"

#include <algorithm>
#include <utility>

namespace
{
    bool TryBuildNeuronContext(
        const MiaIA::Engine::NetworkTopology& topology,
        std::uint64_t neuronId,
        MiaIA::Core::NeuronContextSnapshot& result)
    {
        const MiaIA::Core::Neuron* neuron =
            topology.FindNeuron(neuronId);
        const MiaIA::Core::Layer* layer =
            topology.FindLayerForNeuron(neuronId);

        if (neuron == nullptr || layer == nullptr)
        {
            return false;
        }

        result = MiaIA::Core::NeuronContextSnapshot{
            {
                neuron->Id,
                neuron->Activation,
                neuron->Bias
            },
            layer->Id,
            layer->Name,
            layer->Order,
            layer->Activation
        };
        return true;
    }
}

namespace MiaIA::Engine
{
    Core::NetworkSnapshot NetworkInspector::Snapshot(
        const Core::Network& network)
    {
        return Core::SnapshotBuilder::Build(network);
    }

    Core::NetworkOverviewSnapshot NetworkInspector::Overview(
        const Core::Network& network)
    {
        Core::NetworkOverviewSnapshot result;
        result.Layers.reserve(network.Layers.size());
        result.ConnectionCount = network.Connections.size();

        for (const Core::Layer& layer : network.Layers)
        {
            result.NeuronCount += layer.Neurons.size();
            result.Layers.push_back(Core::LayerOverviewSnapshot{
                layer.Id,
                layer.Name,
                layer.Order,
                layer.Neurons.size(),
                layer.Activation
            });
        }

        return result;
    }

    bool NetworkInspector::TryGetNeuron(
        const Core::Network& network,
        std::uint64_t neuronId,
        Core::NeuronSnapshot& result)
    {
        const NetworkTopology topology(network);

        const Core::Neuron* neuron =
            topology.FindNeuron(neuronId);

        if (neuron == nullptr)
        {
            return false;
        }

        result = Core::NeuronSnapshot{
            neuron->Id,
            neuron->Activation,
            neuron->Bias
        };

        return true;
    }

    bool NetworkInspector::TryGetConnection(
        const Core::Network& network,
        std::uint64_t connectionId,
        Core::ConnectionSnapshot& result)
    {
        const NetworkTopology topology(network);

        const Core::Connection* connection =
            topology.FindConnection(connectionId);

        if (connection == nullptr)
        {
            return false;
        }

        result = Core::ConnectionSnapshot{
            connection->Id,
            connection->FromNeuron,
            connection->ToNeuron,
            connection->Weight
        };

        return true;
    }

    bool NetworkInspector::TryInspectNeuron(
        const Core::Network& network,
        std::uint64_t neuronId,
        std::size_t maximumConnectionsPerDirection,
        Core::NeuronInspectionSnapshot& result)
    {
        const NetworkTopology topology(network);
        Core::NeuronInspectionSnapshot snapshot;

        if (!TryBuildNeuronContext(
            topology,
            neuronId,
            snapshot.Context))
        {
            return false;
        }

        snapshot.IncomingConnections.reserve(
            std::min(
                maximumConnectionsPerDirection,
                network.Connections.size()));
        snapshot.OutgoingConnections.reserve(
            std::min(
                maximumConnectionsPerDirection,
                network.Connections.size()));

        for (const Core::Connection& connection : network.Connections)
        {
            if (connection.ToNeuron == neuronId)
            {
                ++snapshot.IncomingConnectionCount;

                if (snapshot.IncomingConnections.size() <
                    maximumConnectionsPerDirection)
                {
                    snapshot.IncomingConnections.push_back({
                        connection.Id,
                        connection.FromNeuron,
                        connection.ToNeuron,
                        connection.Weight
                    });
                }
            }

            if (connection.FromNeuron == neuronId)
            {
                ++snapshot.OutgoingConnectionCount;

                if (snapshot.OutgoingConnections.size() <
                    maximumConnectionsPerDirection)
                {
                    snapshot.OutgoingConnections.push_back({
                        connection.Id,
                        connection.FromNeuron,
                        connection.ToNeuron,
                        connection.Weight
                    });
                }
            }
        }

        result = std::move(snapshot);
        return true;
    }

    bool NetworkInspector::TryInspectConnection(
        const Core::Network& network,
        std::uint64_t connectionId,
        Core::ConnectionInspectionSnapshot& result)
    {
        const NetworkTopology topology(network);
        const Core::Connection* connection =
            topology.FindConnection(connectionId);

        if (connection == nullptr)
        {
            return false;
        }

        Core::ConnectionInspectionSnapshot snapshot;
        snapshot.Connection = {
            connection->Id,
            connection->FromNeuron,
            connection->ToNeuron,
            connection->Weight
        };

        if (!TryBuildNeuronContext(
                topology,
                connection->FromNeuron,
                snapshot.FromNeuron) ||
            !TryBuildNeuronContext(
                topology,
                connection->ToNeuron,
                snapshot.ToNeuron))
        {
            return false;
        }

        result = std::move(snapshot);
        return true;
    }

    bool NetworkInspector::TryGetLayer(
        const Core::Network& network,
        std::uint64_t layerId,
        Core::LayerSnapshot& result)
    {
        const NetworkTopology topology(network);

        const Core::Layer* layer =
            topology.FindLayer(layerId);

        if (layer == nullptr)
        {
            return false;
        }

        Core::LayerSnapshot snapshot;
        snapshot.Id = layer->Id;
        snapshot.Name = layer->Name;
        snapshot.Order = layer->Order;
        snapshot.Activation = layer->Activation;
        snapshot.Neurons.reserve(layer->Neurons.size());

        for (const Core::Neuron& neuron : layer->Neurons)
        {
            snapshot.Neurons.push_back(
                Core::NeuronSnapshot{
                    neuron.Id,
                    neuron.Activation,
                    neuron.Bias
                });
        }

        result = std::move(snapshot);

        return true;
    }
}
