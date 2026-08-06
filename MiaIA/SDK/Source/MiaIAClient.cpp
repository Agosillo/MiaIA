#include "../Include/MiaIAClient.h"
#include "../../Engine/Runtime/NetworkRuntime.h"
#include "../../Engine/Runtime/NetworkFactory.h"
#include "../../Engine/Inspection/NetworkInspector.h"
#include "../../Engine/Editing/NetworkEditor.h"
#include "../../Engine/Input/NetworkInput.h"
#include "../../Engine/Parameters/NetworkParameters.h"
#include "../../Engine/Weights/NetworkWeights.h"
#include "../../Core/Model/Network.h"

namespace
{
    MiaIA::Core::Network CurrentNetwork;
}

namespace MiaIA::SDK
{
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

    bool MiaIAClient::SetInputValues(
        const std::vector<double>& values)
    {
        return Engine::NetworkInput::SetValues(
            CurrentNetwork,
            values);
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
        return Engine::NetworkInspector::TryGetNeuron(
            CurrentNetwork,
            neuronId,
            result);
    }

    bool MiaIAClient::TryGetConnection(
        std::uint64_t connectionId,
        Core::ConnectionSnapshot& result)
    {
        return Engine::NetworkInspector::TryGetConnection(
            CurrentNetwork,
            connectionId,
            result);
    }

    bool MiaIAClient::TryGetLayer(
        std::uint64_t layerId,
        Core::LayerSnapshot& result)
    {
        return Engine::NetworkInspector::TryGetLayer(
            CurrentNetwork,
            layerId,
            result);
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
        return Engine::NetworkParameters::SetLayerActivation(
            CurrentNetwork,
            layerId,
            activation);
    }

    bool MiaIAClient::Forward()
    {
        return Engine::NetworkRuntime::Forward(CurrentNetwork);
    }

    Core::NetworkSnapshot MiaIAClient::GetSnapshot()
    {
        return Engine::NetworkInspector::Snapshot(CurrentNetwork);
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
        return Engine::NetworkFactory::CreateDense(
            CurrentNetwork,
            inputCount,
            hiddenCount,
            hiddenLayers,
            outputCount);
    }

}
