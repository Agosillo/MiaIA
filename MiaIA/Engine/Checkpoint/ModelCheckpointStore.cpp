#include "ModelCheckpointStore.h"

#include "../Analysis/ModelComparator.h"
#include "../Validation/NetworkValidator.h"
#include "../../Core/Execution/SnapshotBuilder.h"

#include <algorithm>
#include <limits>
#include <unordered_set>
#include <utility>

namespace
{
    bool HasText(const std::string& value)
    {
        return value.find_first_not_of(" \t\r\n") != std::string::npos;
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

}

namespace MiaIA::Engine
{
    bool ModelCheckpointStore::Capture(
        const Core::Network& network,
        const std::string& name,
        Core::ModelCheckpointSummarySnapshot& result)
    {
        if (!HasText(name) ||
            NextId == std::numeric_limits<std::uint64_t>::max() ||
            !NetworkValidator::ValidateForForward(network))
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
        Core::ModelComparisonSnapshot comparison;
        if (!ModelComparator::Compare(
            first->Network,
            second->Network,
            comparison))
        {
            return false;
        }

        candidate.TopologyCompatible = comparison.Topology.Compatible;
        candidate.ActivationTypeChangeCount =
            comparison.ActivationTypeChangeCount;
        candidate.ChangedBiasCount = comparison.ChangedBiasCount;
        candidate.ChangedWeightCount = comparison.ChangedWeightCount;
        candidate.Neurons = std::move(comparison.Neurons);
        candidate.Connections = std::move(comparison.Connections);

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

    std::vector<ModelCheckpointArchiveEntryView>
    ModelCheckpointStore::ArchiveEntries() const
    {
        std::vector<ModelCheckpointArchiveEntryView> result;
        result.reserve(Entries.size());

        for (const Entry& entry : Entries)
        {
            result.push_back({
                entry.Summary.Id,
                &entry.Summary.Name,
                &entry.Network
            });
        }

        return result;
    }

    std::uint64_t ModelCheckpointStore::NextIdentifier() const
    {
        return NextId;
    }

    bool ModelCheckpointStore::ReplaceArchiveEntries(
        std::vector<ModelCheckpointArchiveEntry> entries,
        std::uint64_t nextIdentifier)
    {
        if (nextIdentifier == 0)
        {
            return false;
        }

        std::vector<Entry> replacements;
        replacements.reserve(entries.size());
        std::unordered_set<std::uint64_t> identifiers;
        std::uint64_t maximumIdentifier{};

        for (ModelCheckpointArchiveEntry& source : entries)
        {
            if (source.Id == 0 || source.Id >= nextIdentifier ||
                !identifiers.insert(source.Id).second ||
                !HasText(source.Name) ||
                !NetworkValidator::ValidateForForward(source.Network))
            {
                return false;
            }

            Entry entry;
            entry.Summary.Id = source.Id;
            entry.Summary.Name = std::move(source.Name);
            entry.Summary.LayerCount = source.Network.Layers.size();
            entry.Summary.NeuronCount = NeuronCount(source.Network);
            entry.Summary.ConnectionCount =
                source.Network.Connections.size();
            entry.Network = std::move(source.Network);
            maximumIdentifier = std::max(maximumIdentifier, entry.Summary.Id);
            replacements.push_back(std::move(entry));
        }

        if (maximumIdentifier >= nextIdentifier)
        {
            return false;
        }

        Entries = std::move(replacements);
        NextId = nextIdentifier;
        return true;
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
