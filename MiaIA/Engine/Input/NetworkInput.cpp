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

        NetworkTopology topology(network);

        Core::Neuron* neuron =
            topology.FindNeuron(neuronId);

        if (neuron == nullptr)
        {
            return false;
        }

        neuron->Activation = activation;

        return true;
    }

    bool NetworkInput::SetValues(
        Core::Network& network,
        const std::vector<double>& values)
    {
        Core::Layer* inputLayer = nullptr;

        for (Core::Layer& layer : network.Layers)
        {
            if (layer.Order == 0)
            {
                inputLayer = &layer;
                break;
            }
        }

        if (inputLayer == nullptr ||
            inputLayer->Neurons.empty() ||
            inputLayer->Neurons.size() != values.size())
        {
            return false;
        }

        for (const double value : values)
        {
            if (!std::isfinite(value))
            {
                return false;
            }
        }

        for (std::size_t index = 0;
            index < values.size();
            ++index)
        {
            inputLayer->Neurons[index].Activation = values[index];
        }

        return true;
    }
}
