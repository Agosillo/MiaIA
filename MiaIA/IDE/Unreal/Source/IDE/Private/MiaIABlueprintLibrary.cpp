#include "MiaIABlueprintLibrary.h"

#include "MiaIAClient.h"
#include "MiaIACommandProcessor.h"
#include "Misc/Paths.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace
{
    EMiaIAActivationType ToBlueprint(
        MiaIA::Core::ActivationType activation)
    {
        switch (activation)
        {
        case MiaIA::Core::ActivationType::Sigmoid:
            return EMiaIAActivationType::Sigmoid;
        case MiaIA::Core::ActivationType::ReLU:
            return EMiaIAActivationType::ReLU;
        case MiaIA::Core::ActivationType::Tanh:
            return EMiaIAActivationType::Tanh;
        case MiaIA::Core::ActivationType::Linear:
            return EMiaIAActivationType::Linear;
        }

        return EMiaIAActivationType::Sigmoid;
    }

    MiaIA::Core::ActivationType ToCore(
        EMiaIAActivationType activation)
    {
        switch (activation)
        {
        case EMiaIAActivationType::Sigmoid:
            return MiaIA::Core::ActivationType::Sigmoid;
        case EMiaIAActivationType::ReLU:
            return MiaIA::Core::ActivationType::ReLU;
        case EMiaIAActivationType::Tanh:
            return MiaIA::Core::ActivationType::Tanh;
        case EMiaIAActivationType::Linear:
            return MiaIA::Core::ActivationType::Linear;
        }

        return MiaIA::Core::ActivationType::Sigmoid;
    }

    FMiaIANeuronSnapshot ToBlueprint(
        const MiaIA::Core::NeuronSnapshot& source)
    {
        FMiaIANeuronSnapshot result;
        result.Id = static_cast<int64>(source.Id);
        result.Activation = source.Activation;
        result.Bias = source.Bias;
        return result;
    }

    FMiaIAConnectionSnapshot ToBlueprint(
        const MiaIA::Core::ConnectionSnapshot& source)
    {
        FMiaIAConnectionSnapshot result;
        result.Id = static_cast<int64>(source.Id);
        result.FromNeuron = static_cast<int64>(source.FromNeuron);
        result.ToNeuron = static_cast<int64>(source.ToNeuron);
        result.Weight = source.Weight;
        return result;
    }

    FMiaIANeuronContext ToBlueprint(
        const MiaIA::Core::NeuronContextSnapshot& source)
    {
        FMiaIANeuronContext result;
        result.Neuron = ToBlueprint(source.Neuron);
        result.LayerId = static_cast<int64>(source.LayerId);
        result.LayerName = UTF8_TO_TCHAR(source.LayerName.c_str());
        result.LayerOrder = static_cast<int64>(source.LayerOrder);
        result.LayerActivation = ToBlueprint(source.LayerActivation);
        return result;
    }

    FMiaIANeuronInspection ToBlueprint(
        const MiaIA::Core::NeuronInspectionSnapshot& source)
    {
        FMiaIANeuronInspection result;
        result.Context = ToBlueprint(source.Context);
        result.IncomingConnectionCount = static_cast<int64>(
            source.IncomingConnectionCount);
        result.OutgoingConnectionCount = static_cast<int64>(
            source.OutgoingConnectionCount);
        result.IncomingConnections.Reserve(
            static_cast<int32>(source.IncomingConnections.size()));
        result.OutgoingConnections.Reserve(
            static_cast<int32>(source.OutgoingConnections.size()));

        for (const auto& connection : source.IncomingConnections)
        {
            result.IncomingConnections.Add(ToBlueprint(connection));
        }

        for (const auto& connection : source.OutgoingConnections)
        {
            result.OutgoingConnections.Add(ToBlueprint(connection));
        }

        return result;
    }

    FMiaIAConnectionInspection ToBlueprint(
        const MiaIA::Core::ConnectionInspectionSnapshot& source)
    {
        FMiaIAConnectionInspection result;
        result.Connection = ToBlueprint(source.Connection);
        result.FromNeuron = ToBlueprint(source.FromNeuron);
        result.ToNeuron = ToBlueprint(source.ToNeuron);
        return result;
    }

    FMiaIALayerSnapshot ToBlueprint(
        const MiaIA::Core::LayerSnapshot& source)
    {
        FMiaIALayerSnapshot result;
        result.Id = static_cast<int64>(source.Id);
        result.Name = UTF8_TO_TCHAR(source.Name.c_str());
        result.Order = static_cast<int64>(source.Order);
        result.Activation = ToBlueprint(source.Activation);
        result.Neurons.Reserve(static_cast<int32>(source.Neurons.size()));

        for (const auto& sourceNeuron : source.Neurons)
        {
            FMiaIANeuronSnapshot neuron;
            neuron.Id = static_cast<int64>(sourceNeuron.Id);
            neuron.Activation = sourceNeuron.Activation;
            neuron.Bias = sourceNeuron.Bias;
            result.Neurons.Add(MoveTemp(neuron));
        }

        return result;
    }

    FMiaIANetworkSnapshot ToBlueprint(
        const MiaIA::Core::NetworkSnapshot& source)
    {
        FMiaIANetworkSnapshot result;
        result.Layers.Reserve(static_cast<int32>(source.Layers.size()));

        for (const auto& sourceLayer : source.Layers)
        {
            result.Layers.Add(ToBlueprint(sourceLayer));
        }

        result.Connections.Reserve(
            static_cast<int32>(source.Connections.size()));

        for (const auto& sourceConnection : source.Connections)
        {
            FMiaIAConnectionSnapshot connection;
            connection.Id = static_cast<int64>(sourceConnection.Id);
            connection.FromNeuron =
                static_cast<int64>(sourceConnection.FromNeuron);
            connection.ToNeuron =
                static_cast<int64>(sourceConnection.ToNeuron);
            connection.Weight = sourceConnection.Weight;
            result.Connections.Add(MoveTemp(connection));
        }

        return result;
    }

    FMiaIANetworkOverview ToBlueprint(
        const MiaIA::Core::NetworkOverviewSnapshot& source)
    {
        FMiaIANetworkOverview result;
        result.NeuronCount = static_cast<int64>(source.NeuronCount);
        result.ConnectionCount = static_cast<int64>(
            source.ConnectionCount);
        result.Layers.Reserve(static_cast<int32>(source.Layers.size()));

        for (const auto& sourceLayer : source.Layers)
        {
            FMiaIALayerOverview layer;
            layer.Id = static_cast<int64>(sourceLayer.Id);
            layer.Name = UTF8_TO_TCHAR(sourceLayer.Name.c_str());
            layer.Order = static_cast<int64>(sourceLayer.Order);
            layer.NeuronCount = static_cast<int64>(
                sourceLayer.NeuronCount);
            layer.Activation = ToBlueprint(sourceLayer.Activation);
            result.Layers.Add(MoveTemp(layer));
        }

        return result;
    }

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

    EMiaIATrainingBreakpointKind ToBlueprint(
        MiaIA::Core::TrainingBreakpointKind kind)
    {
        switch (kind)
        {
        case MiaIA::Core::TrainingBreakpointKind::Phase:
            return EMiaIATrainingBreakpointKind::Phase;
        case MiaIA::Core::TrainingBreakpointKind::NeuronActivationAbove:
            return EMiaIATrainingBreakpointKind::NeuronActivationAbove;
        case MiaIA::Core::TrainingBreakpointKind::NeuronActivationBelow:
            return EMiaIATrainingBreakpointKind::NeuronActivationBelow;
        case MiaIA::Core::TrainingBreakpointKind::NeuronGradientMagnitudeAbove:
            return EMiaIATrainingBreakpointKind::NeuronGradientMagnitudeAbove;
        case MiaIA::Core::TrainingBreakpointKind::ConnectionUpdateMagnitudeAbove:
            return EMiaIATrainingBreakpointKind::ConnectionUpdateMagnitudeAbove;
        }

        return EMiaIATrainingBreakpointKind::Phase;
    }

    MiaIA::Core::TrainingBreakpointKind ToCore(
        EMiaIATrainingBreakpointKind kind)
    {
        switch (kind)
        {
        case EMiaIATrainingBreakpointKind::Phase:
            return MiaIA::Core::TrainingBreakpointKind::Phase;
        case EMiaIATrainingBreakpointKind::NeuronActivationAbove:
            return MiaIA::Core::TrainingBreakpointKind::NeuronActivationAbove;
        case EMiaIATrainingBreakpointKind::NeuronActivationBelow:
            return MiaIA::Core::TrainingBreakpointKind::NeuronActivationBelow;
        case EMiaIATrainingBreakpointKind::NeuronGradientMagnitudeAbove:
            return MiaIA::Core::TrainingBreakpointKind::NeuronGradientMagnitudeAbove;
        case EMiaIATrainingBreakpointKind::ConnectionUpdateMagnitudeAbove:
            return MiaIA::Core::TrainingBreakpointKind::ConnectionUpdateMagnitudeAbove;
        }

        return MiaIA::Core::TrainingBreakpointKind::Phase;
    }

    MiaIA::Core::TrainingDebugPhase ToCore(
        EMiaIATrainingDebugPhase phase)
    {
        switch (phase)
        {
        case EMiaIATrainingDebugPhase::Idle:
            return MiaIA::Core::TrainingDebugPhase::Idle;
        case EMiaIATrainingDebugPhase::BeforeForward:
            return MiaIA::Core::TrainingDebugPhase::BeforeForward;
        case EMiaIATrainingDebugPhase::ForwardComplete:
            return MiaIA::Core::TrainingDebugPhase::ForwardComplete;
        case EMiaIATrainingDebugPhase::BackwardComplete:
            return MiaIA::Core::TrainingDebugPhase::BackwardComplete;
        case EMiaIATrainingDebugPhase::UpdateComplete:
            return MiaIA::Core::TrainingDebugPhase::UpdateComplete;
        case EMiaIATrainingDebugPhase::Verified:
            return MiaIA::Core::TrainingDebugPhase::Verified;
        case EMiaIATrainingDebugPhase::Committed:
            return MiaIA::Core::TrainingDebugPhase::Committed;
        }

        return MiaIA::Core::TrainingDebugPhase::Idle;
    }

    FMiaIATrainingBreakpoint ToBlueprint(
        const MiaIA::Core::TrainingBreakpointSnapshot& source)
    {
        FMiaIATrainingBreakpoint result;
        result.Id = static_cast<int64>(source.Id);
        result.bEnabled = source.Enabled;
        result.Kind = ToBlueprint(source.Spec.Kind);
        result.Phase = ToBlueprint(source.Spec.Phase);
        result.TargetId = static_cast<int64>(source.Spec.TargetId);
        result.Threshold = source.Spec.Threshold;
        result.HitCount = static_cast<int64>(source.HitCount);
        return result;
    }

    FMiaIATrainingBreakpointHit ToBlueprint(
        const MiaIA::Core::TrainingBreakpointHitSnapshot& source)
    {
        FMiaIATrainingBreakpointHit result;
        result.BreakpointId = static_cast<int64>(source.BreakpointId);
        result.Kind = ToBlueprint(source.Kind);
        result.Phase = ToBlueprint(source.Phase);
        result.TargetId = static_cast<int64>(source.TargetId);
        result.ObservedValue = source.ObservedValue;
        result.Threshold = source.Threshold;
        result.StepIndex = static_cast<int64>(source.StepIndex);
        result.SampleIndex = static_cast<int64>(source.SampleIndex);
        return result;
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
        result.CandidateNetwork = ToBlueprint(source.CandidateNetwork);

        TMap<int64, int32> neuronIndices;
        for (const auto& sourceLayer : source.CandidateNetwork.Layers)
        {
            for (const auto& sourceNeuron : sourceLayer.Neurons)
            {
                FMiaIADebugNeuronTelemetry telemetry;
                telemetry.Id = static_cast<int64>(sourceNeuron.Id);
                telemetry.LayerOrder = static_cast<int64>(sourceLayer.Order);
                telemetry.CandidateActivation = sourceNeuron.Activation;
                neuronIndices.Add(
                    telemetry.Id,
                    result.NeuronTelemetry.Add(MoveTemp(telemetry)));
            }
        }

        const bool hasGradients = source.Phase >=
            MiaIA::Core::TrainingDebugPhase::BackwardComplete;
        if (hasGradients)
        {
            for (const auto& gradient : source.Step.Before.Neurons)
            {
                const int32* index = neuronIndices.Find(
                    static_cast<int64>(gradient.Id));
                if (!index)
                {
                    continue;
                }

                auto& telemetry = result.NeuronTelemetry[*index];
                telemetry.bHasGradients = true;
                telemetry.ActivationGradient = gradient.ActivationGradient;
                telemetry.PreActivationGradient =
                    gradient.PreActivationGradient;
                telemetry.BiasGradient = gradient.BiasGradient;
            }
        }

        const bool hasUpdates = source.Phase >=
            MiaIA::Core::TrainingDebugPhase::UpdateComplete;
        if (hasUpdates)
        {
            for (const auto& update : source.Step.NeuronUpdates)
            {
                const int32* index = neuronIndices.Find(
                    static_cast<int64>(update.Id));
                if (!index)
                {
                    continue;
                }

                auto& telemetry = result.NeuronTelemetry[*index];
                telemetry.bHasUpdate = true;
                telemetry.Delta = update.Delta;
            }
        }

        TMap<int64, int32> connectionIndices;
        for (const auto& sourceConnection :
            source.CandidateNetwork.Connections)
        {
            FMiaIADebugConnectionTelemetry telemetry;
            telemetry.Id = static_cast<int64>(sourceConnection.Id);
            telemetry.FromNeuron = static_cast<int64>(
                sourceConnection.FromNeuron);
            telemetry.ToNeuron = static_cast<int64>(
                sourceConnection.ToNeuron);
            telemetry.CandidateWeight = sourceConnection.Weight;
            connectionIndices.Add(
                telemetry.Id,
                result.ConnectionTelemetry.Add(MoveTemp(telemetry)));
        }

        if (hasGradients)
        {
            for (const auto& gradient : source.Step.Before.Connections)
            {
                const int32* index = connectionIndices.Find(
                    static_cast<int64>(gradient.Id));
                if (!index)
                {
                    continue;
                }

                auto& telemetry = result.ConnectionTelemetry[*index];
                telemetry.bHasGradient = true;
                telemetry.WeightGradient = gradient.WeightGradient;
            }
        }

        if (hasUpdates)
        {
            for (const auto& update : source.Step.ConnectionUpdates)
            {
                const int32* index = connectionIndices.Find(
                    static_cast<int64>(update.Id));
                if (!index)
                {
                    continue;
                }

                auto& telemetry = result.ConnectionTelemetry[*index];
                telemetry.bHasUpdate = true;
                telemetry.Delta = update.Delta;
            }
        }

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
        result.Breakpoints.Reserve(
            static_cast<int32>(source.Breakpoints.size()));

        for (const auto& breakpoint : source.Breakpoints)
        {
            result.Breakpoints.Add(ToBlueprint(breakpoint));
        }

        result.bHasBreakpointHit = source.HasBreakpointHit;
        result.LastBreakpointHit = ToBlueprint(source.LastBreakpointHit);
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

    FMiaIAProjectInfo ToBlueprint(
        const MiaIA::Core::ProjectInfoSnapshot& source)
    {
        FMiaIAProjectInfo result;
        result.FormatVersion = static_cast<int32>(source.FormatVersion);
        result.Path = UTF8_TO_TCHAR(source.Path.c_str());
        result.bHasModel = source.HasModel;
        result.bHasDatasetReference = source.HasDatasetReference;
        result.bDatasetLoaded = source.DatasetLoaded;
        result.DatasetSource = UTF8_TO_TCHAR(source.DatasetSource.c_str());
        result.DatasetInputCount = static_cast<int64>(
            source.DatasetInputCount);
        result.DatasetTargetCount = static_cast<int64>(
            source.DatasetTargetCount);
        result.bDatasetHasHeader = source.DatasetHasHeader;
        result.bTrainingAvailable = source.Training.Available;
        result.TrainingEpochCount = static_cast<int64>(
            source.Training.EpochCount);
        result.TrainingLearningRate = source.Training.LearningRate;
        result.BreakpointCount = static_cast<int64>(
            source.BreakpointCount);
        return result;
    }
}

FString UMiaIABlueprintLibrary::ExecuteCommand(
    const FString& Command,
    bool& OutExitRequested)
{
    const auto result = MiaIA::CLI::MiaIACommandProcessor::Execute(
        std::string(TCHAR_TO_UTF8(*Command)),
        std::string(TCHAR_TO_UTF8(*FPaths::ProjectDir())));
    OutExitRequested = result.ExitRequested;
    return UTF8_TO_TCHAR(result.Output.c_str());
}

bool UMiaIABlueprintLibrary::NewProject()
{
    return MiaIA::SDK::MiaIAClient::NewProject();
}

bool UMiaIABlueprintLibrary::OpenProject(const FString& Path)
{
    return !Path.IsEmpty() &&
        MiaIA::SDK::MiaIAClient::OpenProject(
            std::string(TCHAR_TO_UTF8(*Path)));
}

bool UMiaIABlueprintLibrary::SaveProject(const FString& Path)
{
    return !Path.IsEmpty() &&
        MiaIA::SDK::MiaIAClient::SaveProject(
            std::string(TCHAR_TO_UTF8(*Path)));
}

FMiaIAProjectInfo UMiaIABlueprintLibrary::GetProjectInfo()
{
    return ToBlueprint(MiaIA::SDK::MiaIAClient::GetProjectInfo());
}

bool UMiaIABlueprintLibrary::ImportOnnx(const FString& Path)
{
    return !Path.IsEmpty() &&
        MiaIA::SDK::MiaIAClient::ImportOnnx(
            std::string(TCHAR_TO_UTF8(*Path)));
}

bool UMiaIABlueprintLibrary::ExportOnnx(const FString& Path)
{
    return !Path.IsEmpty() &&
        MiaIA::SDK::MiaIAClient::ExportOnnx(
            std::string(TCHAR_TO_UTF8(*Path)));
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

bool UMiaIABlueprintLibrary::CreateConfiguredDenseNetwork(
    int32 InputCount,
    int32 HiddenCount,
    int32 HiddenLayers,
    int32 OutputCount,
    EMiaIAActivationType HiddenActivation,
    EMiaIAActivationType OutputActivation,
    double InitialWeight,
    double InitialBias)
{
    if (InputCount <= 0 ||
        HiddenCount <= 0 ||
        HiddenLayers < 0 ||
        OutputCount <= 0 ||
        !FMath::IsFinite(InitialWeight) ||
        !FMath::IsFinite(InitialBias))
    {
        return false;
    }

    MiaIA::Core::DenseNetworkConfiguration configuration;
    configuration.HiddenActivation = ToCore(HiddenActivation);
    configuration.OutputActivation = ToCore(OutputActivation);
    configuration.InitialWeight = InitialWeight;
    configuration.InitialBias = InitialBias;

    return MiaIA::SDK::MiaIAClient::CreateDenseNetwork(
        InputCount,
        HiddenCount,
        HiddenLayers,
        OutputCount,
        configuration);
}

bool UMiaIABlueprintLibrary::ConfigureNetworkParameters(
    bool bUpdateHiddenActivation,
    EMiaIAActivationType HiddenActivation,
    bool bUpdateOutputActivation,
    EMiaIAActivationType OutputActivation,
    bool bUpdateConnectionWeights,
    double ConnectionWeight,
    bool bUpdateNonInputBiases,
    double NonInputBias,
    FMiaIANetworkParameterUpdateResult& Result)
{
    if ((bUpdateConnectionWeights && !FMath::IsFinite(ConnectionWeight)) ||
        (bUpdateNonInputBiases && !FMath::IsFinite(NonInputBias)))
    {
        return false;
    }

    MiaIA::Core::NetworkParameterUpdate update;

    if (bUpdateHiddenActivation)
    {
        update.HiddenActivation = ToCore(HiddenActivation);
    }

    if (bUpdateOutputActivation)
    {
        update.OutputActivation = ToCore(OutputActivation);
    }

    if (bUpdateConnectionWeights)
    {
        update.ConnectionWeight = ConnectionWeight;
    }

    if (bUpdateNonInputBiases)
    {
        update.NonInputBias = NonInputBias;
    }

    MiaIA::Core::NetworkParameterUpdateSnapshot updateResult;

    if (!MiaIA::SDK::MiaIAClient::ApplyNetworkParameterUpdate(
        update,
        updateResult))
    {
        return false;
    }

    FMiaIANetworkParameterUpdateResult blueprintResult;
    blueprintResult.HiddenLayersChanged =
        static_cast<int64>(updateResult.HiddenLayersChanged);
    blueprintResult.bOutputLayerChanged =
        updateResult.OutputLayerChanged;
    blueprintResult.ConnectionWeightsChanged =
        static_cast<int64>(updateResult.ConnectionWeightsChanged);
    blueprintResult.NeuronBiasesChanged =
        static_cast<int64>(updateResult.NeuronBiasesChanged);
    Result = blueprintResult;
    return true;
}

bool UMiaIABlueprintLibrary::SetNeuronBias(int64 NeuronId, double Bias)
{
    return NeuronId >= 0 &&
        FMath::IsFinite(Bias) &&
        MiaIA::SDK::MiaIAClient::SetNeuronBias(
            static_cast<std::uint64_t>(NeuronId),
            Bias);
}

bool UMiaIABlueprintLibrary::SetConnectionWeight(
    int64 ConnectionId,
    double Weight)
{
    return ConnectionId >= 0 &&
        FMath::IsFinite(Weight) &&
        MiaIA::SDK::MiaIAClient::SetConnectionWeight(
            static_cast<std::uint64_t>(ConnectionId),
            Weight);
}

FMiaIANetworkSnapshot UMiaIABlueprintLibrary::GetNetworkSnapshot()
{
    return ToBlueprint(MiaIA::SDK::MiaIAClient::GetSnapshot());
}

FMiaIANetworkOverview UMiaIABlueprintLibrary::GetNetworkOverview()
{
    return ToBlueprint(MiaIA::SDK::MiaIAClient::GetNetworkOverview());
}

bool UMiaIABlueprintLibrary::GetLayerSnapshot(
    int64 LayerId,
    FMiaIALayerSnapshot& OutLayer)
{
    if (LayerId < 0)
    {
        return false;
    }

    MiaIA::Core::LayerSnapshot layer;

    if (!MiaIA::SDK::MiaIAClient::TryGetLayer(
        static_cast<std::uint64_t>(LayerId),
        layer))
    {
        return false;
    }

    OutLayer = ToBlueprint(layer);
    return true;
}

bool UMiaIABlueprintLibrary::InspectNeuron(
    int64 NeuronId,
    int32 MaximumConnections,
    FMiaIANeuronInspection& OutInspection)
{
    if (NeuronId < 0 || MaximumConnections < 0)
    {
        return false;
    }

    MiaIA::Core::NeuronInspectionSnapshot inspection;

    if (!MiaIA::SDK::MiaIAClient::TryInspectNeuron(
        static_cast<std::uint64_t>(NeuronId),
        static_cast<std::size_t>(MaximumConnections),
        inspection))
    {
        return false;
    }

    OutInspection = ToBlueprint(inspection);
    return true;
}

bool UMiaIABlueprintLibrary::InspectConnection(
    int64 ConnectionId,
    FMiaIAConnectionInspection& OutInspection)
{
    if (ConnectionId < 0)
    {
        return false;
    }

    MiaIA::Core::ConnectionInspectionSnapshot inspection;

    if (!MiaIA::SDK::MiaIAClient::TryInspectConnection(
        static_cast<std::uint64_t>(ConnectionId),
        inspection))
    {
        return false;
    }

    OutInspection = ToBlueprint(inspection);
    return true;
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

bool UMiaIABlueprintLibrary::ResumeTrainingSession()
{
    return MiaIA::SDK::MiaIAClient::ResumeTrainingSession();
}

bool UMiaIABlueprintLibrary::PauseTrainingSession()
{
    return MiaIA::SDK::MiaIAClient::PauseTrainingSession();
}

bool UMiaIABlueprintLibrary::AddTrainingBreakpoint(
    EMiaIATrainingBreakpointKind Kind,
    EMiaIATrainingDebugPhase Phase,
    int64 TargetId,
    double Threshold,
    FMiaIATrainingBreakpoint& OutBreakpoint)
{
    if (TargetId < 0)
    {
        return false;
    }

    MiaIA::Core::TrainingBreakpointSpec spec;
    spec.Kind = ToCore(Kind);
    spec.Phase = ToCore(Phase);
    spec.TargetId = static_cast<std::uint64_t>(TargetId);
    spec.Threshold = Threshold;
    MiaIA::Core::TrainingBreakpointSnapshot breakpoint;

    if (!MiaIA::SDK::MiaIAClient::AddTrainingBreakpoint(
        spec,
        breakpoint))
    {
        return false;
    }

    OutBreakpoint = ToBlueprint(breakpoint);
    return true;
}

TArray<FMiaIATrainingBreakpoint>
UMiaIABlueprintLibrary::GetTrainingBreakpoints()
{
    const auto source =
        MiaIA::SDK::MiaIAClient::GetTrainingBreakpoints();
    TArray<FMiaIATrainingBreakpoint> result;
    result.Reserve(static_cast<int32>(source.size()));

    for (const auto& breakpoint : source)
    {
        result.Add(ToBlueprint(breakpoint));
    }

    return result;
}

bool UMiaIABlueprintLibrary::SetTrainingBreakpointEnabled(
    int64 BreakpointId,
    bool bEnabled)
{
    return BreakpointId > 0 &&
        MiaIA::SDK::MiaIAClient::SetTrainingBreakpointEnabled(
            static_cast<std::uint64_t>(BreakpointId),
            bEnabled);
}

bool UMiaIABlueprintLibrary::RemoveTrainingBreakpoint(
    int64 BreakpointId)
{
    return BreakpointId > 0 &&
        MiaIA::SDK::MiaIAClient::RemoveTrainingBreakpoint(
            static_cast<std::uint64_t>(BreakpointId));
}

bool UMiaIABlueprintLibrary::ClearTrainingBreakpoints()
{
    return MiaIA::SDK::MiaIAClient::ClearTrainingBreakpoints();
}

bool UMiaIABlueprintLibrary::GetLastTrainingBreakpointHit(
    FMiaIATrainingBreakpointHit& OutHit)
{
    MiaIA::Core::TrainingBreakpointHitSnapshot hit;

    if (!MiaIA::SDK::MiaIAClient::TryGetLastTrainingBreakpointHit(hit))
    {
        return false;
    }

    OutHit = ToBlueprint(hit);
    return true;
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

FMiaIANetworkSnapshot UMiaIABlueprintLibrary::GetDebugNetworkSnapshot()
{
    return ToBlueprint(
        MiaIA::SDK::MiaIAClient::GetTrainingDebug().CandidateNetwork);
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
