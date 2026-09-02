#pragma once

#include "Brushes/SlateRoundedBoxBrush.h"
#include "Containers/Set.h"
#include "MiaIABlueprintTypes.h"
#include "Styling/MiaIAEditorTheme.h"
#include "Widgets/MiaIAVisualizationSettings.h"
#include "Widgets/SLeafWidget.h"

DECLARE_DELEGATE_TwoParams(
    FOnMiaIANeuronSelectionChanged,
    const TSet<int64>&,
    int64)
DECLARE_DELEGATE_OneParam(FOnMiaIAConnectionSelected, int64)
DECLARE_DELEGATE_OneParam(FOnMiaIALayerSelected, int64)
DECLARE_DELEGATE_OneParam(FOnMiaIALayerOpenRequested, int64)
DECLARE_DELEGATE_RetVal(bool, FOnMiaIALayerFocusExitRequested)

enum class EMiaIANeuronNavigationDirection : uint8
{
    PreviousNeuron,
    NextNeuron,
    PreviousLayer,
    NextLayer
};

DECLARE_DELEGATE_OneParam(
    FOnMiaIANeuronNavigationRequested,
    EMiaIANeuronNavigationDirection)

class IDESTUDIO_API SMiaIANetworkView final : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SMiaIANetworkView)
    {
    }
        SLATE_EVENT(
            FOnMiaIANeuronSelectionChanged,
            OnNeuronSelectionChanged)
        SLATE_EVENT(FOnMiaIAConnectionSelected, OnConnectionSelected)
        SLATE_EVENT(FOnMiaIALayerSelected, OnLayerSelected)
        SLATE_EVENT(
            FOnMiaIANeuronNavigationRequested,
            OnNeuronNavigationRequested)
        SLATE_EVENT(FOnMiaIALayerOpenRequested, OnLayerOpenRequested)
        SLATE_EVENT(FSimpleDelegate, OnNetworkOpenRequested)
        SLATE_EVENT(
            FOnMiaIALayerFocusExitRequested,
            OnLayerFocusExitRequested)
    SLATE_END_ARGS()

    SMiaIANetworkView();

    void Construct(const FArguments& InArgs);
    void SetSnapshot(const FMiaIANetworkSnapshot& InSnapshot);
    void SetOverview(
        const FMiaIANetworkOverview& InOverview,
        bool bInCompactMode,
        bool bInNetworkAggregateMode);
    void SetDebugSnapshot(const FMiaIATrainingDebugSnapshot& InDebug);
    void SetForwardTraceOverlay(
        const TMap<int64, double>& InActivations,
        const TMap<int64, double>& InContributions,
        const TSet<int64>& InPlaybackConnections,
        bool bInPlaybackActive);
    void SetBackwardTraceOverlay(
        const TMap<int64, double>& InNeuronGradients,
        const TMap<int64, double>& InConnectionGradients,
        const TSet<int64>& InPlaybackConnections,
        bool bInPlaybackActive);
    void SetSignalHealthOverlay(
        const TMap<int64, EMiaIASignalHealthVisualState>& InNeurons,
        const TMap<int64, EMiaIASignalHealthVisualState>& InConnections,
        bool bInActive);
    void SetModelComparisonOverlay(
        const TMap<int64, double>& InBiasDeltas,
        const TMap<int64, double>& InWeightDeltas,
        bool bInActive);
    void SetSelectedNeurons(
        const TSet<int64>& InNeuronIds,
        int64 InPrimaryNeuronId);
    void SetSelectedConnection(int64 InConnectionId);
    void SetSelectedLayer(int64 InLayerId);
    void SetTheme(EMiaIAEditorTheme InTheme);
    void SetVisualizationSettings(
        const FMiaIAVisualizationSettings& InSettings);
    void FitView();
    void ResetLayout();
    void RevealNeuron(int64 NeuronId);

    virtual bool SupportsKeyboardFocus() const override { return true; }
    virtual void Tick(
        const FGeometry& AllottedGeometry,
        const double InCurrentTime,
        const float InDeltaTime) override;
    virtual FVector2D ComputeDesiredSize(float) const override;
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
    virtual FReply OnMouseButtonDoubleClick(
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
    virtual FReply OnKeyDown(
        const FGeometry& MyGeometry,
        const FKeyEvent& KeyEvent) override;
    virtual void OnMouseCaptureLost(
        const FCaptureLostEvent& CaptureLostEvent) override;

private:
    FLinearColor ActivationColor(double Activation) const;
    FLinearColor SignedNeuronColor(double Value, double Maximum) const;
    FLinearColor SignedConnectionColor(double Value, double Maximum) const;
    FLinearColor SignalHealthColor(
        EMiaIASignalHealthVisualState State) const;
    double NeuronMetric(const FMiaIADebugNeuronTelemetry& Telemetry) const;
    double ConnectionMetric(
        const FMiaIADebugConnectionTelemetry& Telemetry) const;
    int32 PaintCompactOverview(
        const FGeometry& AllottedGeometry,
        FSlateWindowElementList& OutDrawElements,
        int32 LayerId) const;
    int32 PaintNetworkAggregate(
        const FGeometry& AllottedGeometry,
        FSlateWindowElementList& OutDrawElements,
        int32 LayerId) const;
    FVector2D NetworkAggregatePosition(const FVector2D& Size) const;
    FVector2D CompactLayerPosition(
        int32 LayerIndex,
        const FVector2D& Size) const;
    FVector2D AutomaticPosition(
        int32 LayerIndex,
        int32 NeuronIndex) const;
    float DisplayNeuronDiameter() const;
    float DisplaySelectionDiameter() const;
    float LayoutUnitSize() const;
    bool ShouldDrawConnection(
        const FMiaIAConnectionSnapshot& Connection) const;
    bool CanApplyDragTranslation(
        const FVector2D& Translation) const;
    FVector2D LayoutPosition(
        const FVector2D& NormalizedPosition,
        const FVector2D& Size) const;
    FVector2D ViewPosition(
        const FVector2D& NormalizedPosition,
        const FVector2D& Size) const;
    FVector2D LocalToNormalized(
        const FVector2D& LocalPosition,
        const FVector2D& Size) const;
    FString BuildLayoutKey() const;
    static double DistanceToSegment(
        const FVector2D& Point,
        const FVector2D& Start,
        const FVector2D& End);
    void CompleteMarqueeSelection();
    void NotifyNeuronSelectionChanged();

    FMiaIANetworkSnapshot Snapshot;
    FMiaIANetworkOverview Overview;
    EMiaIATrainingDebugPhase DebugPhase{EMiaIATrainingDebugPhase::Idle};
    TMap<int64, FMiaIADebugNeuronTelemetry> NeuronTelemetry;
    TMap<int64, FMiaIADebugConnectionTelemetry> ConnectionTelemetry;
    TMap<int64, double> ForwardTraceActivations;
    TMap<int64, double> ForwardTraceContributions;
    TSet<int64> ForwardTracePlaybackConnections;
    TMap<int64, double> BackwardTraceNeuronGradients;
    TMap<int64, double> BackwardTraceConnectionGradients;
    TSet<int64> BackwardTracePlaybackConnections;
    TMap<int64, EMiaIASignalHealthVisualState> SignalHealthNeurons;
    TMap<int64, EMiaIASignalHealthVisualState> SignalHealthConnections;
    TMap<int64, double> ModelComparisonBiasDeltas;
    TMap<int64, double> ModelComparisonWeightDeltas;
    double MaximumNeuronMetric{1.0};
    double MaximumConnectionMetric{1.0};
    double MaximumForwardTraceContribution{1.0};
    double MaximumBackwardTraceNeuronGradient{1.0};
    double MaximumBackwardTraceConnectionGradient{1.0};
    double MaximumModelComparisonBiasDelta{1.0};
    double MaximumModelComparisonWeightDelta{1.0};
    int64 SelectedNeuronId{-1};
    int64 SelectedConnectionId{-1};
    int64 SelectedLayerId{-1};
    int32 SelectionBlinkFrame{-1};
    int64 DraggedNeuronId{-1};
    TSet<int64> SelectedNeuronIds;
    EMiaIAEditorTheme Theme{EMiaIAEditorTheme::FollowUnreal};
    FMiaIAVisualizationSettings VisualizationSettings;
    FString LayoutKey;
    TMap<int64, FVector2D> ManualNeuronPositions;
    FVector2D ViewOffset{FVector2D::ZeroVector};
    FVector2D LastPointerPosition{FVector2D::ZeroVector};
    FVector2D DragStartNormalizedPointer{FVector2D::ZeroVector};
    FVector2D MarqueeStart{FVector2D::ZeroVector};
    FVector2D MarqueeEnd{FVector2D::ZeroVector};
    mutable FVector2D ViewportSize{FVector2D::ZeroVector};
    TMap<int64, FVector2D> DragStartPositions;
    float Zoom{1.0f};
    bool bCompactMode{};
    bool bNetworkAggregateMode{};
    bool bForwardTracePlaybackActive{};
    bool bBackwardTracePlaybackActive{};
    bool bSignalHealthActive{};
    bool bModelComparisonActive{};
    bool bSelectionCursorVisible{true};
    bool bPanning{};
    bool bMarqueeSelecting{};
    bool bMarqueeAdditive{};
    mutable TMap<int64, FVector2D> NeuronPositions;
    mutable TMap<int64, FVector2D> CompactLayerPositions;
    FOnMiaIANeuronSelectionChanged OnNeuronSelectionChanged;
    FOnMiaIAConnectionSelected OnConnectionSelected;
    FOnMiaIALayerSelected OnLayerSelected;
    FOnMiaIANeuronNavigationRequested OnNeuronNavigationRequested;
    FOnMiaIALayerOpenRequested OnLayerOpenRequested;
    FSimpleDelegate OnNetworkOpenRequested;
    FOnMiaIALayerFocusExitRequested OnLayerFocusExitRequested;
    FSlateRoundedBoxBrush NeuronBrush;
    FSlateRoundedBoxBrush SelectionBrush;
    FSlateRoundedBoxBrush NeuronLabelBrush;
};
