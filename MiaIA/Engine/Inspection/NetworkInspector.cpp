#include "NetworkInspector.h"
#include "../Differentiation/BackwardEngine.h"
#include "../Evaluation/LossEvaluator.h"
#include "../Execution/ForwardEngine.h"
#include "../Input/NetworkInput.h"
#include "../Topology/NetworkTopology.h"
#include "../../Core/Execution/SnapshotBuilder.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <utility>

namespace
{
    constexpr std::size_t MaximumRelationshipPageSize = 1000;
    constexpr std::size_t MaximumTraceContributionPageSize = 1000;

    bool TryBuildNeuronContext(
        const MiaIA::Engine::NetworkTopology& topology,
        std::uint64_t neuronId,
        MiaIA::Core::NeuronContextSnapshot& result)
    {
        const MiaIA::Core::Neuron* neuron =
            topology.FindNeuron(neuronId);
        const MiaIA::Core::Layer* layer =
            topology.FindLayerForNeuron(neuronId);

        if (neuron == nullptr || layer == nullptr)
        {
            return false;
        }

        result = MiaIA::Core::NeuronContextSnapshot{
            {
                neuron->Id,
                neuron->Activation,
                neuron->Bias
            },
            layer->Id,
            layer->Name,
            layer->Order,
            layer->Activation
        };
        return true;
    }

    bool TryBuildNeuronContextWithoutConnectionIndex(
        const MiaIA::Core::Network& network,
        std::uint64_t neuronId,
        MiaIA::Core::NeuronContextSnapshot& result)
    {
        for (const MiaIA::Core::Layer& layer : network.Layers)
        {
            const auto neuron = std::find_if(
                layer.Neurons.begin(),
                layer.Neurons.end(),
                [neuronId](const MiaIA::Core::Neuron& candidate)
                {
                    return candidate.Id == neuronId;
                });

            if (neuron == layer.Neurons.end())
            {
                continue;
            }

            result = MiaIA::Core::NeuronContextSnapshot{
                {
                    neuron->Id,
                    neuron->Activation,
                    neuron->Bias
                },
                layer.Id,
                layer.Name,
                layer.Order,
                layer.Activation
            };
            return true;
        }

        return false;
    }

    const MiaIA::Core::ForwardTraceNeuronSnapshot* FindTraceNeuron(
        const MiaIA::Core::ForwardTraceSnapshot& trace,
        std::uint64_t neuronId)
    {
        for (const auto& layer : trace.Layers)
        {
            const auto neuron = std::find_if(
                layer.Neurons.begin(),
                layer.Neurons.end(),
                [neuronId](const auto& candidate)
                {
                    return candidate.Id == neuronId;
                });

            if (neuron != layer.Neurons.end())
            {
                return &*neuron;
            }
        }

        return nullptr;
    }
}

namespace MiaIA::Engine
{
    Core::NetworkSnapshot NetworkInspector::Snapshot(
        const Core::Network& network)
    {
        return Core::SnapshotBuilder::Build(network);
    }

    Core::NetworkOverviewSnapshot NetworkInspector::Overview(
        const Core::Network& network)
    {
        Core::NetworkOverviewSnapshot result;
        result.Layers.reserve(network.Layers.size());
        result.ConnectionCount = network.Connections.size();

        for (const Core::Layer& layer : network.Layers)
        {
            result.NeuronCount += layer.Neurons.size();
            result.Layers.push_back(Core::LayerOverviewSnapshot{
                layer.Id,
                layer.Name,
                layer.Order,
                layer.Neurons.size(),
                layer.Activation
            });
        }

        return result;
    }

    bool NetworkInspector::TryGetNeuron(
        const Core::Network& network,
        std::uint64_t neuronId,
        Core::NeuronSnapshot& result)
    {
        const NetworkTopology topology(network);

        const Core::Neuron* neuron =
            topology.FindNeuron(neuronId);

        if (neuron == nullptr)
        {
            return false;
        }

        result = Core::NeuronSnapshot{
            neuron->Id,
            neuron->Activation,
            neuron->Bias
        };

        return true;
    }

