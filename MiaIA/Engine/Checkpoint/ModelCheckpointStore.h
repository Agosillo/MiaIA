#pragma once

#include "../../Core/Model/Network.h"
#include "../../Core/Public/ModelCheckpointSnapshot.h"

#include <cstdint>
#include <string>
#include <vector>

namespace MiaIA::Engine
{
    class ModelCheckpointStore final
    {
    public:
        bool Capture(
            const Core::Network& network,
            const std::string& name,
            Core::ModelCheckpointSummarySnapshot& result);

        [[nodiscard]]
        std::vector<Core::ModelCheckpointSummarySnapshot> List() const;

        bool TryGet(
            std::uint64_t checkpointId,
            Core::ModelCheckpointSnapshot& result) const;

        bool Compare(
            std::uint64_t firstCheckpointId,
            std::uint64_t secondCheckpointId,
            Core::ModelCheckpointComparisonSnapshot& result) const;

        bool TryRestore(
            std::uint64_t checkpointId,
            Core::Network& result) const;

        bool Remove(std::uint64_t checkpointId);
        void Clear();

    private:
        struct Entry
        {
            Core::ModelCheckpointSummarySnapshot Summary;
            Core::Network Network;
        };

        const Entry* Find(std::uint64_t checkpointId) const;

        std::vector<Entry> Entries;
        std::uint64_t NextId{ 1 };
    };
}
