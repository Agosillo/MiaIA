#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace MiaIA::Core
{
    struct ModelTopologySummarySnapshot
    {
        std::size_t LayerCount{};
        std::size_t NeuronCount{};
        std::size_t ConnectionCount{};
    };

    struct ModelTopologyCompatibilitySnapshot
    {
        ModelTopologySummarySnapshot Reference;
        ModelTopologySummarySnapshot Current;
        bool LayerCountMatches{};
        bool NeuronCountMatches{};
        bool ConnectionCountMatches{};
        bool LayerStructureMatches{};
        bool NeuronStructureMatches{};
        bool ConnectionStructureMatches{};
        bool Compatible{};
    };

    struct ModelValueComparisonSnapshot
    {
        double FirstValue{};
        double SecondValue{};
        double Delta{};
        double AbsoluteDelta{};
    };

    struct ModelNeuronComparisonSnapshot
    {
        std::uint64_t Id{};
        std::uint64_t LayerId{};
        std::uint64_t LayerOrder{};
        ModelValueComparisonSnapshot Bias;
    };

    struct ModelConnectionComparisonSnapshot
    {
        std::uint64_t Id{};
        std::uint64_t FromNeuron{};
        std::uint64_t ToNeuron{};
        ModelValueComparisonSnapshot Weight;
    };

    struct ModelComparisonSnapshot
    {
        ModelTopologyCompatibilitySnapshot Topology;
        std::size_t ActivationTypeChangeCount{};
        std::size_t ChangedBiasCount{};
        std::size_t ChangedWeightCount{};
        std::vector<ModelNeuronComparisonSnapshot> Neurons;
        std::vector<ModelConnectionComparisonSnapshot> Connections;
    };

    struct ModelContextComparisonSnapshot
    {
        std::uint64_t ReferenceContextId{};
        std::uint64_t CurrentContextId{};
        std::string ReferenceContextName;
        std::string CurrentContextName;
        ModelComparisonSnapshot Model;
    };
}