    bool NetworkInspector::TryGetConnection(
        const Core::Network& network,
        std::uint64_t connectionId,
        Core::ConnectionSnapshot& result)
    {
        const NetworkTopology topology(network);

        const Core::Connection* connection =
            topology.FindConnection(connectionId);

        if (connection == nullptr)
        {
            return false;
        }

        result = Core::ConnectionSnapshot{
            connection->Id,
            connection->FromNeuron,
            connection->ToNeuron,
            connection->Weight
        };

        return true;
    }

    bool NetworkInspector::TryInspectNeuron(
        const Core::Network& network,
        std::uint64_t neuronId,
        std::size_t maximumConnectionsPerDirection,
        Core::NeuronInspectionSnapshot& result)
    {
        const NetworkTopology topology(network);
        Core::NeuronInspectionSnapshot snapshot;

        if (!TryBuildNeuronContext(
            topology,
            neuronId,
            snapshot.Context))
        {
            return false;
        }

        snapshot.IncomingConnections.reserve(
            std::min(
                maximumConnectionsPerDirection,
                network.Connections.size()));
        snapshot.OutgoingConnections.reserve(
            std::min(
                maximumConnectionsPerDirection,
                network.Connections.size()));

        for (const Core::Connection& connection : network.Connections)
        {
            if (connection.ToNeuron == neuronId)
            {
                ++snapshot.IncomingConnectionCount;

                if (snapshot.IncomingConnections.size() <
                    maximumConnectionsPerDirection)
                {
                    snapshot.IncomingConnections.push_back({
                        connection.Id,
                        connection.FromNeuron,
                        connection.ToNeuron,
                        connection.Weight
                    });
                }
            }

            if (connection.FromNeuron == neuronId)
            {
                ++snapshot.OutgoingConnectionCount;

                if (snapshot.OutgoingConnections.size() <
                    maximumConnectionsPerDirection)
                {
                    snapshot.OutgoingConnections.push_back({
                        connection.Id,
                        connection.FromNeuron,
                        connection.ToNeuron,
                        connection.Weight
                    });
                }
            }
        }

        result = std::move(snapshot);
        return true;
    }

    bool NetworkInspector::TryGetNeuronRelationships(
        const Core::Network& network,
        std::uint64_t neuronId,
        const Core::NeuronRelationshipPageRequest& request,
        Core::NeuronRelationshipPageSnapshot& result)
    {
        const bool validDirection = request.Direction ==
            Core::NeuronRelationshipDirection::Incoming ||
            request.Direction ==
                Core::NeuronRelationshipDirection::Outgoing;
        const bool validSort = request.Sort ==
            Core::NeuronRelationshipSort::ConnectionId ||
            request.Sort == Core::NeuronRelationshipSort::Weight ||
            request.Sort ==
                Core::NeuronRelationshipSort::AbsoluteWeight;

        if (!validDirection || !validSort || request.Limit == 0 ||
            request.Limit > MaximumRelationshipPageSize ||
            !std::isfinite(request.MinimumAbsoluteWeight) ||
            request.MinimumAbsoluteWeight < 0.0)
        {
            return false;
        }

        Core::NeuronRelationshipPageSnapshot snapshot;

        if (!TryBuildNeuronContextWithoutConnectionIndex(
            network,
            neuronId,
            snapshot.Context))
        {
            return false;
        }

        snapshot.Direction = request.Direction;
        snapshot.Offset = request.Offset;
        snapshot.Limit = request.Limit;

        std::vector<const Core::Connection*> filteredConnections;

        for (const Core::Connection& connection : network.Connections)
        {
            const bool matchesDirection =
                request.Direction ==
                    Core::NeuronRelationshipDirection::Incoming
                ? connection.ToNeuron == neuronId
                : connection.FromNeuron == neuronId;

            if (!matchesDirection)
            {
                continue;
            }

            ++snapshot.TotalConnectionCount;

            if (std::abs(connection.Weight) <
                request.MinimumAbsoluteWeight)
            {
                continue;
            }

            filteredConnections.push_back(&connection);
        }

        snapshot.FilteredConnectionCount = filteredConnections.size();

        const auto ascendingLess = [&request](
            const Core::Connection* left,
            const Core::Connection* right)
        {
            switch (request.Sort)
            {
            case Core::NeuronRelationshipSort::Weight:
                if (left->Weight != right->Weight)
                {
                    return left->Weight < right->Weight;
                }
                break;

            case Core::NeuronRelationshipSort::AbsoluteWeight:
                if (std::abs(left->Weight) != std::abs(right->Weight))
                {
                    return std::abs(left->Weight) <
                        std::abs(right->Weight);
                }
                break;

            case Core::NeuronRelationshipSort::ConnectionId:
                break;
            }

            return left->Id < right->Id;
        };

        std::sort(
            filteredConnections.begin(),
            filteredConnections.end(),
            [&ascendingLess, &request](
                const Core::Connection* left,
                const Core::Connection* right)
            {
                return request.Descending
                    ? ascendingLess(right, left)
                    : ascendingLess(left, right);
            });

        if (request.Offset < filteredConnections.size())
        {
            const std::size_t pageSize = std::min(
                request.Limit,
                filteredConnections.size() - request.Offset);
            const std::size_t end = request.Offset + pageSize;
            snapshot.Connections.reserve(pageSize);

            for (std::size_t index = request.Offset;
                index < end;
                ++index)
            {
                const Core::Connection& connection =
                    *filteredConnections[index];
                snapshot.Connections.push_back({
                    connection.Id,
                    connection.FromNeuron,
                    connection.ToNeuron,
                    connection.Weight
                });
            }
        }

        snapshot.HasPrevious = request.Offset > 0 &&
            snapshot.FilteredConnectionCount > 0;
        snapshot.HasNext = request.Offset <
            snapshot.FilteredConnectionCount &&
            snapshot.Connections.size() <
                snapshot.FilteredConnectionCount - request.Offset;
        result = std::move(snapshot);
        return true;
    }

