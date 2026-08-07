#include "SampleEvaluator.h"

#include "LossEvaluator.h"
#include "../Input/NetworkInput.h"
#include "../Runtime/NetworkRuntime.h"
#include "../Validation/NetworkValidator.h"
#include "../../Core/Model/Dataset.h"
#include "../../Core/Model/Network.h"

#include <algorithm>
#include <utility>

namespace MiaIA::Engine
{
    bool SampleEvaluator::Evaluate(
        const Core::Dataset& dataset,
        std::size_t index,
        Core::Network& network,
        Core::LossType type,
        Core::SampleEvaluationSnapshot& result)
    {
        if (type != Core::LossType::MeanSquaredError ||
            index >= dataset.Samples.size() ||
            !NetworkValidator::ValidateForForward(network))
        {
            return false;
        }

        const Core::Sample& sample = dataset.Samples[index];

        const auto inputLayer = std::find_if(
            network.Layers.begin(),
            network.Layers.end(),
            [](const Core::Layer& layer)
            {
                return layer.Order == 0;
            });

        const auto outputLayer = std::max_element(
            network.Layers.begin(),
            network.Layers.end(),
            [](const Core::Layer& left, const Core::Layer& right)
            {
                return left.Order < right.Order;
            });

        if (inputLayer == network.Layers.end() ||
            outputLayer == network.Layers.end() ||
            sample.Inputs.size() != inputLayer->Neurons.size() ||
            sample.Targets.size() != outputLayer->Neurons.size())
        {
            return false;
        }

        if (!NetworkInput::SetValues(network, sample.Inputs) ||
            !NetworkRuntime::Forward(network))
        {
            return false;
        }

        const auto evaluatedOutputLayer = std::max_element(
            network.Layers.begin(),
            network.Layers.end(),
            [](const Core::Layer& left, const Core::Layer& right)
            {
                return left.Order < right.Order;
            });

        std::vector<double> predictions;
        predictions.reserve(evaluatedOutputLayer->Neurons.size());

        for (const Core::Neuron& neuron : evaluatedOutputLayer->Neurons)
        {
            predictions.push_back(neuron.Activation);
        }

        std::vector<double> errors;
        double loss{};

        if (!LossEvaluator::Evaluate(
            predictions,
            sample.Targets,
            type,
            errors,
            loss))
        {
            return false;
        }

        Core::SampleEvaluationSnapshot evaluation;
        evaluation.SampleIndex = index;
        evaluation.Type = type;
        evaluation.Targets = sample.Targets;
        evaluation.Predictions = std::move(predictions);
        evaluation.Errors = std::move(errors);
        evaluation.Loss = loss;

        result = std::move(evaluation);

        return true;
    }
}
