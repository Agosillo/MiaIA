#include "NetworkParameters.h"
#include "../Topology/NetworkTopology.h"
#include "../Validation/NetworkValidator.h"

#include <cmath>
#include <utility>

namespace
{
    bool IsSupportedActivation(MiaIA::Core::ActivationType activation)
    {
        switch (activation)
        {
        case MiaIA::Core::ActivationType::Sigmoid:
        case MiaIA::Core::ActivationType::ReLU:
        case MiaIA::Core::ActivationType::Tanh:
        case MiaIA::Core::ActivationType::Linear:
            return true;
        }

        return false;
    }
}

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

        Core::Layer* layer =
            topology.FindLayerForNeuron(neuronId);
        Core::Neuron* neuron =
            topology.FindNeuron(neuronId);

        if (layer == nullptr || layer->Order == 0 || neuron == nullptr)
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

    bool NetworkParameters::ApplyUpdate(
        Core::Network& network,
        const Core::NetworkParameterUpdate& update,
        Core::NetworkParameterUpdateSnapshot& result)
    {
        if (!update.HasRequestedChanges() ||
            !NetworkValidator::ValidateForForward(network) ||
            (update.HiddenActivation.has_value() &&
                !IsSupportedActivation(*update.HiddenActivation)) ||
            (update.OutputActivation.has_value() &&
                !IsSupportedActivation(*update.OutputActivation)) ||
            (update.ConnectionWeight.has_value() &&
                !std::isfinite(*update.ConnectionWeight)) ||
            (update.NonInputBias.has_value() &&
                !std::isfinite(*update.NonInputBias)))
        {
            return false;
        }

        Core::Network candidate = network;
        Core::NetworkParameterUpdateSnapshot candidateResult;
        const std::uint64_t outputOrder =
            static_cast<std::uint64_t>(candidate.Layers.size() - 1);

        for (Core::Layer& layer : candidate.Layers)
        {
            if (layer.Order > 0 &&
                layer.Order < outputOrder &&
                update.HiddenActivation.has_value() &&
                layer.Activation != *update.HiddenActivation)
            {
                layer.Activation = *update.HiddenActivation;
                ++candidateResult.HiddenLayersChanged;
            }

            if (layer.Order == outputOrder &&
                update.OutputActivation.has_value() &&
                layer.Activation != *update.OutputActivation)
            {
                layer.Activation = *update.OutputActivation;
                candidateResult.OutputLayerChanged = true;
            }

            if (layer.Order > 0 && update.NonInputBias.has_value())
            {
                for (Core::Neuron& neuron : layer.Neurons)
                {
                    if (neuron.Bias != *update.NonInputBias)
                    {
                        neuron.Bias = *update.NonInputBias;
                        ++candidateResult.NeuronBiasesChanged;
                    }
                }
            }
        }

        if (update.ConnectionWeight.has_value())
        {
            for (Core::Connection& connection : candidate.Connections)
            {
                if (connection.Weight != *update.ConnectionWeight)
                {
                    connection.Weight = *update.ConnectionWeight;
                    ++candidateResult.ConnectionWeightsChanged;
                }
            }
        }

        if (!NetworkValidator::ValidateForForward(candidate))
        {
            return false;
        }

        network = std::move(candidate);
        result = candidateResult;
        return true;
    }
}
