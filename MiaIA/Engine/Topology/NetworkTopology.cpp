#include "NetworkTopology.h"

namespace MiaIA::Engine
{

    NetworkTopology::NetworkTopology(
        Core::Network& network)
        :
        network(network)
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
}