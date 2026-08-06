#include <vector>
#include <algorithm>
#include <unordered_set>
#include <unordered_map>
#include "NetworkValidator.h"

namespace MiaIA::Engine
{
    bool NetworkValidator::ValidateForForward(
        const Core::Network& network)
    {
        if (network.Layers.size() < 2)
        {
            return false;
        }

        if (network.Connections.empty())
        {
            return false;
        }

        std::unordered_set<std::uint64_t> neuronIds;

        for (const Core::Layer& layer : network.Layers)
        {
            for (const Core::Neuron& neuron : layer.Neurons)
            {
                neuronIds.insert(neuron.Id);
            }
        }

        std::unordered_map<std::uint64_t, std::uint64_t> neuronLayerOrders;

        for (const Core::Layer& layer : network.Layers)
        {
            for (const Core::Neuron& neuron : layer.Neurons)
            {
                neuronLayerOrders[neuron.Id] = layer.Order;
            }
        }

        for (const Core::Connection& connection : network.Connections)
        {
            if (neuronIds.find(connection.FromNeuron) == neuronIds.end())
            {
                return false;
            }

            if (neuronIds.find(connection.ToNeuron) == neuronIds.end())
            {
                return false;
            }

            if (neuronLayerOrders[connection.FromNeuron] >= neuronLayerOrders[connection.ToNeuron])
            {
                return false;
            }
        }

        std::unordered_set<std::uint64_t> targetNeuronIds;

        for (const Core::Connection& connection : network.Connections)
        {
            targetNeuronIds.insert(connection.ToNeuron);
        }

        for (const Core::Layer& layer : network.Layers)
        {
            if (layer.Order == 0)
            {
                continue;
            }

            for (const Core::Neuron& neuron : layer.Neurons)
            {
                if (targetNeuronIds.find(neuron.Id) == targetNeuronIds.end())
                {
                    return false;
                }
            }
        }

        std::vector<std::uint64_t> orders;

        for (const Core::Layer& layer : network.Layers)
        {
            orders.push_back(layer.Order);
        }

        std::sort(orders.begin(), orders.end());

        for (std::size_t index = 0; index < orders.size(); ++index)
        {
            if (orders[index] != index)
            {
                return false;
            }
        }
        return true;
    }
}