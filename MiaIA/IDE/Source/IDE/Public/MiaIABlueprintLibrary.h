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
    UFUNCTION(BlueprintCallable, Category = "MiaIA|Network")
    static bool CreateDenseNetwork(
        int32 InputCount,
        int32 HiddenCount,
        int32 HiddenLayers,
        int32 OutputCount);

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

    UFUNCTION(BlueprintCallable, Category = "MiaIA|Training|Debug")
    static bool GetDebugNeuron(
        int64 NeuronId,
        FMiaIATrainingDebugNeuron& OutNeuron);

    UFUNCTION(BlueprintCallable, Category = "MiaIA|Training|Debug")
    static bool GetDebugConnection(
        int64 ConnectionId,
        FMiaIATrainingDebugConnection& OutConnection);
};
