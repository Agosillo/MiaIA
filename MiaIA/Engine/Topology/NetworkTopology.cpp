#include "NetworkTopology.h"

namespace MiaIA::Engine
{
    Core::Neuron* NetworkTopology::FindNeuron(
        Core::Network& network,
        std::uint64_t neuronId)
    {
        for (Core::Layer& layer : network.Layers)
        {
            for (Core::Neuron& neuron : layer.Neurons)
            {
                if (neuron.Id == neuronId)
                {
                    return &neuron;
                }
            }
        }

        return nullptr;
    }

    const Core::Neuron* NetworkTopology::FindNeuron(
        const Core::Network& network,
        std::uint64_t neuronId)
    {
        for (const Core::Layer& layer : network.Layers)
        {
            for (const Core::Neuron& neuron : layer.Neurons)
            {
                if (neuron.Id == neuronId)
                {
                    return &neuron;
                }
            }
        }

        return nullptr;
    }
    Core::Layer* NetworkTopology::FindLayer(
        Core::Network& network,
        std::uint64_t layerId)
    {
        for (Core::Layer& layer : network.Layers)
        {
            if (layer.Id == layerId)
            {
                return &layer;
            }
        }

        return nullptr;
    }

    const Core::Layer* NetworkTopology::FindLayer(
        const Core::Network& network,
        std::uint64_t layerId)
    {
        for (const Core::Layer& layer : network.Layers)
        {
            if (layer.Id == layerId)
            {
                return &layer;
            }
        }

        return nullptr;
    }

    Core::Connection* NetworkTopology::FindConnection(
        Core::Network& network,
        std::uint64_t connectionId)
    {
        for (Core::Connection& connection : network.Connections)
        {
            if (connection.Id == connectionId)
            {
                return &connection;
            }
        }

        return nullptr;
    }

    const Core::Connection* NetworkTopology::FindConnection(
        const Core::Network& network,
        std::uint64_t connectionId)
    {
        for (const Core::Connection& connection : network.Connections)
        {
            if (connection.Id == connectionId)
            {
                return &connection;
            }
        }

        return nullptr;
    }

    Core::Layer* NetworkTopology::FindLayerForNeuron(
        Core::Network& network,
        std::uint64_t neuronId)
    {
        for (Core::Layer& layer : network.Layers)
        {
            for (Core::Neuron& neuron : layer.Neurons)
            {
                if (neuron.Id == neuronId)
                {
                    return &layer;
                }
            }
        }

        return nullptr;
    }

    const Core::Layer* NetworkTopology::FindLayerForNeuron(
        const Core::Network& network,
        std::uint64_t neuronId)
    {
        for (const Core::Layer& layer : network.Layers)
        {
            for (const Core::Neuron& neuron : layer.Neurons)
            {
                if (neuron.Id == neuronId)
                {
                    return &layer;
                }
            }
        }

        return nullptr;
    }
}