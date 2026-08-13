#include "ModelCheckpointStore.h"

#include "../Validation/NetworkValidator.h"
#include "../../Core/Execution/SnapshotBuilder.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <utility>

namespace
{
    bool HasText(const std::string& value)
    {
        return value.find_first_not_of(" \t\r\n") != std::string::npos;
    }

    MiaIA::Core::ModelCheckpointValueComparisonSnapshot CompareValues(
        double first,
        double second)
    {
        MiaIA::Core::ModelCheckpointValueComparisonSnapshot result;
        result.FirstValue = first;
        result.SecondValue = second;
        result.Delta = second - first;
        result.AbsoluteDelta = std::abs(result.Delta);
        return result;
    }

    std::size_t NeuronCount(const MiaIA::Core::Network& network)
    {
        std::size_t result{};
        for (const MiaIA::Core::Layer& layer : network.Layers)
        {
            result += layer.Neurons.size();
        }
        return result;
    }

    bool HasCompatibleTopology(
        const MiaIA::Core::Network& first,
        const MiaIA::Core::Network& second)
    {
        if (first.Layers.size() != second.Layers.size() ||
            first.Connections.size() != second.Connections.size())
        {
            return false;
        }

        std::map<std::uint64_t, const MiaIA::Core::Layer*> secondLayers;
        for (const auto& layer : second.Layers)
        {
            if (!secondLayers.emplace(layer.Id, &layer).second)
            {
                return false;
            }
        }

        for (const auto& firstLayer : first.Layers)
        {
            const auto secondLayerIterator = secondLayers.find(firstLayer.Id);
            if (secondLayerIterator == secondLayers.end())
            {
                return false;
            }
            const auto& secondLayer = *secondLayerIterator->second;
            if (firstLayer.Order != secondLayer.Order ||
                firstLayer.Neurons.size() != secondLayer.Neurons.size())
            {
                return false;
            }

            std::map<std::uint64_t, const MiaIA::Core::Neuron*> secondNeurons;
            for (const auto& neuron : secondLayer.Neurons)
            {
                if (!secondNeurons.emplace(neuron.Id, &neuron).second)
                {
                    return false;
                }
            }

            for (const auto& neuron : firstLayer.Neurons)
            {
                if (secondNeurons.find(neuron.Id) == secondNeurons.end())
                {
                    return false;
                }
            }
        }

        std::map<std::uint64_t, const MiaIA::Core::Connection*>
            secondConnections;
        for (const auto& connection : second.Connections)
        {
            if (!secondConnections.emplace(connection.Id, &connection).second)
            {
                return false;
            }
        }

        for (const auto& firstConnection : first.Connections)
        {
            const auto iterator = secondConnections.find(firstConnection.Id);
            if (iterator == secondConnections.end() ||
                firstConnection.FromNeuron != iterator->second->FromNeuron ||
                firstConnection.ToNeuron != iterator->second->ToNeuron)
            {
                return false;
            }
        }

        return true;
    }
}

namespace MiaIA::Engine
{
    bool ModelCheckpointStore::Capture(
        const Core::Network& network,
        const std::string& name,
        Core::ModelCheckpointSummarySnapshot& result)
    {
        if (!HasText(name) || !NetworkValidator::ValidateForForward(network))
        {
            return false;
        }

        Entry entry;
        entry.Summary.Id = NextId++;
        entry.Summary.Name = name;
        entry.Summary.LayerCount = network.Layers.size();
        entry.Summary.NeuronCount = NeuronCount(network);
        entry.Summary.ConnectionCount = network.Connections.size();
        entry.Network = network;
        result = entry.Summary;
        Entries.push_back(std::move(entry));
        return true;
    }

    std::vector<Core::ModelCheckpointSummarySnapshot>
    ModelCheckpointStore::List() const
    {
        std::vector<Core::ModelCheckpointSummarySnapshot> result;
        result.reserve(Entries.size());
        for (const Entry& entry : Entries)
        {
            result.push_back(entry.Summary);
        }
        return result;
    }

    bool ModelCheckpointStore::TryGet(
        std::uint64_t checkpointId,
        Core::ModelCheckpointSnapshot& result) const
    {
        const Entry* entry = Find(checkpointId);
        if (!entry)
        {
            return false;
        }

        Core::ModelCheckpointSnapshot candidate;
        candidate.Summary = entry->Summary;
        candidate.Network = Core::SnapshotBuilder::Build(entry->Network);
        result = std::move(candidate);
        return true;
    }

