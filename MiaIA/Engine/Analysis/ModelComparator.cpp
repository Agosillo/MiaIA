#include "ModelComparator.h"

#include "../Validation/NetworkValidator.h"

#include <cmath>
#include <map>
#include <utility>

namespace
{
    MiaIA::Core::ModelTopologySummarySnapshot Summarize(
        const MiaIA::Core::Network& network)
    {
        MiaIA::Core::ModelTopologySummarySnapshot result;
        result.LayerCount = network.Layers.size();
        result.ConnectionCount = network.Connections.size();

        for (const MiaIA::Core::Layer& layer : network.Layers)
        {
            result.NeuronCount += layer.Neurons.size();
        }

        return result;
    }

    MiaIA::Core::ModelValueComparisonSnapshot CompareValues(
        double reference,
        double current)
    {
        MiaIA::Core::ModelValueComparisonSnapshot result;
        result.FirstValue = reference;
        result.SecondValue = current;
        result.Delta = current - reference;
        result.AbsoluteDelta = std::abs(result.Delta);
        return result;
    }

    void CompareTopology(
        const MiaIA::Core::Network& reference,
        const MiaIA::Core::Network& current,
        MiaIA::Core::ModelTopologyCompatibilitySnapshot& result)
    {
        result.Reference = Summarize(reference);
        result.Current = Summarize(current);
        result.LayerCountMatches = result.Reference.LayerCount ==
            result.Current.LayerCount;
        result.NeuronCountMatches = result.Reference.NeuronCount ==
            result.Current.NeuronCount;
        result.ConnectionCountMatches = result.Reference.ConnectionCount ==
            result.Current.ConnectionCount;
        result.LayerStructureMatches = true;
        result.NeuronStructureMatches = true;
        result.ConnectionStructureMatches = true;

        std::map<std::uint64_t, const MiaIA::Core::Layer*> currentLayers;
        for (const MiaIA::Core::Layer& layer : current.Layers)
        {
            currentLayers.emplace(layer.Id, &layer);
        }

        for (const MiaIA::Core::Layer& referenceLayer : reference.Layers)
        {
            const auto currentLayer = currentLayers.find(referenceLayer.Id);
            if (currentLayer == currentLayers.end() ||
                referenceLayer.Order != currentLayer->second->Order)
            {
                result.LayerStructureMatches = false;
                result.NeuronStructureMatches = false;
                continue;
            }

            std::map<std::uint64_t, const MiaIA::Core::Neuron*>
                currentNeurons;
            for (const MiaIA::Core::Neuron& neuron :
                currentLayer->second->Neurons)
            {
                currentNeurons.emplace(neuron.Id, &neuron);
            }

            if (referenceLayer.Neurons.size() !=
                currentLayer->second->Neurons.size())
            {
                result.NeuronStructureMatches = false;
            }

            for (const MiaIA::Core::Neuron& neuron :
                referenceLayer.Neurons)
            {
                if (!currentNeurons.contains(neuron.Id))
                {
                    result.NeuronStructureMatches = false;
                }
            }
        }

        std::map<std::uint64_t, const MiaIA::Core::Connection*>
            currentConnections;
        for (const MiaIA::Core::Connection& connection :
            current.Connections)
        {
            currentConnections.emplace(connection.Id, &connection);
        }

        for (const MiaIA::Core::Connection& connection :
            reference.Connections)
        {
            const auto currentConnection = currentConnections.find(
                connection.Id);
            if (currentConnection == currentConnections.end() ||
                connection.FromNeuron !=
                    currentConnection->second->FromNeuron ||
                connection.ToNeuron != currentConnection->second->ToNeuron)
            {
                result.ConnectionStructureMatches = false;
            }
        }

        result.Compatible = result.LayerCountMatches &&
            result.NeuronCountMatches &&
            result.ConnectionCountMatches &&
            result.LayerStructureMatches &&
            result.NeuronStructureMatches &&
            result.ConnectionStructureMatches;
    }
}

bool MiaIA::Engine::ModelComparator::Compare(
    const Core::Network& reference,
    const Core::Network& current,
    Core::ModelComparisonSnapshot& result)
{
    if (!NetworkValidator::ValidateForForward(reference) ||
        !NetworkValidator::ValidateForForward(current))
    {
        return false;
    }

    Core::ModelComparisonSnapshot candidate;
    CompareTopology(reference, current, candidate.Topology);

    if (!candidate.Topology.Compatible)
    {
        result = std::move(candidate);
        return true;
    }

    std::map<std::uint64_t, const Core::Layer*> currentLayers;
    for (const Core::Layer& layer : current.Layers)
    {
        currentLayers.emplace(layer.Id, &layer);
    }

    candidate.Neurons.reserve(candidate.Topology.Reference.NeuronCount);
    for (const Core::Layer& referenceLayer : reference.Layers)
    {
        const Core::Layer& currentLayer =
            *currentLayers.at(referenceLayer.Id);
        if (referenceLayer.Activation != currentLayer.Activation)
        {
            ++candidate.ActivationTypeChangeCount;
        }

        std::map<std::uint64_t, const Core::Neuron*> currentNeurons;
        for (const Core::Neuron& neuron : currentLayer.Neurons)
        {
            currentNeurons.emplace(neuron.Id, &neuron);
        }

        for (const Core::Neuron& referenceNeuron :
            referenceLayer.Neurons)
        {
            const Core::Neuron& currentNeuron =
                *currentNeurons.at(referenceNeuron.Id);
            Core::ModelNeuronComparisonSnapshot comparison;
            comparison.Id = referenceNeuron.Id;
            comparison.LayerId = referenceLayer.Id;
            comparison.LayerOrder = referenceLayer.Order;
            comparison.Bias = CompareValues(
                referenceNeuron.Bias,
                currentNeuron.Bias);
            candidate.ChangedBiasCount +=
                comparison.Bias.AbsoluteDelta > 0.0 ? 1 : 0;
            candidate.Neurons.push_back(comparison);
        }
    }

    std::map<std::uint64_t, const Core::Connection*> currentConnections;
    for (const Core::Connection& connection : current.Connections)
    {
        currentConnections.emplace(connection.Id, &connection);
    }

    candidate.Connections.reserve(reference.Connections.size());
    for (const Core::Connection& referenceConnection :
        reference.Connections)
    {
        const Core::Connection& currentConnection =
            *currentConnections.at(referenceConnection.Id);
        Core::ModelConnectionComparisonSnapshot comparison;
        comparison.Id = referenceConnection.Id;
        comparison.FromNeuron = referenceConnection.FromNeuron;
        comparison.ToNeuron = referenceConnection.ToNeuron;
        comparison.Weight = CompareValues(
            referenceConnection.Weight,
            currentConnection.Weight);
        candidate.ChangedWeightCount +=
            comparison.Weight.AbsoluteDelta > 0.0 ? 1 : 0;
        candidate.Connections.push_back(comparison);
    }

    result = std::move(candidate);
    return true;
}
