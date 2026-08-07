#pragma once

#include "Brushes/SlateRoundedBoxBrush.h"
#include "MiaIABlueprintTypes.h"
#include "Styling/MiaIAEditorTheme.h"
#include "Widgets/SLeafWidget.h"

DECLARE_DELEGATE_OneParam(FOnMiaIANeuronSelected, int64)
DECLARE_DELEGATE_OneParam(FOnMiaIAConnectionSelected, int64)

class SMiaIANetworkView final : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SMiaIANetworkView)
    {
    }
        SLATE_EVENT(FOnMiaIANeuronSelected, OnNeuronSelected)
        SLATE_EVENT(FOnMiaIAConnectionSelected, OnConnectionSelected)
    SLATE_END_ARGS()

    SMiaIANetworkView();

    void Construct(const FArguments& InArgs);
    void SetSnapshot(const FMiaIANetworkSnapshot& InSnapshot);
    void SetSelectedNeuron(int64 InNeuronId);
    void SetSelectedConnection(int64 InConnectionId);
    void SetTheme(EMiaIAEditorTheme InTheme);
    void FitView();
    void ResetLayout();

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
    FLinearColor ActivationColor(double Activation) const;
    FVector2D AutomaticPosition(
        int32 LayerIndex,
        int32 NeuronIndex) const;
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

    FMiaIANetworkSnapshot Snapshot;
    int64 SelectedNeuronId{-1};
    int64 SelectedConnectionId{-1};
    int64 DraggedNeuronId{-1};
    EMiaIAEditorTheme Theme{EMiaIAEditorTheme::FollowUnreal};
    FString LayoutKey;
    TMap<int64, FVector2D> ManualNeuronPositions;
    FVector2D ViewOffset{FVector2D::ZeroVector};
    FVector2D LastPointerPosition{FVector2D::ZeroVector};
    mutable FVector2D ViewportSize{FVector2D::ZeroVector};
    float Zoom{1.0f};
    bool bPanning{};
    mutable TMap<int64, FVector2D> NeuronPositions;
    FOnMiaIANeuronSelected OnNeuronSelected;
    FOnMiaIAConnectionSelected OnConnectionSelected;
    FSlateRoundedBoxBrush NeuronBrush;
    FSlateRoundedBoxBrush SelectionBrush;
};