    bool ModelCheckpointStore::Compare(
        std::uint64_t firstCheckpointId,
        std::uint64_t secondCheckpointId,
        Core::ModelCheckpointComparisonSnapshot& result) const
    {
        const Entry* first = Find(firstCheckpointId);
        const Entry* second = Find(secondCheckpointId);
        if (!first || !second)
        {
            return false;
        }

        Core::ModelCheckpointComparisonSnapshot candidate;
        candidate.FirstCheckpointId = first->Summary.Id;
        candidate.SecondCheckpointId = second->Summary.Id;
        candidate.FirstCheckpointName = first->Summary.Name;
        candidate.SecondCheckpointName = second->Summary.Name;
        candidate.TopologyCompatible = HasCompatibleTopology(
            first->Network,
            second->Network);

        if (!candidate.TopologyCompatible)
        {
            result = std::move(candidate);
            return true;
        }

        std::map<std::uint64_t, const Core::Layer*> secondLayers;
        for (const auto& layer : second->Network.Layers)
        {
            secondLayers.emplace(layer.Id, &layer);
        }

        for (const auto& firstLayer : first->Network.Layers)
        {
            const auto& secondLayer = *secondLayers.at(firstLayer.Id);
            if (firstLayer.Activation != secondLayer.Activation)
            {
                ++candidate.ActivationTypeChangeCount;
            }

            std::map<std::uint64_t, const Core::Neuron*> secondNeurons;
            for (const auto& neuron : secondLayer.Neurons)
            {
                secondNeurons.emplace(neuron.Id, &neuron);
            }

            for (const auto& firstNeuron : firstLayer.Neurons)
            {
                const auto& secondNeuron = *secondNeurons.at(firstNeuron.Id);
                Core::ModelCheckpointNeuronComparisonSnapshot comparison;
                comparison.Id = firstNeuron.Id;
                comparison.LayerOrder = firstLayer.Order;
                comparison.Bias = CompareValues(
                    firstNeuron.Bias,
                    secondNeuron.Bias);
                candidate.ChangedBiasCount +=
                    comparison.Bias.AbsoluteDelta > 0.0 ? 1 : 0;
                candidate.Neurons.push_back(comparison);
            }
        }

        std::map<std::uint64_t, const Core::Connection*> secondConnections;
        for (const auto& connection : second->Network.Connections)
        {
            secondConnections.emplace(connection.Id, &connection);
        }

        for (const auto& firstConnection : first->Network.Connections)
        {
            const auto& secondConnection =
                *secondConnections.at(firstConnection.Id);
            Core::ModelCheckpointConnectionComparisonSnapshot comparison;
            comparison.Id = firstConnection.Id;
            comparison.FromNeuron = firstConnection.FromNeuron;
            comparison.ToNeuron = firstConnection.ToNeuron;
            comparison.Weight = CompareValues(
                firstConnection.Weight,
                secondConnection.Weight);
            candidate.ChangedWeightCount +=
                comparison.Weight.AbsoluteDelta > 0.0 ? 1 : 0;
            candidate.Connections.push_back(comparison);
        }

        result = std::move(candidate);
        return true;
    }

    bool ModelCheckpointStore::TryRestore(
        std::uint64_t checkpointId,
        Core::Network& result) const
    {
        const Entry* entry = Find(checkpointId);
        if (!entry)
        {
            return false;
        }

        Core::Network candidate = entry->Network;
        if (!NetworkValidator::ValidateForForward(candidate))
        {
            return false;
        }

        result = std::move(candidate);
        return true;
    }

    bool ModelCheckpointStore::Remove(std::uint64_t checkpointId)
    {
        const auto iterator = std::find_if(
            Entries.begin(),
            Entries.end(),
            [checkpointId](const Entry& entry)
            {
                return entry.Summary.Id == checkpointId;
            });
        if (iterator == Entries.end())
        {
            return false;
        }

        Entries.erase(iterator);
        return true;
    }

    void ModelCheckpointStore::Clear()
    {
        Entries.clear();
    }

    const ModelCheckpointStore::Entry* ModelCheckpointStore::Find(
        std::uint64_t checkpointId) const
    {
        const auto iterator = std::find_if(
            Entries.begin(),
            Entries.end(),
            [checkpointId](const Entry& entry)
            {
                return entry.Summary.Id == checkpointId;
            });
        return iterator == Entries.end() ? nullptr : &*iterator;
    }
}
