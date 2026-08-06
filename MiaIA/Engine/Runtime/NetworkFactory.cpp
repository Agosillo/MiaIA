#include "NetworkFactory.h"
#include "../Editing/NetworkEditor.h"

#include <cstdint>
#include <utility>
#include <vector>

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
        if (inputCount <= 0 ||
            hiddenCount <= 0 ||
            hiddenLayers < 0 ||
            outputCount <= 0)
        {
            return false;
        }

        Core::Network denseNetwork = Create();

        std::uint64_t nextNeuronId = 1000;
        std::uint64_t nextConnectionId = 1;
        std::uint64_t layerOrder = 0;

        std::vector<std::uint64_t> previousLayer;
        std::vector<std::uint64_t> currentLayer;

        if (!NetworkEditor::AddLayer(
            denseNetwork,
            layerOrder,
            "Input",
            layerOrder))
        {
            return false;
        }

        currentLayer.reserve(inputCount);

        for (int index = 0; index < inputCount; ++index)
        {
            const std::uint64_t neuronId = ++nextNeuronId;

            if (!NetworkEditor::AddNeuron(
                denseNetwork,
                layerOrder,
                neuronId,
                0.0,
                0.0))
            {
                return false;
            }

            currentLayer.push_back(neuronId);
        }

        previousLayer = currentLayer;

        for (int hiddenIndex = 0;
            hiddenIndex < hiddenLayers;
            ++hiddenIndex)
        {
            ++layerOrder;
            currentLayer.clear();
            currentLayer.reserve(hiddenCount);

            if (!NetworkEditor::AddLayer(
                denseNetwork,
                layerOrder,
                "Hidden",
                layerOrder))
            {
                return false;
            }

            for (int neuronIndex = 0;
                neuronIndex < hiddenCount;
                ++neuronIndex)
            {
                const std::uint64_t neuronId = ++nextNeuronId;

                if (!NetworkEditor::AddNeuron(
                    denseNetwork,
                    layerOrder,
                    neuronId,
                    0.0,
                    0.0))
                {
                    return false;
                }

                currentLayer.push_back(neuronId);
            }

            for (const std::uint64_t fromNeuron : previousLayer)
            {
                for (const std::uint64_t toNeuron : currentLayer)
                {
                    if (!NetworkEditor::AddConnection(
                        denseNetwork,
                        nextConnectionId++,
                        fromNeuron,
                        toNeuron,
                        0.1))
                    {
                        return false;
                    }
                }
            }

            previousLayer = currentLayer;
        }

        ++layerOrder;
        currentLayer.clear();
        currentLayer.reserve(outputCount);

        if (!NetworkEditor::AddLayer(
            denseNetwork,
            layerOrder,
            "Output",
            layerOrder))
        {
            return false;
        }

        for (int index = 0; index < outputCount; ++index)
        {
            const std::uint64_t neuronId = ++nextNeuronId;

            if (!NetworkEditor::AddNeuron(
                denseNetwork,
                layerOrder,
                neuronId,
                0.0,
                0.0))
            {
                return false;
            }

            currentLayer.push_back(neuronId);
        }

        for (const std::uint64_t fromNeuron : previousLayer)
        {
            for (const std::uint64_t toNeuron : currentLayer)
            {
                if (!NetworkEditor::AddConnection(
                    denseNetwork,
                    nextConnectionId++,
                    fromNeuron,
                    toNeuron,
                    0.1))
                {
                    return false;
                }
            }
        }

        network = std::move(denseNetwork);

        return true;
    }
}
