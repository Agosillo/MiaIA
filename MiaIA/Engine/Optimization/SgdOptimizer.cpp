#include "SgdOptimizer.h"

#include "../../Core/Model/Network.h"

#include <cmath>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace MiaIA::Engine
{
    bool SgdOptimizer::Apply(
        Core::Network& network,
        const Core::SampleGradientSnapshot& gradients,
        double learningRate,
        std::vector<Core::ConnectionUpdateSnapshot>& connectionUpdates,
        std::vector<Core::NeuronUpdateSnapshot>& neuronUpdates)
    {
        if (!std::isfinite(learningRate) || learningRate <= 0.0)
        {
            return false;
        }

        std::unordered_map<std::uint64_t, Core::Connection*> connectionsById;
        std::unordered_map<std::uint64_t, Core::Neuron*> neuronsById;
        std::unordered_map<std::uint64_t, std::uint64_t> neuronLayerOrders;

        connectionsById.reserve(network.Connections.size());

        for (Core::Connection& connection : network.Connections)
        {
            connectionsById[connection.Id] = &connection;
        }

        std::size_t neuronCount = 0;

        for (Core::Layer& layer : network.Layers)
        {
            neuronCount += layer.Neurons.size();

            for (Core::Neuron& neuron : layer.Neurons)
            {
                neuronsById[neuron.Id] = &neuron;
                neuronLayerOrders[neuron.Id] = layer.Order;
            }
        }

        if (gradients.Connections.size() != network.Connections.size() ||
            gradients.Neurons.size() != neuronCount)
        {
            return false;
        }

        std::unordered_set<std::uint64_t> connectionIds;
        std::unordered_set<std::uint64_t> neuronIds;
        std::vector<Core::ConnectionUpdateSnapshot> calculatedConnectionUpdates;
        std::vector<Core::NeuronUpdateSnapshot> calculatedNeuronUpdates;

        connectionIds.reserve(gradients.Connections.size());
        neuronIds.reserve(gradients.Neurons.size());
        calculatedConnectionUpdates.reserve(gradients.Connections.size());
        calculatedNeuronUpdates.reserve(gradients.Neurons.size());

        for (const Core::ConnectionGradientSnapshot& gradient :
            gradients.Connections)
        {
            const auto connectionIt = connectionsById.find(gradient.Id);

            if (connectionIt == connectionsById.end() ||
                !connectionIds.insert(gradient.Id).second)
            {
                return false;
            }

            const Core::Connection& connection = *connectionIt->second;

            if (connection.FromNeuron != gradient.FromNeuron ||
                connection.ToNeuron != gradient.ToNeuron ||
                !std::isfinite(connection.Weight) ||
                !std::isfinite(gradient.WeightGradient))
            {
                return false;
            }

            const double delta =
                -learningRate * gradient.WeightGradient;

            const double updatedWeight = connection.Weight + delta;

            if (!std::isfinite(delta) || !std::isfinite(updatedWeight))
            {
                return false;
            }

            Core::ConnectionUpdateSnapshot update;
            update.Id = connection.Id;
            update.PreviousWeight = connection.Weight;
            update.Gradient = gradient.WeightGradient;
            update.Delta = delta;
            update.UpdatedWeight = updatedWeight;

            calculatedConnectionUpdates.push_back(update);
        }

        for (const Core::NeuronGradientSnapshot& gradient :
            gradients.Neurons)
        {
            const auto neuronIt = neuronsById.find(gradient.Id);
            const auto layerOrderIt = neuronLayerOrders.find(gradient.Id);

            if (neuronIt == neuronsById.end() ||
                layerOrderIt == neuronLayerOrders.end() ||
                !neuronIds.insert(gradient.Id).second ||
                layerOrderIt->second != gradient.LayerOrder ||
                !std::isfinite(gradient.BiasGradient))
            {
                return false;
            }

            if (gradient.LayerOrder == 0)
            {
                if (gradient.BiasGradient != 0.0)
                {
                    return false;
                }

                continue;
            }

            const Core::Neuron& neuron = *neuronIt->second;

            if (!std::isfinite(neuron.Bias))
            {
                return false;
            }

            const double delta =
                -learningRate * gradient.BiasGradient;

            const double updatedBias = neuron.Bias + delta;

            if (!std::isfinite(delta) || !std::isfinite(updatedBias))
            {
                return false;
            }

            Core::NeuronUpdateSnapshot update;
            update.Id = neuron.Id;
            update.PreviousBias = neuron.Bias;
            update.Gradient = gradient.BiasGradient;
            update.Delta = delta;
            update.UpdatedBias = updatedBias;

            calculatedNeuronUpdates.push_back(update);
        }

        for (const Core::ConnectionUpdateSnapshot& update :
            calculatedConnectionUpdates)
        {
            connectionsById[update.Id]->Weight = update.UpdatedWeight;
        }

        for (const Core::NeuronUpdateSnapshot& update :
            calculatedNeuronUpdates)
        {
            neuronsById[update.Id]->Bias = update.UpdatedBias;
        }

        connectionUpdates = std::move(calculatedConnectionUpdates);
        neuronUpdates = std::move(calculatedNeuronUpdates);

        return true;
    }
}