    bool NetworkInspector::TryInspectConnection(
        const Core::Network& network,
        std::uint64_t connectionId,
        Core::ConnectionInspectionSnapshot& result)
    {
        const NetworkTopology topology(network);
        const Core::Connection* connection =
            topology.FindConnection(connectionId);

        if (connection == nullptr)
        {
            return false;
        }

        Core::ConnectionInspectionSnapshot snapshot;
        snapshot.Connection = {
            connection->Id,
            connection->FromNeuron,
            connection->ToNeuron,
            connection->Weight
        };

        if (!TryBuildNeuronContext(
                topology,
                connection->FromNeuron,
                snapshot.FromNeuron) ||
            !TryBuildNeuronContext(
                topology,
                connection->ToNeuron,
                snapshot.ToNeuron))
        {
            return false;
        }

        result = std::move(snapshot);
        return true;
    }

    bool NetworkInspector::TraceForward(
        const Core::Network& network,
        const std::vector<double>& inputs,
        Core::ForwardTraceSnapshot& result)
    {
        Core::Network candidate = network;

        if (!NetworkInput::SetValues(candidate, inputs))
        {
            return false;
        }

        Core::ForwardTraceSnapshot snapshot;

        if (!ForwardEngine::Run(candidate, snapshot))
        {
            return false;
        }

        result = std::move(snapshot);
        return true;
    }

