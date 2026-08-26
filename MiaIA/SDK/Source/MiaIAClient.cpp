#include "../Include/MiaIAClient.h"
#include "MiaIAClientState.h"
#include "ProjectState.h"
#include "../../Engine/Runtime/NetworkRuntime.h"
#include "../../Engine/Runtime/NetworkFactory.h"
#include "../../Engine/Inspection/NetworkInspector.h"
#include "../../Engine/Editing/NetworkEditor.h"
#include "../../Engine/Input/NetworkInput.h"
#include "../../Engine/Parameters/NetworkParameters.h"
#include "../../Engine/Weights/NetworkWeights.h"
#include "../../Engine/Checkpoint/ModelCheckpointStore.h"
#include "../../Core/Model/Network.h"
#include "../../Core/Model/Dataset.h"
#include "../../Core/Model/TrainingSession.h"
#include "../../Core/Model/TrainingDebugSession.h"
#include "../../Core/Public/ProjectInfoSnapshot.h"

#include <mutex>

namespace
{
    MiaIA::SDK::Detail::ProjectState CurrentProjectState;
    std::mutex CurrentClientMutex;
}

MiaIA::SDK::Detail::ProjectState&
MiaIA::SDK::Detail::ClientProjectState()
{
    return CurrentProjectState;
}

MiaIA::Core::Dataset& MiaIA::SDK::Detail::ClientDataset()
{
    return CurrentProjectState.ActiveModel().Dataset;
}

MiaIA::Core::Network& MiaIA::SDK::Detail::ClientNetwork()
{
    return CurrentProjectState.ActiveModel().Network;
}

MiaIA::Core::ProjectInfoSnapshot&
MiaIA::SDK::Detail::ClientProjectInfo()
{
    return CurrentProjectState.Info;
}

MiaIA::Core::TrainingSession&
MiaIA::SDK::Detail::ClientTrainingSession()
{
    return CurrentProjectState.ActiveModel().TrainingSession;
}

MiaIA::Core::TrainingDebugSession&
MiaIA::SDK::Detail::ClientTrainingDebugSession()
{
    return CurrentProjectState.ActiveModel().TrainingDebugSession;
}

MiaIA::Engine::ModelCheckpointStore&
MiaIA::SDK::Detail::ClientCheckpointStore()
{
    return CurrentProjectState.ActiveModel().Checkpoints;
}

std::mutex& MiaIA::SDK::Detail::ClientMutex()
{
    return CurrentClientMutex;
}

bool MiaIA::SDK::Detail::IsTrainingSessionRunning()
{
    return ClientTrainingSession().Status ==
        MiaIA::Core::TrainingSessionStatus::Running;
}

bool MiaIA::SDK::Detail::IsTrainingDebugActive()
{
    return ClientTrainingDebugSession().Phase >=
        MiaIA::Core::TrainingDebugPhase::BeforeForward &&
        ClientTrainingDebugSession().Phase <
        MiaIA::Core::TrainingDebugPhase::Committed;
}

bool MiaIA::SDK::Detail::IsClientMutationBlocked()
{
    return IsTrainingSessionRunning() || IsTrainingDebugActive();
}

namespace MiaIA::SDK
{
    Core::NetworkOverviewSnapshot MiaIAClient::GetNetworkOverview()
    {
        const std::scoped_lock lock(Detail::ClientMutex());
        return Engine::NetworkInspector::Overview(
            Detail::ClientNetwork());
    }

