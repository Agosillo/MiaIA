#include <algorithm>
#include "NetworkEditor.h"

namespace MiaIA::Engine
{
    void NetworkEditor::Clear(Core::Network& network)
    {
        network.Layers.clear();
        network.Connections.clear();
    }

    bool NetworkEditor::AddLayer(
        Core::Network& network,
        std::uint64_t id,
        const std::string& name,
        std::uint64_t order)
    {
        if (name.empty())
        {
            return false;
        }

        for (const Core::Layer& layer : network.Layers)
        {
            if (layer.Id == id)
            {
                return false;
            }

            if (layer.Order == order)
            {
                return false;
            }
        }

        Core::Layer layer;

        layer.Id = id;
        layer.Name = name;
        layer.Order = order;

        network.Layers.push_back(layer);

        return true;
    }

    bool NetworkEditor::AddNeuron(
        Core::Network& network,
        std::uint64_t layerId,
        std::uint64_t neuronId,
        double bias,
        double activation)
    {
        for (Core::Layer& layer : network.Layers)
        {
            if (layer.Id == layerId)
            {
                for (const Core::Neuron& neuron : layer.Neurons)
                {
                    if (neuron.Id == neuronId)
                    {
                        return false;
                    }
                }

                Core::Neuron neuron;

                neuron.Id = neuronId;
                neuron.Bias = bias;
                neuron.Activation = activation;

                layer.Neurons.push_back(neuron);

                return true;
            }
        }

        return false;
    }


    bool NetworkEditor::AddConnection(
        Core::Network& network,
        std::uint64_t id,
        std::uint64_t fromNeuron,
        std::uint64_t toNeuron,
        double weight)
    {
        for (const Core::Connection& connection : network.Connections)
        {
            if (connection.Id == id)
            {
                return false;
            }

            if (connection.FromNeuron == fromNeuron &&
                connection.ToNeuron == toNeuron)
            {
                return false;
            }
        }

        Core::Layer* fromLayer = nullptr;
        Core::Layer* toLayer = nullptr;

        for (Core::Layer& layer : network.Layers)
        {
            for (const Core::Neuron& neuron : layer.Neurons)
            {
                if (neuron.Id == fromNeuron)
                {
                    fromLayer = &layer;
                }

                if (neuron.Id == toNeuron)
                {
                    toLayer = &layer;
                }
            }
        }

        if (fromLayer == nullptr || toLayer == nullptr)
        {
            return false;
        }

        if (fromLayer->Order >= toLayer->Order)
        {
            return false;
        }

        Core::Connection connection;

        connection.Id = id;
        connection.FromNeuron = fromNeuron;
        connection.ToNeuron = toNeuron;
        connection.Weight = weight;

        network.Connections.push_back(connection);

        return true;
    }

    bool NetworkEditor::RemoveConnection(
        Core::Network& network,
        std::uint64_t id)
    {
        for (auto it = network.Connections.begin();
            it != network.Connections.end();
            ++it)
        {
            if (it->Id == id)
            {
                network.Connections.erase(it);
                return true;
            }
        }

        return false;
    }

    bool NetworkEditor::RemoveNeuron(
        Core::Network& network,
        std::uint64_t neuronId)
    {
        for (Core::Layer& layer : network.Layers)
        {
            for (auto neuronIt = layer.Neurons.begin();
                neuronIt != layer.Neurons.end();
                ++neuronIt)
            {
                if (neuronIt->Id == neuronId)
                {
                    network.Connections.erase(
                        std::remove_if(
                            network.Connections.begin(),
                            network.Connections.end(),
                            [neuronId](const Core::Connection& connection)
                            {
                                return connection.FromNeuron == neuronId ||
                                    connection.ToNeuron == neuronId;
                            }),
                        network.Connections.end());

                    layer.Neurons.erase(neuronIt);

                    return true;
                }
            }
        }

        return false;
    }

    bool NetworkEditor::RemoveLayer(
        Core::Network& network,
        std::uint64_t layerId)
    {
        for (auto layerIt = network.Layers.begin();
            layerIt != network.Layers.end();
            ++layerIt)
        {
            if (layerIt->Id == layerId)
            {
                const std::uint64_t removedOrder = layerIt->Order;

                for (const Core::Neuron& neuron : layerIt->Neurons)
                {
                    network.Connections.erase(
                        std::remove_if(
                            network.Connections.begin(),
                            network.Connections.end(),
                            [&neuron](const Core::Connection& connection)
                            {
                                return connection.FromNeuron == neuron.Id ||
                                    connection.ToNeuron == neuron.Id;
                            }),
                        network.Connections.end());
                }

                network.Layers.erase(layerIt);

                for (Core::Layer& layer : network.Layers)
                {
                    if (layer.Order > removedOrder)
                    {
                        --layer.Order;
                    }
                }

                return true;
            }
        }

        return false;
    }
}