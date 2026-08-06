#include <cmath>
#include "NetworkInput.h"
#include "../Topology/NetworkTopology.h"

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

        Core::Neuron* neuron =
            NetworkTopology::FindNeuron(
                network,
                neuronId);

        if (neuron == nullptr)
        {
            return false;
        }

        neuron->Activation = activation;

        return true;
    }
}