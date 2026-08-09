#pragma once

#include "MiaIABlueprintTypes.h"
#include "Styling/MiaIAEditorTheme.h"
#include "Widgets/SMiaIANetworkView.h"
#include "Widgets/SViewport.h"

class FPreviewScene;
class FSceneViewport;
class FMiaIAViewportClient;

class IDESTUDIO_API SMiaIA3DNetworkView final : public SViewport
{
public:
    SLATE_BEGIN_ARGS(SMiaIA3DNetworkView)
    {
    }
        SLATE_EVENT(FOnMiaIANeuronSelected, OnNeuronSelected)
        SLATE_EVENT(FOnMiaIAConnectionSelected, OnConnectionSelected)
    SLATE_END_ARGS()

    virtual ~SMiaIA3DNetworkView() override;

    void Construct(const FArguments& InArgs);
    void SetSnapshot(const FMiaIANetworkSnapshot& InSnapshot);
    void SetOverview(
        const FMiaIANetworkOverview& InOverview,
        bool bInCompactMode);
    void SetDebugSnapshot(const FMiaIATrainingDebugSnapshot& InDebug);
    void SetSelectedNeuron(int64 InNeuronId);
    void SetSelectedConnection(int64 InConnectionId);
    void SetTheme(EMiaIAEditorTheme InTheme);
    void FitView();
    void ResetView();

    virtual void Tick(
        const FGeometry& AllottedGeometry,
        const double InCurrentTime,
        const float InDeltaTime) override;
    virtual int32 OnPaint(
        const FPaintArgs& Args,
        const FGeometry& AllottedGeometry,
        const FSlateRect& MyCullingRect,
        FSlateWindowElementList& OutDrawElements,
        int32 LayerId,
        const FWidgetStyle& InWidgetStyle,
        bool bParentEnabled) const override;
    virtual FReply OnMouseButtonDown(
        const FGeometry& MyGeometry,
        const FPointerEvent& MouseEvent) override;
    virtual FReply OnMouseButtonUp(
        const FGeometry& MyGeometry,
        const FPointerEvent& MouseEvent) override;
    virtual FReply OnMouseMove(
        const FGeometry& MyGeometry,
        const FPointerEvent& MouseEvent) override;
    virtual FReply OnMouseWheel(
        const FGeometry& MyGeometry,
        const FPointerEvent& MouseEvent) override;
    virtual void OnMouseCaptureLost(
        const FCaptureLostEvent& CaptureLostEvent) override;

private:
    struct FNodeRenderData
    {
        int64 Id{-1};
        FVector Position{FVector::ZeroVector};
    };

    struct FConnectionRenderData
    {
        int64 Id{-1};
        FVector From{FVector::ZeroVector};
        FVector To{FVector::ZeroVector};
    };

    void RebuildScene();
    void RebuildDetailedScene();
    void RebuildCompactScene();
    void UpdateCamera();
    bool ProjectToViewport(
        const FVector& WorldPosition,
        FVector2D& OutPosition) const;
    float ProjectedSphereRadius(const FVector& WorldPosition) const;
    bool ProjectedSphereBounds(
        const FVector& WorldPosition,
        FVector2D& OutCenter,
        FVector2D& OutRadius) const;
    bool DeprojectFromViewport(
        const FVector2D& ViewportPosition,
        FVector& OutOrigin,
        FVector& OutDirection) const;
    const FNodeRenderData* FindNodeAt(
        const FVector2D& ViewportPosition) const;
    bool BeginNodeDrag(
        const FVector2D& ViewportPosition,
        const FNodeRenderData& Node);
    void UpdateNodeDrag(const FVector2D& ViewportPosition);
    void SelectAt(const FVector2D& ViewportPosition);
    FLinearColor ActivationColor(double Activation) const;
    FLinearColor SignedNeuronColor(double Value, double Maximum) const;
    FLinearColor SignedConnectionColor(
        double Value,
        double Maximum) const;
    double NeuronMetric(const FMiaIADebugNeuronTelemetry& Telemetry) const;
    double ConnectionMetric(
        const FMiaIADebugConnectionTelemetry& Telemetry) const;
    static double DistanceToSegment(
        const FVector2D& Point,
        const FVector2D& Start,
        const FVector2D& End);

    FMiaIANetworkSnapshot Snapshot;
    FMiaIANetworkOverview Overview;
    EMiaIATrainingDebugPhase DebugPhase{EMiaIATrainingDebugPhase::Idle};
    TMap<int64, FMiaIADebugNeuronTelemetry> NeuronTelemetry;
    TMap<int64, FMiaIADebugConnectionTelemetry> ConnectionTelemetry;
    TMap<int64, FVector> ManualNodePositions;
    TArray<FNodeRenderData> RenderedNodes;
    TArray<FConnectionRenderData> RenderedConnections;
    TUniquePtr<FPreviewScene> PreviewScene;
    TSharedPtr<FMiaIAViewportClient> ViewportClient;
    TSharedPtr<FSceneViewport> SceneViewport;
    FOnMiaIANeuronSelected OnNeuronSelected;
    FOnMiaIAConnectionSelected OnConnectionSelected;
    EMiaIAEditorTheme Theme{EMiaIAEditorTheme::FollowUnreal};
    FVector LookAt{FVector::ZeroVector};
    FVector DragPlaneOrigin{FVector::ZeroVector};
    FVector DragPlaneNormal{FVector::ForwardVector};
    FVector DragOffset{FVector::ZeroVector};
    FVector2D LastPointerPosition{FVector2D::ZeroVector};
    double MaximumNeuronMetric{1.0};
    double MaximumConnectionMetric{1.0};
    float CameraYaw{-90.0f};
    float CameraPitch{0.0f};
    float CameraDistance{1150.0f};
    int64 SelectedNeuronId{-1};
    int64 SelectedConnectionId{-1};
    int64 DraggedNeuronId{-1};
    bool bCompactMode{};
    bool bSceneDirty{true};
    bool bOrbiting{};
    bool bPanning{};
    bool bDraggingNode{};
};
