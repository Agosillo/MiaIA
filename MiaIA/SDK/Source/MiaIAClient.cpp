#include "../Include/MiaIAClient.h"
#include "MiaIAClientState.h"
#include "../../Engine/Runtime/NetworkRuntime.h"
#include "../../Engine/Runtime/NetworkFactory.h"
#include "../../Engine/Inspection/NetworkInspector.h"
#include "../../Engine/Editing/NetworkEditor.h"
#include "../../Engine/Input/NetworkInput.h"
#include "../../Engine/Parameters/NetworkParameters.h"
#include "../../Engine/Weights/NetworkWeights.h"
#include "../../Core/Model/Network.h"
#include "../../Core/Model/Dataset.h"
#include "../../Core/Model/TrainingSession.h"
#include "../../Core/Model/TrainingDebugSession.h"

#include <mutex>

namespace
{
    MiaIA::Core::Dataset CurrentDataset;
    MiaIA::Core::Network CurrentNetwork;
    MiaIA::Core::TrainingSession CurrentTrainingSession;
    MiaIA::Core::TrainingDebugSession CurrentTrainingDebugSession;
    std::mutex CurrentClientMutex;
}

MiaIA::Core::Dataset& MiaIA::SDK::Detail::ClientDataset()
{
    return CurrentDataset;
}

MiaIA::Core::Network& MiaIA::SDK::Detail::ClientNetwork()
{
    return CurrentNetwork;
}

MiaIA::Core::TrainingSession&
MiaIA::SDK::Detail::ClientTrainingSession()
{
    return CurrentTrainingSession;
}

MiaIA::Core::TrainingDebugSession&
MiaIA::SDK::Detail::ClientTrainingDebugSession()
{
    return CurrentTrainingDebugSession;
}

std::mutex& MiaIA::SDK::Detail::ClientMutex()
{
    return CurrentClientMutex;
}

bool MiaIA::SDK::Detail::IsTrainingSessionRunning()
{
    return CurrentTrainingSession.Status ==
        MiaIA::Core::TrainingSessionStatus::Running;
}

bool MiaIA::SDK::Detail::IsTrainingDebugActive()
{
    return CurrentTrainingDebugSession.Phase >=
        MiaIA::Core::TrainingDebugPhase::BeforeForward &&
        CurrentTrainingDebugSession.Phase <
        MiaIA::Core::TrainingDebugPhase::Committed;
}

bool MiaIA::SDK::Detail::IsClientMutationBlocked()
{
    return IsTrainingSessionRunning() || IsTrainingDebugActive();
}

namespace MiaIA::SDK
{
    bool MiaIAClient::ClearNetwork()
    {
        const std::scoped_lock lock(Detail::ClientMutex());

        if (Detail::IsClientMutationBlocked())
        {
            return false;
        }

        Engine::NetworkEditor::Clear(CurrentNetwork);
        return true;
    }

    bool MiaIAClient::AddLayer(std::uint64_t id, const std::string& name, std::uint64_t order)
    {
        const std::scoped_lock lock(Detail::ClientMutex());

        if (Detail::IsClientMutationBlocked())
        {
            return false;
        }

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
        const std::scoped_lock lock(Detail::ClientMutex());

        if (Detail::IsClientMutationBlocked())
        {
            return false;
        }

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
        const std::scoped_lock lock(Detail::ClientMutex());

        if (Detail::IsClientMutationBlocked())
        {
            return false;
        }

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
        const std::scoped_lock lock(Detail::ClientMutex());

        if (Detail::IsClientMutationBlocked())
        {
            return false;
        }

        return Engine::NetworkInput::SetActivation(
            CurrentNetwork,
            neuronId,
            activation);
    }

    bool MiaIAClient::SetInputValues(
        const std::vector<double>& values)
    {
        const std::scoped_lock lock(Detail::ClientMutex());

        if (Detail::IsClientMutationBlocked())
        {
            return false;
        }

        return Engine::NetworkInput::SetValues(
            CurrentNetwork,
            values);
    }

