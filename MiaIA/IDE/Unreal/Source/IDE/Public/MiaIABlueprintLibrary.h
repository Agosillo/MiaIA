#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MiaIABlueprintTypes.h"
#include "MiaIABlueprintLibrary.generated.h"

UCLASS()
class IDE_API UMiaIABlueprintLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "MiaIA|Command")
    static FString ExecuteCommand(
        const FString& Command,
        bool& OutExitRequested);

    UFUNCTION(BlueprintCallable, Category = "MiaIA|Project")
    static bool NewProject();

    UFUNCTION(BlueprintCallable, Category = "MiaIA|Project")
    static bool OpenProject(const FString& Path);

    UFUNCTION(BlueprintCallable, Category = "MiaIA|Project")
    static bool SaveProject(const FString& Path);

    UFUNCTION(BlueprintPure, Category = "MiaIA|Project")
    static FMiaIAProjectInfo GetProjectInfo();

    UFUNCTION(BlueprintCallable, Category = "MiaIA|Interchange")
    static bool ImportOnnx(const FString& Path);

    UFUNCTION(BlueprintCallable, Category = "MiaIA|Interchange")
    static bool ExportOnnx(const FString& Path);

    UFUNCTION(BlueprintCallable, Category = "MiaIA|Network")
    static bool CreateDenseNetwork(
        int32 InputCount,
        int32 HiddenCount,
        int32 HiddenLayers,
        int32 OutputCount);

    UFUNCTION(BlueprintPure, Category = "MiaIA|Network")
    static FMiaIANetworkSnapshot GetNetworkSnapshot();

    UFUNCTION(BlueprintPure, Category = "MiaIA|Network")
    static FMiaIANetworkOverview GetNetworkOverview();

    UFUNCTION(BlueprintCallable, Category = "MiaIA|Network|Inspection")
    static bool InspectNeuron(
        int64 NeuronId,
        int32 MaximumConnections,
        FMiaIANeuronInspection& OutInspection);

    UFUNCTION(BlueprintCallable, Category = "MiaIA|Network|Inspection")
    static bool InspectConnection(
        int64 ConnectionId,
        FMiaIAConnectionInspection& OutInspection);

    UFUNCTION(BlueprintCallable, Category = "MiaIA|Dataset")
    static bool ImportCsvDataset(
        const FString& Path,
        int32 InputCount,
        int32 TargetCount,
        bool bHasHeader = true);

    UFUNCTION(BlueprintCallable, Category = "MiaIA|Training")
    static bool StartTrainingSession(
        int64 EpochCount,
        double LearningRate,
        FMiaIATrainingSessionSnapshot& OutSession);

    UFUNCTION(BlueprintPure, Category = "MiaIA|Training")
    static FMiaIATrainingSessionSnapshot GetTrainingSession();

    UFUNCTION(BlueprintCallable, Category = "MiaIA|Training")
    static bool ResumeTrainingSession();

    UFUNCTION(BlueprintCallable, Category = "MiaIA|Training")
    static bool PauseTrainingSession();

    UFUNCTION(BlueprintCallable, Category = "MiaIA|Training|Breakpoint")
    static bool AddTrainingBreakpoint(
        EMiaIATrainingBreakpointKind Kind,
        EMiaIATrainingDebugPhase Phase,
        int64 TargetId,
        double Threshold,
        FMiaIATrainingBreakpoint& OutBreakpoint);

    UFUNCTION(BlueprintPure, Category = "MiaIA|Training|Breakpoint")
    static TArray<FMiaIATrainingBreakpoint> GetTrainingBreakpoints();

    UFUNCTION(BlueprintCallable, Category = "MiaIA|Training|Breakpoint")
    static bool SetTrainingBreakpointEnabled(
        int64 BreakpointId,
        bool bEnabled);

    UFUNCTION(BlueprintCallable, Category = "MiaIA|Training|Breakpoint")
    static bool RemoveTrainingBreakpoint(int64 BreakpointId);

    UFUNCTION(BlueprintCallable, Category = "MiaIA|Training|Breakpoint")
    static bool ClearTrainingBreakpoints();

    UFUNCTION(BlueprintCallable, Category = "MiaIA|Training|Breakpoint")
    static bool GetLastTrainingBreakpointHit(
        FMiaIATrainingBreakpointHit& OutHit);

    UFUNCTION(BlueprintCallable, Category = "MiaIA|Training|Debug")
    static bool StartSessionDebug(
        FMiaIATrainingDebugSnapshot& OutDebug);

    UFUNCTION(BlueprintCallable, Category = "MiaIA|Training|Debug")
    static bool AdvanceDebugPhase(
        FMiaIATrainingDebugSnapshot& OutDebug);

    UFUNCTION(BlueprintCallable, Category = "MiaIA|Training|Debug")
    static bool CancelDebug();

    UFUNCTION(BlueprintPure, Category = "MiaIA|Training|Debug")
    static FMiaIATrainingDebugSnapshot GetDebugStatus();

    UFUNCTION(BlueprintPure, Category = "MiaIA|Training|Debug")
    static FMiaIANetworkSnapshot GetDebugNetworkSnapshot();

    UFUNCTION(BlueprintCallable, Category = "MiaIA|Training|Debug")
    static bool GetDebugNeuron(
        int64 NeuronId,
        FMiaIATrainingDebugNeuron& OutNeuron);

    UFUNCTION(BlueprintCallable, Category = "MiaIA|Training|Debug")
    static bool GetDebugConnection(
        int64 ConnectionId,
        FMiaIATrainingDebugConnection& OutConnection);
};
