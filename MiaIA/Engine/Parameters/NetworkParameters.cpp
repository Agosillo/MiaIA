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
}