    bool MiaIAClient::SetNeuronBias(
        std::uint64_t neuronId,
        double bias)
    {
        const std::scoped_lock lock(Detail::ClientMutex());

        if (Detail::IsClientMutationBlocked())
        {
            return false;
        }

        return Engine::NetworkParameters::SetBias(
            CurrentNetwork,
            neuronId,
            bias);
    }

    bool MiaIAClient::SetConnectionWeight(
        std::uint64_t connectionId,
        double weight)
    {
        const std::scoped_lock lock(Detail::ClientMutex());

        if (Detail::IsClientMutationBlocked())
        {
            return false;
        }

        return Engine::NetworkWeights::SetWeight(
            CurrentNetwork,
            connectionId,
            weight);
    }

    bool MiaIAClient::TryGetNeuron(
        std::uint64_t neuronId,
        Core::NeuronSnapshot& result)
    {
        const std::scoped_lock lock(Detail::ClientMutex());
        return Engine::NetworkInspector::TryGetNeuron(
            CurrentNetwork,
            neuronId,
            result);
    }

    bool MiaIAClient::TryGetConnection(
        std::uint64_t connectionId,
        Core::ConnectionSnapshot& result)
    {
        const std::scoped_lock lock(Detail::ClientMutex());
        return Engine::NetworkInspector::TryGetConnection(
            CurrentNetwork,
            connectionId,
            result);
    }

    bool MiaIAClient::TryGetLayer(
        std::uint64_t layerId,
        Core::LayerSnapshot& result)
    {
        const std::scoped_lock lock(Detail::ClientMutex());
        return Engine::NetworkInspector::TryGetLayer(
            CurrentNetwork,
            layerId,
            result);
    }

    bool MiaIAClient::RemoveConnection(
        std::uint64_t id)
    {
        const std::scoped_lock lock(Detail::ClientMutex());

        if (Detail::IsClientMutationBlocked())
        {
            return false;
        }

        return Engine::NetworkEditor::RemoveConnection(
            CurrentNetwork,
            id);
    }

    bool MiaIAClient::RemoveNeuron(
        std::uint64_t neuronId)
    {
        const std::scoped_lock lock(Detail::ClientMutex());

        if (Detail::IsClientMutationBlocked())
        {
            return false;
        }

        return Engine::NetworkEditor::RemoveNeuron(
            CurrentNetwork,
            neuronId);
    }

    bool MiaIAClient::RemoveLayer(
        std::uint64_t layerId)
    {
        const std::scoped_lock lock(Detail::ClientMutex());

        if (Detail::IsClientMutationBlocked())
        {
            return false;
        }

        return Engine::NetworkEditor::RemoveLayer(
            CurrentNetwork,
            layerId);
    }

    bool MiaIAClient::SetLayerActivation(
        std::uint64_t layerId,
        Core::ActivationType activation)
    {
        const std::scoped_lock lock(Detail::ClientMutex());

        if (Detail::IsClientMutationBlocked())
        {
            return false;
        }

        return Engine::NetworkParameters::SetLayerActivation(
            CurrentNetwork,
            layerId,
            activation);
    }

    bool MiaIAClient::Forward()
    {
        const std::scoped_lock lock(Detail::ClientMutex());

        if (Detail::IsClientMutationBlocked())
        {
            return false;
        }

        return Engine::NetworkRuntime::Forward(CurrentNetwork);
    }

    Core::NetworkSnapshot MiaIAClient::GetSnapshot()
    {
        const std::scoped_lock lock(Detail::ClientMutex());
        return Engine::NetworkInspector::Snapshot(CurrentNetwork);
    }

    bool MiaIAClient::GetConnectionWeight(
        std::uint64_t connectionId,
        double& weight)
    {
        const std::scoped_lock lock(Detail::ClientMutex());
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
        const std::scoped_lock lock(Detail::ClientMutex());

        if (Detail::IsClientMutationBlocked())
        {
            return false;
        }

        return Engine::NetworkFactory::CreateDense(
            CurrentNetwork,
            inputCount,
            hiddenCount,
            hiddenLayers,
            outputCount);
    }

}
