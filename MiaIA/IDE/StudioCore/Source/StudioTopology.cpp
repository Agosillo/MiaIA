#include "../Include/StudioTopology.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <utility>

namespace
{
    double NormalizedIndex(std::size_t index, std::size_t count)
    {
        return count <= 1
            ? 0.5
            : static_cast<double>(index) /
                static_cast<double>(count - 1);
    }

    std::size_t CountNeurons(const MiaIA::Core::NetworkSnapshot& network)
    {
        std::size_t count{};

        for (const auto& layer : network.Layers)
        {
            count += layer.Neurons.size();
        }

        return count;
    }
}

bool MiaIA::Studio::StudioTopologyBuilder::RequiresCompactMode(
    std::size_t neuronCount,
    std::size_t connectionCount)
{
    return neuronCount > DetailedNeuronLimit ||
        connectionCount > DetailedConnectionLimit;
}

MiaIA::Studio::StudioTopologyDetail
MiaIA::Studio::StudioTopologyBuilder::ChooseDetail(
    const Core::NetworkOverviewSnapshot& overview)
{
    return RequiresCompactMode(
        overview.NeuronCount,
        overview.ConnectionCount)
        ? StudioTopologyDetail::Compact
        : StudioTopologyDetail::Detailed;
}

MiaIA::Studio::StudioPosition
MiaIA::Studio::StudioTopologyBuilder::DetailedPosition2D(
    std::size_t layerIndex,
    std::size_t layerCount,
    std::size_t neuronIndex,
    std::size_t neuronCount)
{
    return {
        NormalizedIndex(layerIndex, layerCount),
        NormalizedIndex(neuronIndex, neuronCount),
        0.0
    };
}

MiaIA::Studio::StudioPosition
MiaIA::Studio::StudioTopologyBuilder::DetailedPosition3D(
    std::size_t layerIndex,
    std::size_t layerCount,
    std::size_t neuronIndex,
    std::size_t neuronCount)
{
    if (neuronCount == 0)
    {
        return { 0.5, 0.5, NormalizedIndex(layerIndex, layerCount) };
    }

    const std::size_t columns = std::max<std::size_t>(
        1,
        static_cast<std::size_t>(std::ceil(
            std::sqrt(static_cast<double>(neuronCount)))));
    const std::size_t rows = (neuronCount + columns - 1) / columns;
    const std::size_t column = neuronIndex % columns;
    const std::size_t row = neuronIndex / columns;

    return {
        NormalizedIndex(column, columns),
        NormalizedIndex(row, rows),
        NormalizedIndex(layerIndex, layerCount)
    };
}

MiaIA::Studio::StudioTopologyScene
MiaIA::Studio::StudioTopologyBuilder::BuildDetailed(
    const Core::NetworkSnapshot& network,
    StudioViewMode viewMode)
{
    StudioTopologyScene scene;
    scene.ViewMode = viewMode;
    scene.Detail = StudioTopologyDetail::Detailed;
    scene.LayerCount = network.Layers.size();
    scene.NeuronCount = CountNeurons(network);
    scene.ConnectionCount = network.Connections.size();
    scene.Nodes.reserve(scene.NeuronCount);
    scene.Links.reserve(scene.ConnectionCount);

    std::unordered_map<std::uint64_t, std::uint64_t> knownNeurons;
    knownNeurons.reserve(scene.NeuronCount);

    for (std::size_t layerIndex = 0;
        layerIndex < network.Layers.size();
        ++layerIndex)
    {
        const Core::LayerSnapshot& layer = network.Layers[layerIndex];

        for (std::size_t neuronIndex = 0;
            neuronIndex < layer.Neurons.size();
            ++neuronIndex)
        {
            const Core::NeuronSnapshot& neuron = layer.Neurons[neuronIndex];
            StudioTopologyNode node;
            node.Id = neuron.Id;
            node.LayerId = layer.Id;
            node.LayerOrder = layer.Order;
            node.LayerName = layer.Name;
            node.NeuronCount = 1;
            node.Kind = StudioNodeKind::Neuron;
            node.Position = viewMode == StudioViewMode::TwoDimensional
                ? DetailedPosition2D(
                    layerIndex,
                    network.Layers.size(),
                    neuronIndex,
                    layer.Neurons.size())
                : DetailedPosition3D(
                    layerIndex,
                    network.Layers.size(),
                    neuronIndex,
                    layer.Neurons.size());
            node.Activation = neuron.Activation;
            node.Bias = neuron.Bias;
            scene.Nodes.push_back(std::move(node));
            knownNeurons.emplace(neuron.Id, neuron.Id);
        }
    }

    for (const Core::ConnectionSnapshot& connection : network.Connections)
    {
        if (!knownNeurons.contains(connection.FromNeuron) ||
            !knownNeurons.contains(connection.ToNeuron))
        {
            continue;
        }

        scene.Links.push_back({
            connection.Id,
            connection.FromNeuron,
            connection.ToNeuron,
            false,
            connection.Weight
        });
    }

    return scene;
}

MiaIA::Studio::StudioTopologyScene
MiaIA::Studio::StudioTopologyBuilder::BuildCompact(
    const Core::NetworkOverviewSnapshot& overview,
    StudioViewMode viewMode)
{
    StudioTopologyScene scene;
    scene.ViewMode = viewMode;
    scene.Detail = StudioTopologyDetail::Compact;
    scene.LayerCount = overview.Layers.size();
    scene.NeuronCount = overview.NeuronCount;
    scene.ConnectionCount = overview.ConnectionCount;
    scene.Nodes.reserve(scene.LayerCount);

    if (scene.LayerCount > 1)
    {
        scene.Links.reserve(scene.LayerCount - 1);
    }

    for (std::size_t layerIndex = 0;
        layerIndex < overview.Layers.size();
        ++layerIndex)
    {
        const Core::LayerOverviewSnapshot& layer =
            overview.Layers[layerIndex];
        const double progress = NormalizedIndex(
            layerIndex,
            overview.Layers.size());
        StudioTopologyNode node;
        node.Id = layer.Id;
        node.LayerId = layer.Id;
        node.LayerOrder = layer.Order;
        node.LayerName = layer.Name;
        node.NeuronCount = layer.NeuronCount;
        node.Kind = StudioNodeKind::Layer;
        node.Position = viewMode == StudioViewMode::TwoDimensional
            ? StudioPosition{ progress, 0.5, 0.0 }
            : StudioPosition{ 0.5, 0.5, progress };
        scene.Nodes.push_back(std::move(node));

        if (layerIndex > 0)
        {
            scene.Links.push_back({
                0,
                overview.Layers[layerIndex - 1].Id,
                layer.Id,
                true,
                0.0
            });
        }
    }

    return scene;
}
