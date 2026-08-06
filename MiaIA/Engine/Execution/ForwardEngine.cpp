#pragma once

#include <algorithm>
#include "ForwardEngine.h"
#include "../../Core/Execution/Activation.h"

namespace MiaIA::Engine
{
    bool ForwardEngine::Run(Core::Network& network)
    {
        if (network.Layers.size() < 2)
        {
            return false;
        }

        if (network.Connections.empty())
        {
            return false;
        }

        std::sort(
            network.Layers.begin(),
            network.Layers.end(),
            [](const Core::Layer& a, const Core::Layer& b)
            {
                return a.Order < b.Order;
            });

        for (std::size_t index = 0; index < network.Layers.size(); ++index)
        {
            if (network.Layers[index].Order != index)
            {
                return false;
            }
        }

        for (std::size_t layerIndex = 1;
            layerIndex < network.Layers.size();
            ++layerIndex)
        {
            Core::Layer& layer = network.Layers[layerIndex];

            for (Core::Neuron& neuron : layer.Neurons)
            {
                double sum = neuron.Bias;

                for (const Core::Connection& connection : network.Connections)
                {
                    if (connection.ToNeuron != neuron.Id)
                    {
                        continue;
                    }

                    for (const Core::Layer& sourceLayer : network.Layers)
                    {
                        for (const Core::Neuron& sourceNeuron : sourceLayer.Neurons)
                        {
                            if (sourceNeuron.Id == connection.FromNeuron)
                            {
                                sum += sourceNeuron.Activation * connection.Weight;
                            }
                        }
                    }
                }

                switch (layer.Activation)
                {
                case Core::ActivationType::Sigmoid:
                    neuron.Activation = Core::Activation::Sigmoid(sum);
                    break;

                case Core::ActivationType::ReLU:
                    neuron.Activation = Core::Activation::ReLU(sum);
                    break;

                case Core::ActivationType::Tanh:
                    neuron.Activation = Core::Activation::Tanh(sum);
                    break;

                case Core::ActivationType::Linear:
                    neuron.Activation = Core::Activation::Linear(sum);
                    break;
                }
            }
        }

        return true;
    }
}