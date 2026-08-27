#pragma once

#include "CoreMinimal.h"
#include "StudioController.h"

struct IDE_API FMiaIAInstanceHandle
{
    uint64 Value{};

    bool IsValid() const
    {
        return Value != 0;
    }
};

class IDE_API FMiaIAInstanceService final
{
public:
    static FMiaIAInstanceHandle DefaultInstance();
    static bool IsValid(FMiaIAInstanceHandle Instance);

    static void Refresh(FMiaIAInstanceHandle Instance);
    static bool CreateContext(
        FMiaIAInstanceHandle Instance,
        const FString& Name);
    static bool SelectContext(
        FMiaIAInstanceHandle Instance,
        uint64 ContextId);
    static bool RenameContext(
        FMiaIAInstanceHandle Instance,
        uint64 ContextId,
        const FString& Name);
    static bool RemoveContext(
        FMiaIAInstanceHandle Instance,
        uint64 ContextId);
    static std::vector<MiaIA::Core::ModelContextSnapshot> Contexts(
        FMiaIAInstanceHandle Instance);
    static MiaIA::Core::ModelContextSnapshot ActiveContext(
        FMiaIAInstanceHandle Instance);
    static bool RunForwardTrace(
        FMiaIAInstanceHandle Instance,
        const TArray<double>& Inputs);
    static void ClearForwardTrace(FMiaIAInstanceHandle Instance);
    static bool FocusForwardTraceNeuron(
        FMiaIAInstanceHandle Instance,
        uint64 NeuronId);
    static bool SetForwardTraceContributionRequest(
        FMiaIAInstanceHandle Instance,
        const MiaIA::Core::ForwardTraceContributionPageRequest& Request);
    static bool PlayForwardTrace(FMiaIAInstanceHandle Instance);
    static bool PauseForwardTrace(FMiaIAInstanceHandle Instance);
    static bool RestartForwardTrace(FMiaIAInstanceHandle Instance);
    static bool StepForwardTraceForward(FMiaIAInstanceHandle Instance);
    static bool StepForwardTraceBackward(FMiaIAInstanceHandle Instance);
    static bool AdvanceForwardTracePlayback(
        FMiaIAInstanceHandle Instance,
        double ElapsedSeconds);
    static bool SetForwardTraceFrameDuration(
        FMiaIAInstanceHandle Instance,
        double DurationSeconds);
    static bool RunBackwardTrace(
        FMiaIAInstanceHandle Instance,
        const TArray<double>& Inputs,
        const TArray<double>& Targets);
    static void ClearBackwardTrace(FMiaIAInstanceHandle Instance);
    static bool FocusBackwardTraceNeuron(
        FMiaIAInstanceHandle Instance,
        uint64 NeuronId);
    static bool PlayBackwardTrace(FMiaIAInstanceHandle Instance);
    static bool PauseBackwardTrace(FMiaIAInstanceHandle Instance);
    static bool RestartBackwardTrace(FMiaIAInstanceHandle Instance);
    static bool StepBackwardTraceForward(FMiaIAInstanceHandle Instance);
    static bool StepBackwardTraceBackward(FMiaIAInstanceHandle Instance);
    static bool AdvanceBackwardTracePlayback(
        FMiaIAInstanceHandle Instance,
        double ElapsedSeconds);
    static bool SetBackwardTraceFrameDuration(
        FMiaIAInstanceHandle Instance,
        double DurationSeconds);
    static bool RunSignalHealthDiagnostics(
        FMiaIAInstanceHandle Instance,
        const MiaIA::Core::SignalHealthConfiguration& Configuration);
    static void ClearSignalHealthDiagnostics(
        FMiaIAInstanceHandle Instance);
    static void SetSignalHealthFilter(
        FMiaIAInstanceHandle Instance,
        MiaIA::Studio::StudioSignalHealthFilter Filter);
    static void RefreshModelCheckpoints(FMiaIAInstanceHandle Instance);
    static bool CaptureModelCheckpoint(
        FMiaIAInstanceHandle Instance,
        const FString& Name);
    static bool SelectModelCheckpoint(
        FMiaIAInstanceHandle Instance,
        uint64 CheckpointId);
    static bool CompareModelCheckpoints(
        FMiaIAInstanceHandle Instance,
        uint64 FirstCheckpointId,
        uint64 SecondCheckpointId);
    static bool RestoreModelCheckpoint(
        FMiaIAInstanceHandle Instance,
        uint64 CheckpointId);
    static bool RemoveModelCheckpoint(
        FMiaIAInstanceHandle Instance,
        uint64 CheckpointId);
    static bool ClearModelCheckpoints(FMiaIAInstanceHandle Instance);
    static void RefreshTrainingTimeline(FMiaIAInstanceHandle Instance);
    static bool SelectTrainingTimelineStep(
        FMiaIAInstanceHandle Instance,
        uint64 StepIndex);
    static void ClearTrainingTimelineSelection(
        FMiaIAInstanceHandle Instance);
    static MiaIA::Studio::StudioForwardTraceState ForwardTraceState(
        FMiaIAInstanceHandle Instance);
    static MiaIA::Studio::StudioBackwardTraceState BackwardTraceState(
        FMiaIAInstanceHandle Instance);
    static MiaIA::Studio::StudioSignalHealthState SignalHealthState(
        FMiaIAInstanceHandle Instance);
    static MiaIA::Studio::StudioModelCheckpointState ModelCheckpointState(
        FMiaIAInstanceHandle Instance);
    static MiaIA::Studio::StudioTrainingTimelineState TrainingTimelineState(
        FMiaIAInstanceHandle Instance);
};
