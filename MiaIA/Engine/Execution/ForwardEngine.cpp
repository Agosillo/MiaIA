#pragma once

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

                neuron.Activation = Core::Activation::Sigmoid(sum);
            }
        }

        return true;
    }
}