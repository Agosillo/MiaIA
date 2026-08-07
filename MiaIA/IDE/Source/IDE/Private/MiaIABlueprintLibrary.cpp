#include "MiaIABlueprintLibrary.h"

#include "MiaIAClient.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace
{
    EMiaIATrainingDebugPhase ToBlueprint(
        MiaIA::Core::TrainingDebugPhase phase)
    {
        switch (phase)
        {
        case MiaIA::Core::TrainingDebugPhase::Idle:
            return EMiaIATrainingDebugPhase::Idle;
        case MiaIA::Core::TrainingDebugPhase::BeforeForward:
            return EMiaIATrainingDebugPhase::BeforeForward;
        case MiaIA::Core::TrainingDebugPhase::ForwardComplete:
            return EMiaIATrainingDebugPhase::ForwardComplete;
        case MiaIA::Core::TrainingDebugPhase::BackwardComplete:
            return EMiaIATrainingDebugPhase::BackwardComplete;
        case MiaIA::Core::TrainingDebugPhase::UpdateComplete:
            return EMiaIATrainingDebugPhase::UpdateComplete;
        case MiaIA::Core::TrainingDebugPhase::Verified:
            return EMiaIATrainingDebugPhase::Verified;
        case MiaIA::Core::TrainingDebugPhase::Committed:
            return EMiaIATrainingDebugPhase::Committed;
        }

        return EMiaIATrainingDebugPhase::Idle;
    }

    EMiaIATrainingSessionStatus ToBlueprint(
        MiaIA::Core::TrainingSessionStatus status)
    {
        switch (status)
        {
        case MiaIA::Core::TrainingSessionStatus::Idle:
            return EMiaIATrainingSessionStatus::Idle;
        case MiaIA::Core::TrainingSessionStatus::Active:
            return EMiaIATrainingSessionStatus::Active;
        case MiaIA::Core::TrainingSessionStatus::Running:
            return EMiaIATrainingSessionStatus::Running;
        case MiaIA::Core::TrainingSessionStatus::Completed:
            return EMiaIATrainingSessionStatus::Completed;
        case MiaIA::Core::TrainingSessionStatus::Cancelled:
            return EMiaIATrainingSessionStatus::Cancelled;
        }

        return EMiaIATrainingSessionStatus::Idle;
    }

    FMiaIATrainingDebugSnapshot ToBlueprint(
        const MiaIA::Core::TrainingDebugSnapshot& source)
    {
        FMiaIATrainingDebugSnapshot result;
        result.Phase = ToBlueprint(source.Phase);
        result.SampleIndex = static_cast<int64>(source.SampleIndex);
        result.LearningRate = source.LearningRate;
        result.bHasBeforeEvaluation = source.Phase >=
            MiaIA::Core::TrainingDebugPhase::ForwardComplete;
        result.LossBefore = source.Step.Before.Evaluation.Loss;
        result.bHasAfterEvaluation = source.Phase >=
            MiaIA::Core::TrainingDebugPhase::Verified;
        result.LossAfter = source.Step.After.Loss;
        result.CandidateLayerCount = static_cast<int64>(
            source.CandidateNetwork.Layers.size());
        result.CandidateConnectionCount = static_cast<int64>(
            source.CandidateNetwork.Connections.size());
        return result;
    }

    FMiaIATrainingSessionSnapshot ToBlueprint(
        const MiaIA::Core::TrainingSessionSnapshot& source)
    {
        FMiaIATrainingSessionSnapshot result;
        result.Status = ToBlueprint(source.Status);
        result.EpochCount = static_cast<int64>(source.EpochCount);
        result.CurrentEpoch = static_cast<int64>(source.CurrentEpoch);
        result.NextSampleIndex =
            static_cast<int64>(source.NextSampleIndex);
        result.SampleCount = static_cast<int64>(source.SampleCount);
        result.CompletedSteps =
            static_cast<int64>(source.CompletedSteps);
        result.TotalSteps = static_cast<int64>(source.TotalSteps);
        result.LearningRate = source.LearningRate;
        return result;
    }

    FMiaIATrainingDebugNeuron ToBlueprint(
        const MiaIA::Core::TrainingDebugNeuronSnapshot& source)
    {
        FMiaIATrainingDebugNeuron result;
        result.Phase = ToBlueprint(source.Phase);
        result.Id = static_cast<int64>(source.Id);
        result.LayerOrder = static_cast<int64>(source.LayerOrder);
        result.PublicActivation = source.PublicActivation;
        result.CandidateActivation = source.CandidateActivation;
        result.PublicBias = source.PublicBias;
        result.CandidateBias = source.CandidateBias;
        result.bHasGradients = source.HasGradients;
        result.ActivationGradient = source.ActivationGradient;
        result.PreActivationGradient = source.PreActivationGradient;
        result.BiasGradient = source.BiasGradient;
        result.bHasUpdate = source.HasUpdate;
        result.PreviousBias = source.PreviousBias;
        result.UpdateGradient = source.UpdateGradient;
        result.Delta = source.Delta;
        result.UpdatedBias = source.UpdatedBias;
        return result;
    }

    FMiaIATrainingDebugConnection ToBlueprint(
        const MiaIA::Core::TrainingDebugConnectionSnapshot& source)
    {
        FMiaIATrainingDebugConnection result;
        result.Phase = ToBlueprint(source.Phase);
        result.Id = static_cast<int64>(source.Id);
        result.FromNeuron = static_cast<int64>(source.FromNeuron);
        result.ToNeuron = static_cast<int64>(source.ToNeuron);
        result.PublicWeight = source.PublicWeight;
        result.CandidateWeight = source.CandidateWeight;
        result.bHasGradient = source.HasGradient;
        result.WeightGradient = source.WeightGradient;
        result.bHasUpdate = source.HasUpdate;
        result.PreviousWeight = source.PreviousWeight;
        result.UpdateGradient = source.UpdateGradient;
        result.Delta = source.Delta;
        result.UpdatedWeight = source.UpdatedWeight;
        return result;
    }
}

