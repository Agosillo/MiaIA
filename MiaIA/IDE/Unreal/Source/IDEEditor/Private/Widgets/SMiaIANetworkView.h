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

private:
    FLinearColor ActivationColor(double Activation) const;
    static double DistanceToSegment(
        const FVector2D& Point,
        const FVector2D& Start,
        const FVector2D& End);

    FMiaIANetworkSnapshot Snapshot;
    int64 SelectedNeuronId{-1};
    int64 SelectedConnectionId{-1};
    EMiaIAEditorTheme Theme{EMiaIAEditorTheme::FollowUnreal};
    mutable TMap<int64, FVector2D> NeuronPositions;
    FOnMiaIANeuronSelected OnNeuronSelected;
    FOnMiaIAConnectionSelected OnConnectionSelected;
    FSlateRoundedBoxBrush NeuronBrush;
    FSlateRoundedBoxBrush SelectionBrush;
};
