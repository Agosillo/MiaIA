#include "NetworkTopology.h"

namespace MiaIA::Engine
{

    NetworkTopology::NetworkTopology(
        Core::Network& network)
        :
        network(&network),
        constNetwork(nullptr)
    {
        for (Core::Layer& layer : network.Layers)
        {
            layers[layer.Id] = &layer;

            for (Core::Neuron& neuron : layer.Neurons)
            {
                neurons[neuron.Id] = &neuron;
            }
        }

        for (Core::Connection& connection : network.Connections)
        {
            connections[connection.Id] = &connection;
        }
    }

    NetworkTopology::NetworkTopology(
        const Core::Network& network)
        :
        network(nullptr),
        constNetwork(&network)
    {
        for (const Core::Layer& layer : network.Layers)
        {
            constLayers[layer.Id] = &layer;

            for (const Core::Neuron& neuron : layer.Neurons)
            {
                constNeurons[neuron.Id] = &neuron;
            }
        }

        for (const Core::Connection& connection : network.Connections)
        {
            constConnections[connection.Id] = &connection;
        }
    }

    Core::Neuron* NetworkTopology::FindNeuron(
        std::uint64_t neuronId)
    {
        const auto it = neurons.find(neuronId);

        if (it == neurons.end())
        {
            return nullptr;
        }

        return it->second;
    }

    const Core::Neuron* NetworkTopology::FindNeuron(
        std::uint64_t neuronId) const
    {
        const auto it = constNeurons.find(neuronId);

        if (it == constNeurons.end())
        {
            return nullptr;
        }

        return it->second;
    }


    Core::Layer* NetworkTopology::FindLayer(
        std::uint64_t layerId)
    {
        const auto it = layers.find(layerId);

        if (it == layers.end())
        {
            return nullptr;
        }

        return it->second;
    }

    const Core::Layer* NetworkTopology::FindLayer(
        std::uint64_t layerId) const
    {
        const auto it = constLayers.find(layerId);

        if (it == constLayers.end())
        {
            return nullptr;
        }

        return it->second;
    }

    Core::Connection* NetworkTopology::FindConnection(
        std::uint64_t connectionId)
    {
        const auto it = connections.find(connectionId);

        if (it == connections.end())
        {
            return nullptr;
        }

        return it->second;
    }
    
    const Core::Connection* NetworkTopology::FindConnection(
        std::uint64_t connectionId) const
    {
        const auto it =
            constConnections.find(connectionId);

        if (it == constConnections.end())
        {

            return nullptr;
        }

        return it->second;
    }
    
    Core::Layer* NetworkTopology::FindLayerForNeuron(
        std::uint64_t neuronId)
    {
        for (auto& pair : layers)
        {
            Core::Layer* layer = pair.second;

            for (Core::Neuron& neuron : layer->Neurons)
            {
                if (neuron.Id == neuronId)
                {
                    return layer;
                }
            }
        }

        return nullptr;
    }

    const Core::Layer* NetworkTopology::FindLayerForNeuron(
        std::uint64_t neuronId) const
    {
        for (const auto& pair : constLayers)
        {
            const Core::Layer* layer = pair.second;

            for (const Core::Neuron& neuron : layer->Neurons)
            {
                if (neuron.Id == neuronId)
                {
                    return layer;
                }
            }
        }

        return nullptr;
    }
}
