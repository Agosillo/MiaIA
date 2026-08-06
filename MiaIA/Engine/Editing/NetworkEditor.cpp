#include <algorithm>
#include <cmath>
#include "NetworkEditor.h"
#include "../Topology/NetworkTopology.h"

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
        NetworkTopology topology(network);

        Core::Layer* layer =
            topology.FindLayer(layerId);

        if (layer == nullptr)
        {
            return false;
        }

        for (const Core::Neuron& neuron : layer->Neurons)
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

        layer->Neurons.push_back(neuron);

        return true;
    }


    bool NetworkEditor::AddConnection(
        Core::Network& network,
        std::uint64_t id,
        std::uint64_t fromNeuron,
        std::uint64_t toNeuron,
        double weight)
    {

        if (!std::isfinite(weight))
        {
            return false;
        }

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

        NetworkTopology topology(network);


        Core::Layer* fromLayer = topology.FindLayerForNeuron(fromNeuron);

        Core::Layer* toLayer = topology.FindLayerForNeuron(toNeuron);

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

        NetworkTopology topology(network);

        Core::Layer* layer =
            topology.FindLayerForNeuron(
                neuronId);

        if (layer == nullptr)
        {
            return false;
        }

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


        for (auto neuronIt = layer->Neurons.begin();
            neuronIt != layer->Neurons.end();
            ++neuronIt)
        {
            if (neuronIt->Id == neuronId)
            {
                layer->Neurons.erase(neuronIt);
                return true;
            }
        }

        return false;
    }

    bool NetworkEditor::RemoveLayer(
        Core::Network& network,
        std::uint64_t layerId)
    {
        NetworkTopology topology(network);

        Core::Layer* layer =
            topology.FindLayer(layerId);

        if (layer == nullptr)
        {
            return false;
        }

        std::vector<std::uint64_t> neuronIds;

        neuronIds.reserve(layer->Neurons.size());

        for (const Core::Neuron& neuron : layer->Neurons)
        {
            neuronIds.push_back(neuron.Id);
        }

        network.Connections.erase(
            std::remove_if(
                network.Connections.begin(),
                network.Connections.end(),
                [&neuronIds](const Core::Connection& connection)
                {
                    return std::find(
                        neuronIds.begin(),
                        neuronIds.end(),
                        connection.FromNeuron) != neuronIds.end()
                        ||
                        std::find(
                            neuronIds.begin(),
                            neuronIds.end(),
                            connection.ToNeuron) != neuronIds.end();
                }),
            network.Connections.end());


        const std::uint64_t removedOrder = layer->Order;


        network.Layers.erase(
            std::remove_if(
                network.Layers.begin(),
                network.Layers.end(),
                [layerId](const Core::Layer& item)
                {
                    return item.Id == layerId;
                }),
            network.Layers.end());


        for (Core::Layer& currentLayer : network.Layers)
        {
            if (currentLayer.Order > removedOrder)
            {
                --currentLayer.Order;
            }
        }

        return true;
    }
}