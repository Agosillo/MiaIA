#include "../Include/MiaIAClient.h"
#include "../../Engine/Runtime/NetworkRuntime.h"
#include "../../Engine/Editing/NetworkEditor.h"
#include "../../Engine/Input/NetworkInput.h"
#include "../../Engine/Parameters/NetworkParameters.h"
#include "../../Engine/Weights/NetworkWeights.h"

namespace MiaIA::SDK
{
    Core::Network MiaIAClient::CurrentNetwork;

    int MiaIAClient::TestConnection()
    {
        return 1001;
    }

    void MiaIAClient::ClearNetwork()
    {
        Engine::NetworkEditor::Clear(CurrentNetwork);
    }

    bool MiaIAClient::AddLayer(std::uint64_t id, const std::string& name, std::uint64_t order)
    {
        return Engine::NetworkEditor::AddLayer(
            CurrentNetwork,
            id,
            name,
            order);
    }

    bool MiaIAClient::AddNeuron(
        std::uint64_t layerId,
        std::uint64_t neuronId,
        double bias,
        double activation)
    {
        return Engine::NetworkEditor::AddNeuron(
            CurrentNetwork,
            layerId,
            neuronId,
            bias,
            activation);
    }

    bool MiaIAClient::AddConnection(
        std::uint64_t id,
        std::uint64_t fromNeuron,
        std::uint64_t toNeuron,
        double weight)
    {
        return Engine::NetworkEditor::AddConnection(
            CurrentNetwork,
            id,
            fromNeuron,
            toNeuron,
            weight);
    }

    bool MiaIAClient::SetNeuronActivation(
        std::uint64_t neuronId,
        double activation)
    {
        return Engine::NetworkInput::SetActivation(
            CurrentNetwork,
            neuronId,
            activation);
    }

    bool MiaIAClient::SetNeuronBias(
        std::uint64_t neuronId,
        double bias)
    {
        return Engine::NetworkParameters::SetBias(
            CurrentNetwork,
            neuronId,
            bias);
    }

    bool MiaIAClient::SetConnectionWeight(
        std::uint64_t connectionId,
        double weight)
    {
        return Engine::NetworkWeights::SetWeight(
            CurrentNetwork,
            connectionId,
            weight);
    }

    bool MiaIAClient::TryGetNeuron(
        std::uint64_t neuronId,
        Core::NeuronSnapshot& result)
    {
        for (const Core::Layer& layer : CurrentNetwork.Layers)
        {
            for (const Core::Neuron& neuron : layer.Neurons)
            {
                if (neuron.Id == neuronId)
                {
                    result = Core::NeuronSnapshot{
                        neuron.Id,
                        neuron.Activation,
                        neuron.Bias
                    };

                    return true;
                }
            }
        }

        return false;
    }

    bool MiaIAClient::TryGetConnection(
        std::uint64_t connectionId,
        Core::ConnectionSnapshot& result)
    {
        for (const Core::Connection& connection : CurrentNetwork.Connections)
        {
            if (connection.Id == connectionId)
            {
                result = Core::ConnectionSnapshot{
                    connection.Id,
                    connection.FromNeuron,
                    connection.ToNeuron,
                    connection.Weight
                };

                return true;
            }
        }

        return false;
    }

    bool MiaIAClient::TryGetLayer(
        std::uint64_t layerId,
        Core::LayerSnapshot& result)
    {
        for (const Core::Layer& layer : CurrentNetwork.Layers)
        {
            if (layer.Id == layerId)
            {
                result.Id = layer.Id;
                result.Name = layer.Name;
                result.Order = layer.Order;
                result.Activation = layer.Activation;
                result.Neurons.clear();
                result.Neurons.reserve(layer.Neurons.size());

                for (const Core::Neuron& neuron : layer.Neurons)
                {
                    result.Neurons.push_back(
                        Core::NeuronSnapshot{
                            neuron.Id,
                            neuron.Activation,
                            neuron.Bias
                        });
                }

                return true;
            }
        }

        return false;
    }

    bool MiaIAClient::RemoveConnection(
        std::uint64_t id)
    {
        return Engine::NetworkEditor::RemoveConnection(
            CurrentNetwork,
            id);
    }

    bool MiaIAClient::RemoveNeuron(
        std::uint64_t neuronId)
    {
        return Engine::NetworkEditor::RemoveNeuron(
            CurrentNetwork,
            neuronId);
    }

    bool MiaIAClient::RemoveLayer(
        std::uint64_t layerId)
    {
        return Engine::NetworkEditor::RemoveLayer(
            CurrentNetwork,
            layerId);
    }

    bool MiaIAClient::SetLayerActivation(
        std::uint64_t layerId,
        Core::ActivationType activation)
    {
        for (Core::Layer& layer : CurrentNetwork.Layers)
        {
            if (layer.Id == layerId)
            {
                layer.Activation = activation;
                return true;
            }
        }

        return false;
    }

    bool MiaIAClient::Forward()
    {
        return Engine::NetworkRuntime::Forward(CurrentNetwork);
    }

    Core::NetworkSnapshot MiaIAClient::GetSnapshot()
    {
        return Engine::NetworkRuntime::Snapshot(CurrentNetwork);
    }

    bool MiaIAClient::GetConnectionWeight(
        std::uint64_t connectionId,
        double& weight)
    {
        return Engine::NetworkWeights::GetWeight(
            CurrentNetwork,
            connectionId,
            weight);
    }

    bool MiaIAClient::CreateDenseNetwork(
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


        ClearNetwork();


        std::uint64_t nextNeuronId = 1000;
        std::uint64_t nextConnectionId = 1;


        std::vector<std::uint64_t> previousLayer;


        // Input

        AddLayer(
            0,
            "Input",
            0);


        std::vector<std::uint64_t> currentLayer;


        for (int i = 0; i < inputCount; i++)
        {
            const auto id = ++nextNeuronId;

            AddNeuron(
                0,
                id,
                0.0,
                0.0);

            currentLayer.push_back(id);
        }


        previousLayer = currentLayer;


        int layerOrder = 1;


        // Hidden

        for (int h = 0; h < hiddenLayers; h++)
        {
            currentLayer.clear();

            AddLayer(
                layerOrder,
                "Hidden",
                layerOrder);


            for (int i = 0; i < hiddenCount; i++)
            {
                const auto id = ++nextNeuronId;

                AddNeuron(
                    layerOrder,
                    id,
                    0.0,
                    0.0);

                currentLayer.push_back(id);
            }


            for (auto from : previousLayer)
            {
                for (auto to : currentLayer)
                {
                    AddConnection(
                        nextConnectionId++,
                        from,
                        to,
                        0.1);
                }
            }


            previousLayer = currentLayer;
            layerOrder++;
        }


        // Output

        currentLayer.clear();


        AddLayer(
            layerOrder,
            "Output",
            layerOrder);


        for (int i = 0; i < outputCount; i++)
        {
            const auto id = ++nextNeuronId;

            AddNeuron(
                layerOrder,
                id,
                0.0,
                0.0);

            currentLayer.push_back(id);
        }


        for (auto from : previousLayer)
        {
            for (auto to : currentLayer)
            {
                AddConnection(
                    nextConnectionId++,
                    from,
                    to,
                    0.1);
            }
        }


        return true;
    }

}
