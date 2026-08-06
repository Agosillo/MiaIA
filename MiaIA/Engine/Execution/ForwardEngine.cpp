#pragma once

#include <vector>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include "ForwardEngine.h"
#include "../../Core/Execution/Activation.h"
#include "../Validation/NetworkValidator.h"

namespace MiaIA::Engine
{
    bool ForwardEngine::Run(Core::Network& network)
    {
        if (!NetworkValidator::ValidateForForward(network))
        {
            return false;
        }

        std::unordered_map<std::uint64_t, Core::Neuron*> neuronsById;

        std::size_t neuronCount = 0;

        for (const Core::Layer& layer : network.Layers)
        {
            neuronCount += layer.Neurons.size();
        }

        neuronsById.reserve(neuronCount);

        for (Core::Layer& layer : network.Layers)
        {
            for (Core::Neuron& neuron : layer.Neurons)
            {
                neuronsById[neuron.Id] = &neuron;
            }
        }

        std::unordered_map<
            std::uint64_t,
            std::vector<const Core::Connection*>>
            connectionsByTarget;

        connectionsByTarget.reserve(neuronsById.size());

        for (const Core::Connection& connection : network.Connections)
        {
            connectionsByTarget[connection.ToNeuron].push_back(&connection);
        }

        std::sort(
            network.Layers.begin(),
            network.Layers.end(),
            [](const Core::Layer& a, const Core::Layer& b)
            {
                return a.Order < b.Order;
            });

        for (std::size_t layerIndex = 1;
            layerIndex < network.Layers.size();
            ++layerIndex)
        {
            Core::Layer& layer = network.Layers[layerIndex];

            for (Core::Neuron& neuron : layer.Neurons)
            {
                double sum = neuron.Bias;

                const auto targetIt = connectionsByTarget.find(neuron.Id);

                for (const Core::Connection* connection : targetIt->second)
                {
                    const auto sourceIt =
                        neuronsById.find(connection->FromNeuron);

                    sum +=
                        sourceIt->second->Activation *
                        connection->Weight;
                }

                switch (layer.Activation)
                {
                case Core::ActivationType::Sigmoid:
                    neuron.Activation =
                        Core::Activation::Sigmoid(sum);
                    break;

                case Core::ActivationType::ReLU:
                    neuron.Activation =
                        Core::Activation::ReLU(sum);
                    break;

                case Core::ActivationType::Tanh:
                    neuron.Activation =
                        Core::Activation::Tanh(sum);
                    break;

                case Core::ActivationType::Linear:
                    neuron.Activation =
                        Core::Activation::Linear(sum);
                    break;
                }
            }
        }

        return true;
    }
}