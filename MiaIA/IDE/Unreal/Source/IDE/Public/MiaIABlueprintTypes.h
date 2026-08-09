#pragma once

#include "CoreMinimal.h"
#include "MiaIABlueprintTypes.generated.h"

UENUM(BlueprintType)
enum class EMiaIATrainingDebugPhase : uint8
{
    Idle,
    BeforeForward,
    ForwardComplete,
    BackwardComplete,
    UpdateComplete,
    Verified,
    Committed
};

UENUM(BlueprintType)
enum class EMiaIATrainingSessionStatus : uint8
{
    Idle,
    Active,
    Running,
    Completed,
    Cancelled
};

UENUM(BlueprintType)
enum class EMiaIAActivationType : uint8
{
    Sigmoid,
    ReLU,
    Tanh,
    Linear
};

USTRUCT(BlueprintType)
struct IDE_API FMiaIANeuronSnapshot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    int64 Id{};

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    double Activation{};

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    double Bias{};
};

USTRUCT(BlueprintType)
struct IDE_API FMiaIALayerSnapshot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    int64 Id{};

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    FString Name;

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    int64 Order{};

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    EMiaIAActivationType Activation{
        EMiaIAActivationType::Sigmoid
    };

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    TArray<FMiaIANeuronSnapshot> Neurons;
};

USTRUCT(BlueprintType)
struct IDE_API FMiaIAConnectionSnapshot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    int64 Id{};

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    int64 FromNeuron{};

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    int64 ToNeuron{};

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    double Weight{};
};

USTRUCT(BlueprintType)
struct IDE_API FMiaIANetworkSnapshot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    TArray<FMiaIALayerSnapshot> Layers;

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    TArray<FMiaIAConnectionSnapshot> Connections;
};

USTRUCT(BlueprintType)
struct IDE_API FMiaIALayerOverview
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    int64 Id{};

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    FString Name;

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    int64 Order{};

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    int64 NeuronCount{};

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    EMiaIAActivationType Activation{EMiaIAActivationType::Sigmoid};
};

USTRUCT(BlueprintType)
struct IDE_API FMiaIANetworkOverview
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    TArray<FMiaIALayerOverview> Layers;

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    int64 NeuronCount{};

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    int64 ConnectionCount{};
};

USTRUCT(BlueprintType)
struct IDE_API FMiaIADebugNeuronTelemetry
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    int64 Id{};

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    int64 LayerOrder{};

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    double CandidateActivation{};

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    bool bHasGradients{};

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    double ActivationGradient{};

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    double PreActivationGradient{};

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    double BiasGradient{};

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    bool bHasUpdate{};

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    double Delta{};
};

USTRUCT(BlueprintType)
struct IDE_API FMiaIADebugConnectionTelemetry
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    int64 Id{};

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    int64 FromNeuron{};

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    int64 ToNeuron{};

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    double CandidateWeight{};

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    bool bHasGradient{};

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    double WeightGradient{};

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    bool bHasUpdate{};

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    double Delta{};
};

USTRUCT(BlueprintType)
struct IDE_API FMiaIATrainingDebugSnapshot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    EMiaIATrainingDebugPhase Phase{
        EMiaIATrainingDebugPhase::Idle
    };

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    int64 SampleIndex{};

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    double LearningRate{};

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    bool bHasBeforeEvaluation{};

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    double LossBefore{};

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    bool bHasAfterEvaluation{};

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    double LossAfter{};

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    int64 CandidateLayerCount{};

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    int64 CandidateConnectionCount{};

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    FMiaIANetworkSnapshot CandidateNetwork;

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    TArray<FMiaIADebugNeuronTelemetry> NeuronTelemetry;

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    TArray<FMiaIADebugConnectionTelemetry> ConnectionTelemetry;
};

USTRUCT(BlueprintType)
struct IDE_API FMiaIATrainingSessionSnapshot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    EMiaIATrainingSessionStatus Status{
        EMiaIATrainingSessionStatus::Idle
    };

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    int64 EpochCount{};

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    int64 CurrentEpoch{};

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    int64 NextSampleIndex{};

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    int64 SampleCount{};

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    int64 CompletedSteps{};

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    int64 TotalSteps{};

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    double LearningRate{};
};

USTRUCT(BlueprintType)
struct IDE_API FMiaIATrainingDebugNeuron
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    EMiaIATrainingDebugPhase Phase{
        EMiaIATrainingDebugPhase::Idle
    };

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    int64 Id{};

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    int64 LayerOrder{};

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    double PublicActivation{};

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    double CandidateActivation{};

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    double PublicBias{};

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    double CandidateBias{};

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    bool bHasGradients{};

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    double ActivationGradient{};

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    double PreActivationGradient{};

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    double BiasGradient{};

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    bool bHasUpdate{};

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    double PreviousBias{};

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    double UpdateGradient{};

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    double Delta{};

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    double UpdatedBias{};
};

USTRUCT(BlueprintType)
struct IDE_API FMiaIATrainingDebugConnection
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    EMiaIATrainingDebugPhase Phase{
        EMiaIATrainingDebugPhase::Idle
    };

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    int64 Id{};

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    int64 FromNeuron{};

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    int64 ToNeuron{};

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    double PublicWeight{};

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    double CandidateWeight{};

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    bool bHasGradient{};

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    double WeightGradient{};

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    bool bHasUpdate{};

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    double PreviousWeight{};

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    double UpdateGradient{};

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    double Delta{};

    UPROPERTY(BlueprintReadOnly, Category = "MiaIA")
    double UpdatedWeight{};
};
