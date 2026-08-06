#include "NetworkParameters.h"

#include <cmath>

namespace MiaIA::Engine
{
    bool NetworkParameters::SetBias(
        Core::Network& network,
        std::uint64_t neuronId,
        double bias)
    {
        if (!std::isfinite(bias))
        {
            return false;
        }

        for (Core::Layer& layer : network.Layers)
        {
            for (Core::Neuron& neuron : layer.Neurons)
            {
                if (neuron.Id == neuronId)
                {
                    neuron.Bias = bias;
                    return true;
                }
            }
        }

        return false;
    }
}