#include "NetworkInput.h"

#include <cmath>

namespace MiaIA::Engine
{
    bool NetworkInput::SetActivation(
        Core::Network& network,
        std::uint64_t neuronId,
        double activation)
    {
        if (!std::isfinite(activation))
        {
            return false;
        }

        for (Core::Layer& layer : network.Layers)
        {
            for (Core::Neuron& neuron : layer.Neurons)
            {
                if (neuron.Id == neuronId)
                {
                    neuron.Activation = activation;
                    return true;
                }
            }
        }

        return false;
    }
}