    bool MiaIAClient::ClearNetwork()
    {
        const std::scoped_lock lock(Detail::ClientMutex());

        if (Detail::IsClientMutationBlocked())
        {
            return false;
        }

        Engine::NetworkEditor::Clear(Detail::ClientNetwork());
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
            Detail::ClientNetwork(),
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
            Detail::ClientNetwork(),
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
            Detail::ClientNetwork(),
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
            Detail::ClientNetwork(),
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
            Detail::ClientNetwork(),
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
            Detail::ClientNetwork(),
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
            Detail::ClientNetwork(),
            connectionId,
            weight);
    }

    bool MiaIAClient::TryGetNeuron(
        std::uint64_t neuronId,
        Core::NeuronSnapshot& result)
    {
        const std::scoped_lock lock(Detail::ClientMutex());
        return Engine::NetworkInspector::TryGetNeuron(
            Detail::ClientNetwork(),
            neuronId,
            result);
    }

    bool MiaIAClient::TryGetConnection(
        std::uint64_t connectionId,
        Core::ConnectionSnapshot& result)
    {
        const std::scoped_lock lock(Detail::ClientMutex());
        return Engine::NetworkInspector::TryGetConnection(
            Detail::ClientNetwork(),
            connectionId,
            result);
    }

    bool MiaIAClient::TryInspectNeuron(
        std::uint64_t neuronId,
        std::size_t maximumConnectionsPerDirection,
        Core::NeuronInspectionSnapshot& result)
    {
        const std::scoped_lock lock(Detail::ClientMutex());
        return Engine::NetworkInspector::TryInspectNeuron(
            Detail::ClientNetwork(),
            neuronId,
            maximumConnectionsPerDirection,
            result);
    }

    bool MiaIAClient::TryGetNeuronRelationships(
        std::uint64_t neuronId,
        const Core::NeuronRelationshipPageRequest& request,
        Core::NeuronRelationshipPageSnapshot& result)
    {
        const std::scoped_lock lock(Detail::ClientMutex());
        return Engine::NetworkInspector::TryGetNeuronRelationships(
            Detail::ClientNetwork(),
            neuronId,
            request,
            result);
    }

    bool MiaIAClient::TryInspectConnection(
        std::uint64_t connectionId,
        Core::ConnectionInspectionSnapshot& result)
    {
        const std::scoped_lock lock(Detail::ClientMutex());
        return Engine::NetworkInspector::TryInspectConnection(
            Detail::ClientNetwork(),
            connectionId,
            result);
    }

    bool MiaIAClient::TraceForward(
        const std::vector<double>& inputs,
        Core::ForwardTraceSnapshot& result)
    {
        const std::scoped_lock lock(Detail::ClientMutex());
        return Engine::NetworkInspector::TraceForward(
            Detail::ClientNetwork(),
            inputs,
            result);
    }

    bool MiaIAClient::TraceBackward(
        const std::vector<double>& inputs,
        const std::vector<double>& targets,
        Core::LossType loss,
        Core::BackwardTraceSnapshot& result)
    {
        const std::scoped_lock lock(Detail::ClientMutex());
        return Engine::NetworkInspector::TraceBackward(
            Detail::ClientNetwork(),
            inputs,
            targets,
            loss,
            result);
    }

    bool MiaIAClient::TryGetForwardTraceContributions(
        const std::vector<double>& inputs,
        std::uint64_t neuronId,
        const Core::ForwardTraceContributionPageRequest& request,
        Core::ForwardTraceContributionPageSnapshot& result)
    {
        const std::scoped_lock lock(Detail::ClientMutex());
        return Engine::NetworkInspector::TryGetForwardTraceContributions(
            Detail::ClientNetwork(),
            inputs,
            neuronId,
            request,
            result);
    }

    bool MiaIAClient::TryGetLayer(
        std::uint64_t layerId,
        Core::LayerSnapshot& result)
    {
        const std::scoped_lock lock(Detail::ClientMutex());
        return Engine::NetworkInspector::TryGetLayer(
            Detail::ClientNetwork(),
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
            Detail::ClientNetwork(),
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
            Detail::ClientNetwork(),
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
            Detail::ClientNetwork(),
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
            Detail::ClientNetwork(),
            layerId,
            activation);
    }

    bool MiaIAClient::ApplyNetworkParameterUpdate(
        const Core::NetworkParameterUpdate& update,
        Core::NetworkParameterUpdateSnapshot& result)
    {
        const std::scoped_lock lock(Detail::ClientMutex());

        if (Detail::IsClientMutationBlocked())
        {
            return false;
        }

        return Engine::NetworkParameters::ApplyUpdate(
            Detail::ClientNetwork(),
            update,
            result);
    }

    bool MiaIAClient::Forward()
    {
        const std::scoped_lock lock(Detail::ClientMutex());

        if (Detail::IsClientMutationBlocked())
        {
            return false;
        }

        return Engine::NetworkRuntime::Forward(
            Detail::ClientNetwork());
    }

    Core::NetworkSnapshot MiaIAClient::GetSnapshot()
    {
        const std::scoped_lock lock(Detail::ClientMutex());
        return Engine::NetworkInspector::Snapshot(
            Detail::ClientNetwork());
    }

    bool MiaIAClient::GetConnectionWeight(
        std::uint64_t connectionId,
        double& weight)
    {
        const std::scoped_lock lock(Detail::ClientMutex());
        return Engine::NetworkWeights::GetWeight(
            Detail::ClientNetwork(),
            connectionId,
            weight);
    }

    bool MiaIAClient::CreateDenseNetwork(
        int inputCount,
        int hiddenCount,
        int hiddenLayers,
        int outputCount)
    {
        return CreateDenseNetwork(
            inputCount,
            hiddenCount,
            hiddenLayers,
            outputCount,
            Core::DenseNetworkConfiguration{});
    }

    bool MiaIAClient::CreateDenseNetwork(
        int inputCount,
        int hiddenCount,
        int hiddenLayers,
        int outputCount,
        const Core::DenseNetworkConfiguration& configuration)
    {
        const std::scoped_lock lock(Detail::ClientMutex());

        if (Detail::IsClientMutationBlocked())
        {
            return false;
        }

        return Engine::NetworkFactory::CreateDense(
            Detail::ClientNetwork(),
            inputCount,
            hiddenCount,
            hiddenLayers,
            outputCount,
            configuration);
    }

}
