#pragma once

#include <vector>
#include <algorithm>
#include <unordered_map>
#include <utility>
#include "ForwardEngine.h"
#include "../../Core/Execution/Activation.h"
#include "../Validation/NetworkValidator.h"

namespace MiaIA::Engine
{
    namespace
    {
        bool RunForward(
            Core::Network& network,
            Core::ForwardTraceSnapshot* trace)
        {
            if (!NetworkValidator::ValidateForForward(network))
            {
                return false;
            }

            std::unordered_map<std::uint64_t, Core::Neuron*> neuronsById;

            std::size_t neuronCount = 0;

            for (const Core::Layer& layer : network.Layers)
            {
                neuronCount += layer.Neurons.size();
            }

            neuronsById.reserve(neuronCount);

            for (Core::Layer& layer : network.Layers)
            {
                for (Core::Neuron& neuron : layer.Neurons)
                {
                    neuronsById[neuron.Id] = &neuron;
                }
            }

            std::unordered_map<
                std::uint64_t,
                std::vector<const Core::Connection*>>
                connectionsByTarget;

            connectionsByTarget.reserve(neuronsById.size());

            for (const Core::Connection& connection : network.Connections)
            {
                connectionsByTarget[connection.ToNeuron].push_back(&connection);
            }

            std::sort(
                network.Layers.begin(),
                network.Layers.end(),
                [](const Core::Layer& a, const Core::Layer& b)
                {
                    return a.Order < b.Order;
                });

            Core::ForwardTraceSnapshot snapshot;

            if (trace != nullptr)
            {
                snapshot.Layers.reserve(network.Layers.size());
                snapshot.Inputs.reserve(network.Layers.front().Neurons.size());

                for (const Core::Neuron& neuron :
                    network.Layers.front().Neurons)
                {
                    snapshot.Inputs.push_back(neuron.Activation);
                }
            }

            for (std::size_t layerIndex = 0;
                layerIndex < network.Layers.size();
                ++layerIndex)
            {
                Core::Layer& layer = network.Layers[layerIndex];
                Core::ForwardTraceLayerSnapshot layerTrace;

                if (trace != nullptr)
                {
                    layerTrace.Id = layer.Id;
                    layerTrace.Name = layer.Name;
                    layerTrace.Order = layer.Order;
                    layerTrace.Activation = layer.Activation;
                    layerTrace.Neurons.reserve(layer.Neurons.size());
                }

                for (Core::Neuron& neuron : layer.Neurons)
                {
                    double weightedInputSum{};
                    double preActivation = neuron.Activation;

                    if (layerIndex > 0)
                    {
                        const auto targetIt =
                            connectionsByTarget.find(neuron.Id);

                        for (const Core::Connection* connection :
                            targetIt->second)
                        {
                            const auto sourceIt =
                                neuronsById.find(connection->FromNeuron);

                            weightedInputSum +=
                                sourceIt->second->Activation *
                                connection->Weight;
                        }

                        preActivation = weightedInputSum + neuron.Bias;

                        switch (layer.Activation)
                        {
                        case Core::ActivationType::Sigmoid:
                            neuron.Activation =
                                Core::Activation::Sigmoid(preActivation);
                            break;

                        case Core::ActivationType::ReLU:
                            neuron.Activation =
                                Core::Activation::ReLU(preActivation);
                            break;

                        case Core::ActivationType::Tanh:
                            neuron.Activation =
                                Core::Activation::Tanh(preActivation);
                            break;

                        case Core::ActivationType::Linear:
                            neuron.Activation =
                                Core::Activation::Linear(preActivation);
                            break;
                        }
                    }

                    if (trace != nullptr)
                    {
                        layerTrace.Neurons.push_back({
                            neuron.Id,
                            weightedInputSum,
                            neuron.Bias,
                            preActivation,
                            neuron.Activation,
                            layerIndex == 0
                        });
                    }
                }

                if (trace != nullptr)
                {
                    snapshot.Layers.push_back(std::move(layerTrace));
                }
            }

            if (trace != nullptr)
            {
                snapshot.Outputs.reserve(network.Layers.back().Neurons.size());

                for (const Core::Neuron& neuron :
                    network.Layers.back().Neurons)
                {
                    snapshot.Outputs.push_back(neuron.Activation);
                }

                *trace = std::move(snapshot);
            }

            return true;
        }
    }

    bool ForwardEngine::Run(Core::Network& network)
    {
        return RunForward(network, nullptr);
    }

    bool ForwardEngine::Run(
        Core::Network& network,
        Core::ForwardTraceSnapshot& trace)
    {
        return RunForward(network, &trace);
    }
}
