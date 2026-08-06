#include "NetworkParameters.h"
#include "../Topology/NetworkTopology.h"

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

        NetworkTopology topology(network);

        Core::Neuron* neuron =
            topology.FindNeuron(neuronId);

        if (neuron == nullptr)
        {
            return false;
        }

        neuron->Bias = bias;

        return true;
    }

    bool NetworkParameters::SetLayerActivation(
        Core::Network& network,
        std::uint64_t layerId,
        Core::ActivationType activation)
    {
        NetworkTopology topology(network);

        Core::Layer* layer =
            topology.FindLayer(layerId);

        if (layer == nullptr)
        {
            return false;
        }

        layer->Activation = activation;

        return true;
    }
}