    bool NetworkInspector::TraceBackward(
        const Core::Network& network,
        const std::vector<double>& inputs,
        const std::vector<double>& targets,
        Core::LossType loss,
        Core::BackwardTraceSnapshot& result)
    {
        Core::Network candidate = network;

        if (!NetworkInput::SetValues(candidate, inputs))
        {
            return false;
        }

        Core::ForwardTraceSnapshot forwardTrace;

        if (!ForwardEngine::Run(candidate, forwardTrace))
        {
            return false;
        }

        Core::SampleEvaluationSnapshot evaluation;
        evaluation.Type = loss;
        evaluation.Targets = targets;
        evaluation.Predictions = forwardTrace.Outputs;

        if (!LossEvaluator::Evaluate(
            evaluation.Predictions,
            evaluation.Targets,
            evaluation.Type,
            evaluation.Errors,
            evaluation.Loss))
        {
            return false;
        }

        Core::SampleGradientSnapshot gradients;

        if (!BackwardEngine::Run(candidate, evaluation, gradients))
        {
            return false;
        }

        std::unordered_map<
            std::uint64_t,
            const Core::NeuronGradientSnapshot*> gradientsByNeuron;
        gradientsByNeuron.reserve(gradients.Neurons.size());

        for (const Core::NeuronGradientSnapshot& gradient :
            gradients.Neurons)
        {
            gradientsByNeuron.emplace(gradient.Id, &gradient);
        }

        Core::BackwardTraceSnapshot trace;
        trace.Inputs = inputs;
        trace.Targets = targets;
        trace.Predictions = evaluation.Predictions;
        trace.Errors = evaluation.Errors;
        trace.Loss = loss;
        trace.LossValue = evaluation.Loss;
        trace.Layers.reserve(forwardTrace.Layers.size());

        for (std::size_t layerIndex = 0;
            layerIndex < forwardTrace.Layers.size();
            ++layerIndex)
        {
            const Core::ForwardTraceLayerSnapshot& forwardLayer =
                forwardTrace.Layers[layerIndex];
            Core::BackwardTraceLayerSnapshot layer;
            layer.Id = forwardLayer.Id;
            layer.Name = forwardLayer.Name;
            layer.Order = forwardLayer.Order;
            layer.Activation = forwardLayer.Activation;
            layer.Neurons.reserve(forwardLayer.Neurons.size());

            for (const Core::ForwardTraceNeuronSnapshot& forwardNeuron :
                forwardLayer.Neurons)
            {
                const auto gradient = gradientsByNeuron.find(
                    forwardNeuron.Id);

                if (gradient == gradientsByNeuron.end())
                {
                    return false;
                }

                layer.Neurons.push_back({
                    forwardNeuron.Id,
                    forwardNeuron.Activation,
                    gradient->second->ActivationGradient,
                    gradient->second->PreActivationGradient,
                    gradient->second->BiasGradient,
                    layerIndex == 0,
                    layerIndex + 1 == forwardTrace.Layers.size()
                });
            }

            trace.Layers.push_back(std::move(layer));
        }

        std::unordered_map<std::uint64_t, double>
            preActivationGradients;
        preActivationGradients.reserve(gradients.Neurons.size());

        for (const Core::NeuronGradientSnapshot& gradient :
            gradients.Neurons)
        {
            preActivationGradients.emplace(
                gradient.Id,
                gradient.PreActivationGradient);
        }

        std::unordered_map<std::uint64_t, double> weightGradients;
        weightGradients.reserve(gradients.Connections.size());

        for (const Core::ConnectionGradientSnapshot& gradient :
            gradients.Connections)
        {
            weightGradients.emplace(gradient.Id, gradient.WeightGradient);
        }

        trace.Connections.reserve(candidate.Connections.size());

        for (const Core::Connection& connection : candidate.Connections)
        {
            const auto targetGradient = preActivationGradients.find(
                connection.ToNeuron);
            const auto weightGradient = weightGradients.find(connection.Id);

            if (targetGradient == preActivationGradients.end() ||
                weightGradient == weightGradients.end())
            {
                return false;
            }

            const double sourceContribution =
                connection.Weight * targetGradient->second;

            if (!std::isfinite(sourceContribution))
            {
                return false;
            }

            trace.Connections.push_back({
                connection.Id,
                connection.FromNeuron,
                connection.ToNeuron,
                connection.Weight,
                weightGradient->second,
                sourceContribution
            });
        }

        result = std::move(trace);
        return true;
    }

