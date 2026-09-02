#pragma once

#include "ModelComparisonSnapshot.h"
#include "NetworkSnapshot.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace MiaIA::Core
{
    struct ModelCheckpointSummarySnapshot
    {
        std::uint64_t Id{};
        std::string Name;
        std::size_t LayerCount{};
        std::size_t NeuronCount{};
        std::size_t ConnectionCount{};
    };

    struct ModelCheckpointSnapshot
    {
        ModelCheckpointSummarySnapshot Summary;
        NetworkSnapshot Network;
    };

    using ModelCheckpointValueComparisonSnapshot =
        ModelValueComparisonSnapshot;
    using ModelCheckpointNeuronComparisonSnapshot =
        ModelNeuronComparisonSnapshot;
    using ModelCheckpointConnectionComparisonSnapshot =
        ModelConnectionComparisonSnapshot;

    struct ModelCheckpointComparisonSnapshot
    {
        std::uint64_t FirstCheckpointId{};
        std::uint64_t SecondCheckpointId{};
        std::string FirstCheckpointName;
        std::string SecondCheckpointName;
        bool TopologyCompatible{};
        std::size_t ActivationTypeChangeCount{};
        std::size_t ChangedBiasCount{};
        std::size_t ChangedWeightCount{};
        std::vector<ModelCheckpointNeuronComparisonSnapshot> Neurons;
        std::vector<ModelCheckpointConnectionComparisonSnapshot> Connections;
    };
}
