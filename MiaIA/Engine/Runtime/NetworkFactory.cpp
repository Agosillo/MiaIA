#include "NetworkFactory.h"
#include "../Validation/NetworkValidator.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <stdexcept>
#include <utility>
#include <vector>

namespace
{
    bool CheckedAdd(
        std::size_t left,
        std::size_t right,
        std::size_t& result)
    {
        if (left > std::numeric_limits<std::size_t>::max() - right)
        {
            return false;
        }

        result = left + right;
        return true;
    }

    bool CheckedMultiply(
        std::size_t left,
        std::size_t right,
        std::size_t& result)
    {
        if (left != 0 &&
            right > std::numeric_limits<std::size_t>::max() / left)
        {
            return false;
        }

        result = left * right;
        return true;
    }

    bool CalculateDenseSizes(
        std::size_t inputCount,
        std::size_t hiddenCount,
        std::size_t hiddenLayers,
        std::size_t outputCount,
        std::size_t& neuronCount,
        std::size_t& connectionCount)
    {
        std::size_t hiddenNeuronCount{};
        if (!CheckedMultiply(
            hiddenCount,
            hiddenLayers,
            hiddenNeuronCount) ||
            !CheckedAdd(inputCount, hiddenNeuronCount, neuronCount) ||
            !CheckedAdd(neuronCount, outputCount, neuronCount))
        {
            return false;
        }

        if (hiddenLayers == 0)
        {
            return CheckedMultiply(
                inputCount,
                outputCount,
                connectionCount);
        }

        std::size_t inputConnections{};
        std::size_t hiddenConnections{};
        std::size_t outputConnections{};
        std::size_t hiddenLayerPairs = hiddenLayers - 1;
        std::size_t hiddenLayerWidthSquared{};

        if (!CheckedMultiply(
                inputCount,
                hiddenCount,
                inputConnections) ||
            !CheckedMultiply(
                hiddenCount,
                hiddenCount,
                hiddenLayerWidthSquared) ||
            !CheckedMultiply(
                hiddenLayerPairs,
                hiddenLayerWidthSquared,
                hiddenConnections) ||
            !CheckedMultiply(
                hiddenCount,
                outputCount,
                outputConnections) ||
            !CheckedAdd(
                inputConnections,
                hiddenConnections,
                connectionCount) ||
            !CheckedAdd(
                connectionCount,
                outputConnections,
                connectionCount))
        {
            return false;
        }

        return true;
    }

    bool IsSupportedActivation(MiaIA::Core::ActivationType activation)
    {
        switch (activation)
        {
        case MiaIA::Core::ActivationType::Sigmoid:
        case MiaIA::Core::ActivationType::ReLU:
        case MiaIA::Core::ActivationType::Tanh:
        case MiaIA::Core::ActivationType::Linear:
            return true;
        }

        return false;
    }
}

namespace MiaIA::Engine
{
    Core::Network NetworkFactory::Create()
    {
        return Core::Network{};
    }

    bool NetworkFactory::CreateDense(
        Core::Network& network,
        int inputCount,
        int hiddenCount,
        int hiddenLayers,
        int outputCount)
    {
        return CreateDense(
            network,
            inputCount,
            hiddenCount,
            hiddenLayers,
            outputCount,
            Core::DenseNetworkConfiguration{});
    }

    bool NetworkFactory::CreateDense(
        Core::Network& network,
        int inputCount,
        int hiddenCount,
        int hiddenLayers,
        int outputCount,
        const Core::DenseNetworkConfiguration& configuration)
    {
        if (inputCount <= 0 ||
            hiddenCount <= 0 ||
            hiddenLayers < 0 ||
            outputCount <= 0 ||
            !IsSupportedActivation(configuration.HiddenActivation) ||
            !IsSupportedActivation(configuration.OutputActivation) ||
            !std::isfinite(configuration.InitialWeight) ||
            !std::isfinite(configuration.InitialBias))
        {
            return false;
        }

        const std::size_t inputs = static_cast<std::size_t>(inputCount);
        const std::size_t hiddenWidth =
            static_cast<std::size_t>(hiddenCount);
        const std::size_t hiddenLayerCount =
            static_cast<std::size_t>(hiddenLayers);
        const std::size_t outputs = static_cast<std::size_t>(outputCount);
        std::size_t neuronCount{};
        std::size_t connectionCount{};

        if (!CalculateDenseSizes(
                inputs,
                hiddenWidth,
                hiddenLayerCount,
                outputs,
                neuronCount,
                connectionCount) ||
            neuronCount > std::numeric_limits<std::uint64_t>::max() - 1000 ||
            connectionCount >
                std::numeric_limits<std::uint64_t>::max() - 1)
        {
            return false;
        }

        try
        {
            Core::Network denseNetwork = Create();
            denseNetwork.Layers.reserve(hiddenLayerCount + 2);
            denseNetwork.Connections.reserve(connectionCount);

            std::uint64_t nextNeuronId = 1000;
            std::uint64_t nextConnectionId = 1;
            std::uint64_t layerOrder = 0;
            std::vector<std::uint64_t> previousLayer;
            std::vector<std::uint64_t> currentLayer;

            const auto appendLayer = [&denseNetwork, &nextNeuronId](
                std::uint64_t id,
                const char* name,
                std::size_t count,
                Core::ActivationType activation,
                double bias,
                std::vector<std::uint64_t>& ids)
            {
                Core::Layer layer;
                layer.Id = id;
                layer.Name = name;
                layer.Order = id;
                layer.Activation = activation;
                layer.Neurons.reserve(count);
                ids.clear();
                ids.reserve(count);

                for (std::size_t index = 0; index < count; ++index)
                {
                    const std::uint64_t neuronId = ++nextNeuronId;
                    layer.Neurons.push_back(Core::Neuron{
                        neuronId,
                        bias,
                        0.0
                    });
                    ids.push_back(neuronId);
                }

                denseNetwork.Layers.push_back(std::move(layer));
            };

            const auto appendConnections =
                [&denseNetwork, &nextConnectionId, &configuration](
                    const std::vector<std::uint64_t>& fromLayer,
                    const std::vector<std::uint64_t>& toLayer)
            {
                for (const std::uint64_t fromNeuron : fromLayer)
                {
                    for (const std::uint64_t toNeuron : toLayer)
                    {
                        denseNetwork.Connections.push_back(Core::Connection{
                            nextConnectionId++,
                            fromNeuron,
                            toNeuron,
                            configuration.InitialWeight
                        });
                    }
                }
            };

            appendLayer(
                layerOrder,
                "Input",
                inputs,
                Core::ActivationType::Sigmoid,
                0.0,
                currentLayer);
            previousLayer = currentLayer;

            for (std::size_t hiddenIndex = 0;
                hiddenIndex < hiddenLayerCount;
                ++hiddenIndex)
            {
                appendLayer(
                    ++layerOrder,
                    "Hidden",
                    hiddenWidth,
                    configuration.HiddenActivation,
                    configuration.InitialBias,
                    currentLayer);
                appendConnections(previousLayer, currentLayer);
                previousLayer = currentLayer;
            }

            appendLayer(
                ++layerOrder,
                "Output",
                outputs,
                configuration.OutputActivation,
                configuration.InitialBias,
                currentLayer);
            appendConnections(previousLayer, currentLayer);

            if (!NetworkValidator::ValidateForForward(denseNetwork))
            {
                return false;
            }

            network = std::move(denseNetwork);
            return true;
        }
        catch (const std::bad_alloc&)
        {
            return false;
        }
        catch (const std::length_error&)
        {
            return false;
        }
    }
}