    bool NetworkInspector::TryGetForwardTraceContributions(
        const Core::Network& network,
        const std::vector<double>& inputs,
        std::uint64_t neuronId,
        const Core::ForwardTraceContributionPageRequest& request,
        Core::ForwardTraceContributionPageSnapshot& result)
    {
        const bool validSort = request.Sort ==
            Core::ForwardTraceContributionSort::ConnectionId ||
            request.Sort ==
                Core::ForwardTraceContributionSort::Contribution ||
            request.Sort == Core::ForwardTraceContributionSort::
                AbsoluteContribution;

        if (!validSort || request.Limit == 0 ||
            request.Limit > MaximumTraceContributionPageSize ||
            !std::isfinite(request.MinimumAbsoluteContribution) ||
            request.MinimumAbsoluteContribution < 0.0)
        {
            return false;
        }

        Core::ForwardTraceSnapshot trace;

        if (!TraceForward(network, inputs, trace))
        {
            return false;
        }

        const Core::ForwardTraceNeuronSnapshot* tracedNeuron =
            FindTraceNeuron(trace, neuronId);

        if (tracedNeuron == nullptr)
        {
            return false;
        }

        Core::ForwardTraceContributionPageSnapshot snapshot;
        snapshot.Neuron = *tracedNeuron;
        snapshot.Offset = request.Offset;
        snapshot.Limit = request.Limit;

        std::unordered_map<std::uint64_t, double> activationByNeuron;
        activationByNeuron.reserve(network.Connections.size());

        for (const Core::ForwardTraceLayerSnapshot& layer : trace.Layers)
        {
            for (const Core::ForwardTraceNeuronSnapshot& neuron :
                layer.Neurons)
            {
                activationByNeuron.emplace(neuron.Id, neuron.Activation);
            }
        }

        std::vector<Core::ForwardTraceConnectionContributionSnapshot>
            contributions;

        for (const Core::Connection& connection : network.Connections)
        {
            if (connection.ToNeuron != neuronId)
            {
                continue;
            }

            ++snapshot.TotalContributionCount;
            const auto source = activationByNeuron.find(
                connection.FromNeuron);

            if (source == activationByNeuron.end())
            {
                return false;
            }

            const double contribution =
                source->second * connection.Weight;

            if (std::abs(contribution) <
                request.MinimumAbsoluteContribution)
            {
                continue;
            }

            contributions.push_back({
                connection.Id,
                connection.FromNeuron,
                connection.ToNeuron,
                source->second,
                connection.Weight,
                contribution
            });
        }

        snapshot.FilteredContributionCount = contributions.size();

        const auto ascendingLess = [&request](
            const auto& left,
            const auto& right)
        {
            switch (request.Sort)
            {
            case Core::ForwardTraceContributionSort::Contribution:
                if (left.Contribution != right.Contribution)
                {
                    return left.Contribution < right.Contribution;
                }
                break;

            case Core::ForwardTraceContributionSort::AbsoluteContribution:
                if (std::abs(left.Contribution) !=
                    std::abs(right.Contribution))
                {
                    return std::abs(left.Contribution) <
                        std::abs(right.Contribution);
                }
                break;

            case Core::ForwardTraceContributionSort::ConnectionId:
                break;
            }

            return left.ConnectionId < right.ConnectionId;
        };

        std::sort(
            contributions.begin(),
            contributions.end(),
            [&ascendingLess, &request](const auto& left, const auto& right)
            {
                return request.Descending
                    ? ascendingLess(right, left)
                    : ascendingLess(left, right);
            });

        if (request.Offset < contributions.size())
        {
            const std::size_t count = std::min(
                request.Limit,
                contributions.size() - request.Offset);
            const auto begin = contributions.begin() + request.Offset;
            snapshot.Contributions.assign(begin, begin + count);
        }

        snapshot.HasPrevious = request.Offset > 0 &&
            snapshot.FilteredContributionCount > 0;
        snapshot.HasNext = request.Offset <
            snapshot.FilteredContributionCount &&
            snapshot.Contributions.size() <
                snapshot.FilteredContributionCount - request.Offset;

        result = std::move(snapshot);
        return true;
    }

    bool NetworkInspector::TryGetLayer(
        const Core::Network& network,
        std::uint64_t layerId,
        Core::LayerSnapshot& result)
    {
        const NetworkTopology topology(network);

        const Core::Layer* layer =
            topology.FindLayer(layerId);

        if (layer == nullptr)
        {
            return false;
        }

        Core::LayerSnapshot snapshot;
        snapshot.Id = layer->Id;
        snapshot.Name = layer->Name;
        snapshot.Order = layer->Order;
        snapshot.Activation = layer->Activation;
        snapshot.Neurons.reserve(layer->Neurons.size());

        for (const Core::Neuron& neuron : layer->Neurons)
        {
            snapshot.Neurons.push_back(
                Core::NeuronSnapshot{
                    neuron.Id,
                    neuron.Activation,
                    neuron.Bias
                });
        }

        result = std::move(snapshot);

        return true;
    }
}
