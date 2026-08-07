#include "PredictionEvaluator.h"

#include "../Input/NetworkInput.h"
#include "../Runtime/NetworkRuntime.h"
#include "../Validation/NetworkValidator.h"
#include "../../Core/Model/Network.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <utility>

namespace
{
    void RestoreActivations(
        MiaIA::Core::Network& network,
        const std::unordered_map<std::uint64_t, double>& activations)
    {
        for (MiaIA::Core::Layer& layer : network.Layers)
        {
            for (MiaIA::Core::Neuron& neuron : layer.Neurons)
            {
                const auto activation = activations.find(neuron.Id);

                if (activation != activations.end())
                {
                    neuron.Activation = activation->second;
                }
            }
        }
    }
}

namespace MiaIA::Engine
{
    bool PredictionEvaluator::Predict(
        Core::Network& network,
        const std::vector<double>& inputs,
        Core::PredictionSnapshot& result)
    {
        if (!NetworkValidator::ValidateForForward(network))
        {
            return false;
        }

        const auto inputLayer = std::find_if(
            network.Layers.begin(),
            network.Layers.end(),
            [](const Core::Layer& layer)
            {
                return layer.Order == 0;
            });

        if (inputLayer == network.Layers.end() ||
            inputs.size() != inputLayer->Neurons.size())
        {
            return false;
        }

        for (const double input : inputs)
        {
            if (!std::isfinite(input))
            {
                return false;
            }
        }

        std::unordered_map<std::uint64_t, double> previousActivations;

        for (const Core::Layer& layer : network.Layers)
        {
            for (const Core::Neuron& neuron : layer.Neurons)
            {
                previousActivations[neuron.Id] = neuron.Activation;
            }
        }

        if (!NetworkInput::SetValues(network, inputs) ||
            !NetworkRuntime::Forward(network))
        {
            RestoreActivations(network, previousActivations);
            return false;
        }

        const auto outputLayer = std::max_element(
            network.Layers.begin(),
            network.Layers.end(),
            [](const Core::Layer& left, const Core::Layer& right)
            {
                return left.Order < right.Order;
            });

        if (outputLayer == network.Layers.end() ||
            outputLayer->Neurons.empty())
        {
            RestoreActivations(network, previousActivations);
            return false;
        }

        std::vector<double> outputs;
        outputs.reserve(outputLayer->Neurons.size());

        for (const Core::Neuron& neuron : outputLayer->Neurons)
        {
            if (!std::isfinite(neuron.Activation))
            {
                RestoreActivations(network, previousActivations);
                return false;
            }

            outputs.push_back(neuron.Activation);
        }

        Core::PredictionSnapshot prediction;
        prediction.Inputs = inputs;
        prediction.Outputs = std::move(outputs);

        result = std::move(prediction);

        return true;
    }
}
