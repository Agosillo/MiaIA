#include "BackwardEngine.h"

#include "../Evaluation/LossEvaluator.h"
#include "../Validation/NetworkValidator.h"
#include "../../Core/Model/Network.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{
    bool TryActivationDerivative(
        MiaIA::Core::ActivationType type,
        double activation,
        double& derivative)
    {
        if (!std::isfinite(activation))
        {
            return false;
        }

        switch (type)
        {
        case MiaIA::Core::ActivationType::Sigmoid:
            derivative = activation * (1.0 - activation);
            break;

        case MiaIA::Core::ActivationType::ReLU:
            derivative = activation > 0.0 ? 1.0 : 0.0;
            break;

        case MiaIA::Core::ActivationType::Tanh:
            derivative = 1.0 - activation * activation;
            break;

        case MiaIA::Core::ActivationType::Linear:
            derivative = 1.0;
            break;

        default:
            return false;
        }

        return std::isfinite(derivative);
    }
}

namespace MiaIA::Engine
{
    bool BackwardEngine::Run(
        const Core::Network& network,
        const Core::SampleEvaluationSnapshot& evaluation,
        Core::SampleGradientSnapshot& result)
    {
        if (!NetworkValidator::ValidateForForward(network))
        {
            return false;
        }

        std::vector<const Core::Layer*> orderedLayers;
        orderedLayers.reserve(network.Layers.size());

        for (const Core::Layer& layer : network.Layers)
        {
            orderedLayers.push_back(&layer);
        }

        std::sort(
            orderedLayers.begin(),
            orderedLayers.end(),
            [](const Core::Layer* left, const Core::Layer* right)
            {
                return left->Order < right->Order;
            });

        const Core::Layer& outputLayer = *orderedLayers.back();

        if (evaluation.Predictions.size() != outputLayer.Neurons.size() ||
            evaluation.Targets.size() != outputLayer.Neurons.size())
        {
            return false;
        }

        for (std::size_t index = 0;
            index < outputLayer.Neurons.size();
            ++index)
        {
            if (evaluation.Predictions[index] !=
                outputLayer.Neurons[index].Activation)
            {
                return false;
            }
        }

        std::vector<double> outputGradients;

        if (!LossEvaluator::EvaluateGradient(
            evaluation.Predictions,
            evaluation.Targets,
            evaluation.Type,
            outputGradients))
        {
            return false;
        }

        std::unordered_map<std::uint64_t, const Core::Neuron*> neuronsById;
        std::unordered_map<std::uint64_t, std::vector<const Core::Connection*>>
            incomingConnections;
        std::unordered_map<std::uint64_t, double> activationGradients;
        std::unordered_map<std::uint64_t, double> preActivationGradients;
        std::unordered_map<std::uint64_t, double> biasGradients;
        std::unordered_map<std::uint64_t, double> weightGradients;

        for (const Core::Layer& layer : network.Layers)
        {
            for (const Core::Neuron& neuron : layer.Neurons)
            {
                neuronsById[neuron.Id] = &neuron;
                activationGradients[neuron.Id] = 0.0;
                preActivationGradients[neuron.Id] = 0.0;
                biasGradients[neuron.Id] = 0.0;
            }
        }

        for (const Core::Connection& connection : network.Connections)
        {
            incomingConnections[connection.ToNeuron].push_back(&connection);
        }

        for (std::size_t index = 0;
            index < outputLayer.Neurons.size();
            ++index)
        {
            activationGradients[outputLayer.Neurons[index].Id] =
                outputGradients[index];
        }

        for (std::size_t layerIndex = orderedLayers.size();
            layerIndex-- > 1;)
        {
            const Core::Layer& layer = *orderedLayers[layerIndex];

            for (const Core::Neuron& neuron : layer.Neurons)
            {
                double activationDerivative{};

                if (!TryActivationDerivative(
                    layer.Activation,
                    neuron.Activation,
                    activationDerivative))
                {
                    return false;
                }

                const double preActivationGradient =
                    activationGradients[neuron.Id] * activationDerivative;

                if (!std::isfinite(preActivationGradient))
                {
                    return false;
                }

                preActivationGradients[neuron.Id] = preActivationGradient;
                biasGradients[neuron.Id] = preActivationGradient;

                for (const Core::Connection* connection :
                    incomingConnections[neuron.Id])
                {
                    const Core::Neuron& source =
                        *neuronsById[connection->FromNeuron];

                    const double weightGradient =
                        source.Activation * preActivationGradient;

                    const double sourceGradientContribution =
                        connection->Weight * preActivationGradient;

                    const double updatedSourceGradient =
                        activationGradients[source.Id] +
                        sourceGradientContribution;

                    if (!std::isfinite(weightGradient) ||
                        !std::isfinite(updatedSourceGradient))
                    {
                        return false;
                    }

                    weightGradients[connection->Id] = weightGradient;
                    activationGradients[source.Id] = updatedSourceGradient;
                }
            }
        }

        const Core::Layer& inputLayer = *orderedLayers.front();

        for (const Core::Neuron& neuron : inputLayer.Neurons)
        {
            preActivationGradients[neuron.Id] =
                activationGradients[neuron.Id];
        }

        Core::SampleGradientSnapshot gradients;
        gradients.Evaluation = evaluation;

        for (const Core::Layer* layer : orderedLayers)
        {
            for (const Core::Neuron& neuron : layer->Neurons)
            {
                Core::NeuronGradientSnapshot snapshot;
                snapshot.Id = neuron.Id;
                snapshot.LayerOrder = layer->Order;
                snapshot.ActivationGradient = activationGradients[neuron.Id];
                snapshot.PreActivationGradient =
                    preActivationGradients[neuron.Id];
                snapshot.BiasGradient = biasGradients[neuron.Id];

                gradients.Neurons.push_back(snapshot);
            }
        }

        for (const Core::Connection& connection : network.Connections)
        {
            const auto gradient = weightGradients.find(connection.Id);

            if (gradient == weightGradients.end())
            {
                return false;
            }

            Core::ConnectionGradientSnapshot snapshot;
            snapshot.Id = connection.Id;
            snapshot.FromNeuron = connection.FromNeuron;
            snapshot.ToNeuron = connection.ToNeuron;
            snapshot.WeightGradient = gradient->second;

            gradients.Connections.push_back(snapshot);
        }

        result = std::move(gradients);

        return true;
    }
}