bool UMiaIABlueprintLibrary::CreateDenseNetwork(
    int32 InputCount,
    int32 HiddenCount,
    int32 HiddenLayers,
    int32 OutputCount)
{
    if (InputCount <= 0 ||
        HiddenCount <= 0 ||
        HiddenLayers < 0 ||
        OutputCount <= 0)
    {
        return false;
    }

    return MiaIA::SDK::MiaIAClient::CreateDenseNetwork(
        InputCount,
        HiddenCount,
        HiddenLayers,
        OutputCount);
}

bool UMiaIABlueprintLibrary::ImportCsvDataset(
    const FString& Path,
    int32 InputCount,
    int32 TargetCount,
    bool bHasHeader)
{
    if (Path.IsEmpty() || InputCount <= 0 || TargetCount <= 0)
    {
        return false;
    }

    return MiaIA::SDK::MiaIAClient::ImportCsvDataset(
        std::string(TCHAR_TO_UTF8(*Path)),
        static_cast<std::size_t>(InputCount),
        static_cast<std::size_t>(TargetCount),
        bHasHeader);
}

bool UMiaIABlueprintLibrary::StartTrainingSession(
    int64 EpochCount,
    double LearningRate,
    FMiaIATrainingSessionSnapshot& OutSession)
{
    if (EpochCount <= 0)
    {
        return false;
    }

    MiaIA::Core::TrainingSessionSnapshot session;

    if (!MiaIA::SDK::MiaIAClient::StartTrainingSession(
        static_cast<std::size_t>(EpochCount),
        LearningRate,
        MiaIA::Core::LossType::MeanSquaredError,
        MiaIA::Core::OptimizerType::StochasticGradientDescent,
        session))
    {
        return false;
    }

    OutSession = ToBlueprint(session);
    return true;
}

FMiaIATrainingSessionSnapshot
UMiaIABlueprintLibrary::GetTrainingSession()
{
    return ToBlueprint(
        MiaIA::SDK::MiaIAClient::GetTrainingSession());
}

bool UMiaIABlueprintLibrary::StartSessionDebug(
    FMiaIATrainingDebugSnapshot& OutDebug)
{
    MiaIA::Core::TrainingDebugSnapshot debug;

    if (!MiaIA::SDK::MiaIAClient::StartTrainingSessionDebug(debug))
    {
        return false;
    }

    OutDebug = ToBlueprint(debug);
    return true;
}

bool UMiaIABlueprintLibrary::AdvanceDebugPhase(
    FMiaIATrainingDebugSnapshot& OutDebug)
{
    MiaIA::Core::TrainingDebugSnapshot debug;

    if (!MiaIA::SDK::MiaIAClient::AdvanceTrainingDebug(debug))
    {
        return false;
    }

    OutDebug = ToBlueprint(debug);
    return true;
}

bool UMiaIABlueprintLibrary::CancelDebug()
{
    return MiaIA::SDK::MiaIAClient::CancelTrainingDebug();
}

FMiaIATrainingDebugSnapshot UMiaIABlueprintLibrary::GetDebugStatus()
{
    return ToBlueprint(MiaIA::SDK::MiaIAClient::GetTrainingDebug());
}

bool UMiaIABlueprintLibrary::GetDebugNeuron(
    int64 NeuronId,
    FMiaIATrainingDebugNeuron& OutNeuron)
{
    if (NeuronId < 0)
    {
        return false;
    }

    MiaIA::Core::TrainingDebugNeuronSnapshot neuron;

    if (!MiaIA::SDK::MiaIAClient::TryGetTrainingDebugNeuron(
        static_cast<std::uint64_t>(NeuronId),
        neuron))
    {
        return false;
    }

    OutNeuron = ToBlueprint(neuron);
    return true;
}

bool UMiaIABlueprintLibrary::GetDebugConnection(
    int64 ConnectionId,
    FMiaIATrainingDebugConnection& OutConnection)
{
    if (ConnectionId < 0)
    {
        return false;
    }

    MiaIA::Core::TrainingDebugConnectionSnapshot connection;

    if (!MiaIA::SDK::MiaIAClient::TryGetTrainingDebugConnection(
        static_cast<std::uint64_t>(ConnectionId),
        connection))
    {
        return false;
    }

    OutConnection = ToBlueprint(connection);
    return true;
}
