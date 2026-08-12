#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "../../../Core/Public/NetworkSnapshot.h"

namespace MiaIA::Studio
{
    enum class StudioViewMode
    {
        TwoDimensional,
        ThreeDimensional
    };

    enum class StudioTopologyDetail
    {
        Detailed,
        Compact
    };

    enum class StudioLayoutMode
    {
        Expanded,
        Packed
    };

    enum class StudioLayoutOrientation
    {
        Horizontal,
        Vertical
    };

    struct StudioLayoutPreferences
    {
        StudioLayoutMode Mode{ StudioLayoutMode::Expanded };
        StudioLayoutOrientation Orientation{
            StudioLayoutOrientation::Horizontal };
        double NeuronScale{ 1.0 };
        double NeuronGap{};
        double LayerGap{};
    };

    enum class StudioNodeKind
    {
        Neuron,
        Layer
    };

    struct StudioPosition
    {
        double X{};
        double Y{};
        double Z{};
    };

    struct StudioTopologyNode
    {
        std::uint64_t Id{};
        std::uint64_t LayerId{};
        std::uint64_t LayerOrder{};
        std::string LayerName;
        std::size_t NeuronCount{};
        StudioNodeKind Kind{ StudioNodeKind::Neuron };
        StudioPosition Position;
        double Activation{};
        double Bias{};
    };

    struct StudioTopologyLink
    {
        std::uint64_t Id{};
        std::uint64_t FromNode{};
        std::uint64_t ToNode{};
        bool Aggregate{};
        double Weight{};
    };

    struct StudioTopologyScene
    {
        StudioViewMode ViewMode{ StudioViewMode::TwoDimensional };
        StudioTopologyDetail Detail{ StudioTopologyDetail::Detailed };
        std::size_t LayerCount{};
        std::size_t NeuronCount{};
        std::size_t ConnectionCount{};
        std::vector<StudioTopologyNode> Nodes;
        std::vector<StudioTopologyLink> Links;
    };

    class StudioTopologyBuilder
    {
    public:
        static constexpr std::size_t DetailedNeuronLimit = 2000;
        static constexpr std::size_t DetailedConnectionLimit = 5000;

        [[nodiscard]]
        static bool RequiresCompactMode(
            std::size_t neuronCount,
            std::size_t connectionCount,
            std::size_t detailedNeuronLimit = DetailedNeuronLimit,
            std::size_t detailedConnectionLimit =
                DetailedConnectionLimit);

        [[nodiscard]]
        static StudioTopologyDetail ChooseDetail(
            const Core::NetworkOverviewSnapshot& overview,
            std::size_t detailedNeuronLimit = DetailedNeuronLimit,
            std::size_t detailedConnectionLimit =
                DetailedConnectionLimit);

        [[nodiscard]]
        static StudioPosition DetailedPosition2D(
            std::size_t layerIndex,
            std::size_t layerCount,
            std::size_t neuronIndex,
            std::size_t neuronCount);

        [[nodiscard]]
        static StudioPosition DetailedPosition3D(
            std::size_t layerIndex,
            std::size_t layerCount,
            std::size_t neuronIndex,
            std::size_t neuronCount);

        // Returns a deterministic, centered layout measured in neuron
        // diameters. Renderers can apply their own uniform node scale while
        // preserving the same non-overlapping topology in 2D and 3D.
        [[nodiscard]]
        static StudioPosition DetailedLayoutPosition(
            std::size_t layerIndex,
            std::size_t layerCount,
            std::size_t neuronIndex,
            std::size_t neuronCount,
            const StudioLayoutPreferences& preferences = {});

        [[nodiscard]]
        static StudioTopologyScene BuildDetailed(
            const Core::NetworkSnapshot& network,
            StudioViewMode viewMode,
            const StudioLayoutPreferences& preferences = {});

        [[nodiscard]]
        static StudioTopologyScene BuildCompact(
            const Core::NetworkOverviewSnapshot& overview,
            StudioViewMode viewMode,
            const StudioLayoutPreferences& preferences = {});
    };
}
