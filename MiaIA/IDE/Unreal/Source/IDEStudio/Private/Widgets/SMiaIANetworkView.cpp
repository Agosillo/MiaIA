#include "Widgets/SMiaIANetworkView.h"

#include "StudioTopology.h"
#include "Framework/Application/SlateApplication.h"
#include "Input/Events.h"
#include "Rendering/DrawElements.h"
#include "Styling/AppStyle.h"

namespace
{
    constexpr float HorizontalPadding = 72.0f;
    constexpr float VerticalPadding = 58.0f;
    constexpr float BaseNeuronDiameter = 34.0f;
    constexpr float SelectionPadding = 8.0f;
    constexpr float ConnectionSelectionDistance = 8.0f;
    constexpr float MinimumZoom = 0.03f;
    constexpr float MaximumZoom = 4.0f;
    constexpr float ZoomStep = 1.15f;
    constexpr float NetworkAggregateDiameter = 104.0f;
    constexpr float NetworkIndicatorLength = 84.0f;
    constexpr float NetworkIndicatorGap = 8.0f;
    constexpr int32 MaximumNetworkIndicators = 512;
}

SMiaIANetworkView::SMiaIANetworkView()
    : NeuronBrush(FLinearColor::White)
    , SelectionBrush(FLinearColor::White)
{
}

void SMiaIANetworkView::Construct(const FArguments& InArgs)
{
    OnNeuronSelectionChanged = InArgs._OnNeuronSelectionChanged;
    OnConnectionSelected = InArgs._OnConnectionSelected;
    OnLayerSelected = InArgs._OnLayerSelected;
    OnNeuronNavigationRequested =
        InArgs._OnNeuronNavigationRequested;
    OnLayerOpenRequested = InArgs._OnLayerOpenRequested;
    OnNetworkOpenRequested = InArgs._OnNetworkOpenRequested;
    OnLayerFocusExitRequested = InArgs._OnLayerFocusExitRequested;
    SetClipping(EWidgetClipping::ClipToBoundsAlways);
    SetCanTick(false);
}

void SMiaIANetworkView::SetSnapshot(
    const FMiaIANetworkSnapshot& InSnapshot)
{
    Snapshot = InSnapshot;
    const FString newLayoutKey = BuildLayoutKey();

    if (newLayoutKey != LayoutKey)
    {
        LayoutKey = newLayoutKey;
        ManualNeuronPositions.Reset();
        ViewOffset = FVector2D::ZeroVector;
        Zoom = 1.0f;
    }

    Invalidate(EInvalidateWidgetReason::Paint);
}

void SMiaIANetworkView::SetOverview(
    const FMiaIANetworkOverview& InOverview,
    bool bInCompactMode,
    bool bInNetworkAggregateMode)
{
    const int64 previousInputCount = Overview.Layers.IsEmpty()
        ? 0
        : Overview.Layers[0].NeuronCount;
    const int64 previousOutputCount = Overview.Layers.IsEmpty()
        ? 0
        : Overview.Layers.Last().NeuronCount;
    const int64 nextInputCount = InOverview.Layers.IsEmpty()
        ? 0
        : InOverview.Layers[0].NeuronCount;
    const int64 nextOutputCount = InOverview.Layers.IsEmpty()
        ? 0
        : InOverview.Layers.Last().NeuronCount;
    const bool aggregateDimensionsChanged =
        previousInputCount != nextInputCount ||
        previousOutputCount != nextOutputCount;
    const bool modeChanged = bCompactMode != bInCompactMode ||
        bNetworkAggregateMode != bInNetworkAggregateMode;
    Overview = InOverview;
    bCompactMode = bInCompactMode;
    bNetworkAggregateMode = bInCompactMode &&
        bInNetworkAggregateMode;

    if (modeChanged ||
        (bNetworkAggregateMode && aggregateDimensionsChanged))
    {
        ManualNeuronPositions.Reset();
        ViewOffset = FVector2D::ZeroVector;
        Zoom = 1.0f;
        FitView();
    }

    Invalidate(EInvalidateWidgetReason::Paint);
}

void SMiaIANetworkView::SetDebugSnapshot(
    const FMiaIATrainingDebugSnapshot& InDebug)
{
    DebugPhase = InDebug.Phase;
    NeuronTelemetry.Reset();
    ConnectionTelemetry.Reset();
    MaximumNeuronMetric = UE_DOUBLE_SMALL_NUMBER;
    MaximumConnectionMetric = UE_DOUBLE_SMALL_NUMBER;

    for (const FMiaIADebugNeuronTelemetry& telemetry :
        InDebug.NeuronTelemetry)
    {
        NeuronTelemetry.Add(telemetry.Id, telemetry);
        MaximumNeuronMetric = FMath::Max(
            MaximumNeuronMetric,
            FMath::Abs(NeuronMetric(telemetry)));
    }

    for (const FMiaIADebugConnectionTelemetry& telemetry :
        InDebug.ConnectionTelemetry)
    {
        ConnectionTelemetry.Add(telemetry.Id, telemetry);
        MaximumConnectionMetric = FMath::Max(
            MaximumConnectionMetric,
            FMath::Abs(ConnectionMetric(telemetry)));
    }

    Invalidate(EInvalidateWidgetReason::Paint);
}

void SMiaIANetworkView::SetSelectedNeurons(
    const TSet<int64>& InNeuronIds,
    int64 InPrimaryNeuronId)
{
    SelectedNeuronIds = InNeuronIds;
    SelectedNeuronId = SelectedNeuronIds.Contains(InPrimaryNeuronId)
        ? InPrimaryNeuronId
        : -1;

    if (SelectedNeuronIds.Num() > 0)
    {
        SelectedConnectionId = -1;
        SelectedLayerId = -1;
    }

    Invalidate(EInvalidateWidgetReason::Paint);
}

void SMiaIANetworkView::SetSelectedConnection(int64 InConnectionId)
{
    SelectedConnectionId = InConnectionId;

    if (SelectedConnectionId >= 0)
    {
        SelectedNeuronIds.Reset();
        SelectedNeuronId = -1;
        SelectedLayerId = -1;
    }

    Invalidate(EInvalidateWidgetReason::Paint);
}

void SMiaIANetworkView::SetSelectedLayer(int64 InLayerId)
{
    SelectedLayerId = InLayerId;

    if (SelectedLayerId >= 0)
    {
        SelectedNeuronIds.Reset();
        SelectedNeuronId = -1;
        SelectedConnectionId = -1;
    }

    Invalidate(EInvalidateWidgetReason::Paint);
}

void SMiaIANetworkView::SetTheme(EMiaIAEditorTheme InTheme)
{
    Theme = InTheme;
    Invalidate(EInvalidateWidgetReason::Paint);
}

void SMiaIANetworkView::SetVisualizationSettings(
    const FMiaIAVisualizationSettings& InSettings)
{
    const bool layoutChanged =
        VisualizationSettings.Layout.Mode != InSettings.Layout.Mode ||
        VisualizationSettings.Layout.Orientation !=
            InSettings.Layout.Orientation ||
        VisualizationSettings.Layout.Direction !=
            InSettings.Layout.Direction ||
        !FMath::IsNearlyEqual(
            VisualizationSettings.Layout.NeuronGap,
            InSettings.Layout.NeuronGap) ||
        !FMath::IsNearlyEqual(
            VisualizationSettings.Layout.LayerGap,
            InSettings.Layout.LayerGap);
    const bool scaleChanged = !FMath::IsNearlyEqual(
        VisualizationSettings.NeuronScale,
        InSettings.NeuronScale);
    VisualizationSettings = InSettings;
    VisualizationSettings.Layout.NeuronScale =
        VisualizationSettings.NeuronScale;

    if (layoutChanged || scaleChanged)
    {
        ManualNeuronPositions.Reset();
    }

    Invalidate(EInvalidateWidgetReason::Paint);
}

void SMiaIANetworkView::FitView()
{
    if (bCompactMode)
    {
        ViewOffset = FVector2D::ZeroVector;

        if (bNetworkAggregateMode &&
            ViewportSize.X > 0.0f && ViewportSize.Y > 0.0f)
        {
            const int64 inputCount = Overview.Layers.IsEmpty()
                ? 0
                : Overview.Layers[0].NeuronCount;
            const int64 outputCount = Overview.Layers.IsEmpty()
                ? 0
                : Overview.Layers.Last().NeuronCount;
            const int32 visibleCount = static_cast<int32>(FMath::Min<int64>(
                FMath::Max(inputCount, outputCount),
                MaximumNetworkIndicators));
            const float indicatorSpan = visibleCount <= 1
                ? 0.0f
                : static_cast<float>(visibleCount - 1) *
                    NetworkIndicatorGap;
            const float contentWidth = NetworkAggregateDiameter +
                NetworkIndicatorLength * 2.0f;
            const float contentHeight = FMath::Max(
                NetworkAggregateDiameter,
                indicatorSpan);
            const float availableWidth = FMath::Max(
                1.0f,
                ViewportSize.X - HorizontalPadding * 2.0f);
            const float availableHeight = FMath::Max(
                1.0f,
                ViewportSize.Y - VerticalPadding * 2.0f);
            Zoom = FMath::Clamp(
                FMath::Min(1.0f, FMath::Min(
                    availableWidth / FMath::Max(1.0f, contentWidth),
                    availableHeight / FMath::Max(1.0f, contentHeight))),
                MinimumZoom,
                MaximumZoom);
        }
        else
        {
            Zoom = 1.0f;
        }

        Invalidate(EInvalidateWidgetReason::Paint);
        return;
    }

    if (ViewportSize.X <= 0.0f || ViewportSize.Y <= 0.0f ||
        Snapshot.Layers.IsEmpty())
    {
        return;
    }

    FVector2D minimum(FLT_MAX, FLT_MAX);
    FVector2D maximum(-FLT_MAX, -FLT_MAX);
    bool hasNeuron = false;

    for (int32 layerIndex = 0;
        layerIndex < Snapshot.Layers.Num();
        ++layerIndex)
    {
        const FMiaIALayerSnapshot& layer = Snapshot.Layers[layerIndex];

        for (int32 neuronIndex = 0;
            neuronIndex < layer.Neurons.Num();
            ++neuronIndex)
        {
            const FMiaIANeuronSnapshot& neuron = layer.Neurons[neuronIndex];
            const FVector2D* manual = ManualNeuronPositions.Find(neuron.Id);
            const FVector2D normalized = manual
                ? *manual
                : AutomaticPosition(layerIndex, neuronIndex);
            const FVector2D position = LayoutPosition(
                normalized,
                ViewportSize);
            minimum.X = FMath::Min(minimum.X, position.X);
            minimum.Y = FMath::Min(minimum.Y, position.Y);
            maximum.X = FMath::Max(maximum.X, position.X);
            maximum.Y = FMath::Max(maximum.Y, position.Y);
            hasNeuron = true;
        }
    }

    if (!hasNeuron)
    {
        return;
    }

    const float radius = BaseNeuronDiameter * FMath::Clamp(
        VisualizationSettings.NeuronScale,
        0.25f,
        3.0f) * 0.5f;
    minimum -= FVector2D(radius, radius);
    maximum += FVector2D(radius, radius);
    const FVector2D bounds = maximum - minimum;
    const float availableWidth = FMath::Max(
        1.0f,
        static_cast<float>(ViewportSize.X) - HorizontalPadding * 2.0f);
    const float availableHeight = FMath::Max(
        1.0f,
        static_cast<float>(ViewportSize.Y) - VerticalPadding * 2.0f);
    const float horizontalZoom = bounds.X > UE_KINDA_SMALL_NUMBER
        ? availableWidth / static_cast<float>(bounds.X)
        : 1.0f;
    const float verticalZoom = bounds.Y > UE_KINDA_SMALL_NUMBER
        ? availableHeight / static_cast<float>(bounds.Y)
        : 1.0f;
    Zoom = FMath::Clamp(
        FMath::Min(1.0f, FMath::Min(horizontalZoom, verticalZoom)),
        MinimumZoom,
        MaximumZoom);
    const FVector2D layoutCenter = (minimum + maximum) * 0.5f;
    ViewOffset = -(
        layoutCenter - ViewportSize * 0.5f) * Zoom;
    Invalidate(EInvalidateWidgetReason::Paint);
}

void SMiaIANetworkView::ResetLayout()
{
    ManualNeuronPositions.Reset();
    ViewOffset = FVector2D::ZeroVector;
    Zoom = 1.0f;
    FitView();
}

void SMiaIANetworkView::RevealNeuron(int64 NeuronId)
{
    const FVector2D* position = NeuronPositions.Find(NeuronId);

    if (!position || ViewportSize.X <= 0.0f || ViewportSize.Y <= 0.0f)
    {
        return;
    }

    constexpr float revealMargin = 48.0f;
    FVector2D translation = FVector2D::ZeroVector;

    if (position->X < revealMargin)
    {
        translation.X = revealMargin - position->X;
    }
    else if (position->X > ViewportSize.X - revealMargin)
    {
        translation.X = ViewportSize.X - revealMargin - position->X;
    }

    if (position->Y < revealMargin)
    {
        translation.Y = revealMargin - position->Y;
    }
    else if (position->Y > ViewportSize.Y - revealMargin)
    {
        translation.Y = ViewportSize.Y - revealMargin - position->Y;
    }

    if (!translation.IsNearlyZero())
    {
        ViewOffset += translation;
        Invalidate(EInvalidateWidgetReason::Paint);
    }
}

FVector2D SMiaIANetworkView::ComputeDesiredSize(float) const
{
    return FVector2D(640.0f, 420.0f);
}

FLinearColor SMiaIANetworkView::ActivationColor(double Activation) const
{
    const FMiaIAEditorPalette palette =
        FMiaIAEditorTheme::Palette(Theme);
    const float strength = FMath::Clamp(
        static_cast<float>(FMath::Abs(Activation)),
        0.0f,
        1.0f);
    return FLinearColor::LerpUsingHSV(
        palette.InactiveNeuron,
        palette.ActiveNeuron,
        strength);
}

FLinearColor SMiaIANetworkView::SignedNeuronColor(
    double Value,
    double Maximum) const
{
    const FMiaIAEditorPalette palette =
        FMiaIAEditorTheme::Palette(Theme);
    const float strength = FMath::Clamp(
        static_cast<float>(FMath::Abs(Value) / Maximum),
        0.0f,
        1.0f);
    const FLinearColor target = Value >= 0.0
        ? palette.PositiveWeight
        : palette.NegativeWeight;
    return FLinearColor::LerpUsingHSV(
        palette.InactiveNeuron,
        target,
        strength);
}

FLinearColor SMiaIANetworkView::SignedConnectionColor(
    double Value,
    double Maximum) const
{
    const FMiaIAEditorPalette palette =
        FMiaIAEditorTheme::Palette(Theme);
    const float strength = FMath::Clamp(
        static_cast<float>(FMath::Abs(Value) / Maximum),
        0.0f,
        1.0f);
    const FLinearColor target = Value >= 0.0
        ? palette.PositiveWeight
        : palette.NegativeWeight;
    return target.CopyWithNewOpacity(0.15f + strength * 0.85f);
}

double SMiaIANetworkView::NeuronMetric(
    const FMiaIADebugNeuronTelemetry& Telemetry) const
{
    if (DebugPhase == EMiaIATrainingDebugPhase::BackwardComplete &&
        Telemetry.bHasGradients)
    {
        return Telemetry.LayerOrder == 0
            ? Telemetry.ActivationGradient
            : Telemetry.BiasGradient;
    }

    if (DebugPhase == EMiaIATrainingDebugPhase::UpdateComplete &&
        Telemetry.bHasUpdate)
    {
        return Telemetry.Delta;
    }

    if (DebugPhase == EMiaIATrainingDebugPhase::BackwardComplete ||
        DebugPhase == EMiaIATrainingDebugPhase::UpdateComplete)
    {
        return 0.0;
    }

    return Telemetry.CandidateActivation;
}

double SMiaIANetworkView::ConnectionMetric(
    const FMiaIADebugConnectionTelemetry& Telemetry) const
{
    if (DebugPhase == EMiaIATrainingDebugPhase::BackwardComplete &&
        Telemetry.bHasGradient)
    {
        return Telemetry.WeightGradient;
    }

    if (DebugPhase == EMiaIATrainingDebugPhase::UpdateComplete &&
        Telemetry.bHasUpdate)
    {
        return Telemetry.Delta;
    }

    if (DebugPhase == EMiaIATrainingDebugPhase::BackwardComplete ||
        DebugPhase == EMiaIATrainingDebugPhase::UpdateComplete)
    {
        return 0.0;
    }

    return Telemetry.CandidateWeight;
}

int32 SMiaIANetworkView::PaintCompactOverview(
    const FGeometry& AllottedGeometry,
    FSlateWindowElementList& OutDrawElements,
    int32 LayerId) const
{
    if (bNetworkAggregateMode)
    {
        return PaintNetworkAggregate(
            AllottedGeometry,
            OutDrawElements,
            LayerId);
    }

    const FMiaIAEditorPalette palette =
        FMiaIAEditorTheme::Palette(Theme);
    const FVector2D size = AllottedGeometry.GetLocalSize();
    const int32 layerCount = Overview.Layers.Num();
    CompactLayerPositions.Reset();

    FSlateDrawElement::MakeText(
        OutDrawElements,
        LayerId,
        AllottedGeometry.ToPaintGeometry(
            FVector2D(FMath::Max(1.0, size.X - 32.0), 22.0),
            FSlateLayoutTransform(FVector2D(16.0f, 12.0f))),
        FText::Format(
            NSLOCTEXT(
                "MiaIAStudio",
                "CompactTopologyNotice",
                "Compact topology  |  {0} layers  |  {1} neurons  |  {2} connections"),
            FText::AsNumber(layerCount),
            FText::AsNumber(Overview.NeuronCount),
            FText::AsNumber(Overview.ConnectionCount)),
        FAppStyle::GetFontStyle(TEXT("NormalFontBold")),
        ESlateDrawEffect::None,
        palette.SubduedText);

    if (layerCount == 0)
    {
        return LayerId;
    }

    TArray<FVector2D> positions;
    positions.Reserve(layerCount);

    for (int32 index = 0; index < layerCount; ++index)
    {
        const FVector2D position = CompactLayerPosition(index, size);
        positions.Add(position);

        if (index > 0)
        {
            const TArray<FVector2D> points{
                positions[index - 1],
                position
            };
            FSlateDrawElement::MakeLines(
                OutDrawElements,
                LayerId,
                AllottedGeometry.ToPaintGeometry(),
                points,
                ESlateDrawEffect::None,
                palette.PositiveWeight.CopyWithNewOpacity(0.45f),
                true,
                2.0f);
        }
    }

    const bool isVertical = VisualizationSettings.Layout.Orientation ==
        MiaIA::Studio::StudioLayoutOrientation::Vertical;
    const float axisMargin = isVertical
        ? FMath::Min(VerticalPadding, size.Y * 0.5f)
        : FMath::Min(HorizontalPadding, size.X * 0.5f);
    const float axisLength = FMath::Max(
        0.0f,
        (isVertical ? size.Y : size.X) - axisMargin * 2.0f);
    const float layerSpacing = layerCount <= 1
        ? axisLength
        : axisLength * Zoom / static_cast<float>(layerCount - 1);
    const bool showLabels = layerCount <= 40 &&
        layerSpacing >= (isVertical ? 24.0f : 80.0f);
    const int32 nodeLayer = LayerId + 1;

    for (int32 index = 0; index < layerCount; ++index)
    {
        const FVector2D position = positions[index];
        const FMiaIALayerOverview& layer = Overview.Layers[index];
        CompactLayerPositions.Add(layer.Id, position);

        if (layer.Id == SelectedLayerId)
        {
            const float selectionDiameter = BaseNeuronDiameter +
                SelectionPadding;
            FSlateDrawElement::MakeBox(
                OutDrawElements,
                nodeLayer,
                AllottedGeometry.ToPaintGeometry(
                    FVector2D(selectionDiameter, selectionDiameter),
                    FSlateLayoutTransform(
                        position - FVector2D(
                            selectionDiameter * 0.5f,
                            selectionDiameter * 0.5f))),
                &SelectionBrush,
                ESlateDrawEffect::None,
                palette.Selection);
        }

        FSlateDrawElement::MakeBox(
            OutDrawElements,
            nodeLayer + 1,
            AllottedGeometry.ToPaintGeometry(
                FVector2D(BaseNeuronDiameter, BaseNeuronDiameter),
                FSlateLayoutTransform(
                    position - FVector2D(
                        BaseNeuronDiameter * 0.5f,
                        BaseNeuronDiameter * 0.5f))),
            &NeuronBrush,
            ESlateDrawEffect::None,
            palette.ActiveNeuron);

        if (showLabels)
        {
            FSlateDrawElement::MakeText(
                OutDrawElements,
                nodeLayer + 1,
                AllottedGeometry.ToPaintGeometry(
                    FVector2D(90.0f, 18.0f),
                    FSlateLayoutTransform(
                        position + (isVertical
                            ? FVector2D(21.0f, -9.0f)
                            : FVector2D(-45.0f, 21.0f)))),
                FText::Format(
                    NSLOCTEXT(
                        "MiaIAStudio",
                        "CompactLayerLabel",
                        "L{0}  |  {1}"),
                    FText::AsNumber(layer.Order),
                    FText::AsNumber(layer.NeuronCount)),
                FAppStyle::GetFontStyle(TEXT("SmallFont")),
                ESlateDrawEffect::None,
                palette.SubduedText);
        }
    }

    return nodeLayer + 1;
}

int32 SMiaIANetworkView::PaintNetworkAggregate(
    const FGeometry& AllottedGeometry,
    FSlateWindowElementList& OutDrawElements,
    int32 LayerId) const
{
    CompactLayerPositions.Reset();
    const FMiaIAEditorPalette palette =
        FMiaIAEditorTheme::Palette(Theme);
    const FVector2D size = AllottedGeometry.GetLocalSize();
    const FVector2D center = NetworkAggregatePosition(size);
    const float radius = NetworkAggregateDiameter * 0.5f * Zoom;
    const float indicatorLength = NetworkIndicatorLength * Zoom;
    const float indicatorGap = NetworkIndicatorGap * Zoom;
    const int64 inputCount = Overview.Layers.IsEmpty()
        ? 0
        : Overview.Layers[0].NeuronCount;
    const int64 outputCount = Overview.Layers.IsEmpty()
        ? 0
        : Overview.Layers.Last().NeuronCount;
    const auto drawIndicators = [&](int64 Count,
        bool bPositiveSide, const FLinearColor& Color)
    {
        const int32 visibleCount = static_cast<int32>(FMath::Min<int64>(
            FMath::Max<int64>(0, Count),
            MaximumNetworkIndicators));

        for (int32 index = 0; index < visibleCount; ++index)
        {
            const float offset = (static_cast<float>(index) -
                static_cast<float>(visibleCount - 1) * 0.5f) *
                indicatorGap;
            const float direction = bPositiveSide ? 1.0f : -1.0f;
            const FVector2D start = center + FVector2D(
                direction * radius,
                offset);
            const FVector2D end = start + FVector2D(
                direction * indicatorLength,
                0.0f);

            FSlateDrawElement::MakeLines(
                OutDrawElements,
                LayerId,
                AllottedGeometry.ToPaintGeometry(),
                TArray<FVector2D>{start, end},
                ESlateDrawEffect::None,
                Color.CopyWithNewOpacity(0.82f),
                true,
                FMath::Clamp(1.25f * Zoom, 0.6f, 2.0f));
        }
    };

    drawIndicators(inputCount, false, palette.Debug);
    drawIndicators(outputCount, true, palette.ActiveNeuron);

    const int32 nodeLayer = LayerId + 1;
    FSlateDrawElement::MakeBox(
        OutDrawElements,
        nodeLayer,
        AllottedGeometry.ToPaintGeometry(
            FVector2D(radius * 2.0f, radius * 2.0f),
            FSlateLayoutTransform(center - FVector2D(radius, radius))),
        &NeuronBrush,
        ESlateDrawEffect::None,
        palette.Debug);

    return nodeLayer;
}

FVector2D SMiaIANetworkView::NetworkAggregatePosition(
    const FVector2D& Size) const
{
    return Size * 0.5f + ViewOffset;
}

FVector2D SMiaIANetworkView::CompactLayerPosition(
    int32 LayerIndex,
    const FVector2D& Size) const
{
    const int32 layerCount = Overview.Layers.Num();
    float progress = layerCount <= 1
        ? 0.5f
        : static_cast<float>(LayerIndex) /
            static_cast<float>(layerCount - 1);

    if (VisualizationSettings.Layout.Direction ==
        MiaIA::Studio::StudioLayoutDirection::Reverse)
    {
        progress = 1.0f - progress;
    }

    const float horizontalMargin = FMath::Min(
        HorizontalPadding,
        Size.X * 0.5f);
    const float verticalMargin = FMath::Min(
        VerticalPadding,
        Size.Y * 0.5f);
    const FVector2D start(horizontalMargin, verticalMargin);
    const FVector2D end(
        Size.X - horizontalMargin,
        Size.Y - verticalMargin);
    const FVector2D basePosition =
        VisualizationSettings.Layout.Orientation ==
            MiaIA::Studio::StudioLayoutOrientation::Vertical
        ? FVector2D(Size.X * 0.5f, FMath::Lerp(start.Y, end.Y, progress))
        : FVector2D(FMath::Lerp(start.X, end.X, progress), Size.Y * 0.5f);
    const FVector2D center = Size * 0.5f;
    return center + ViewOffset + (basePosition - center) * Zoom;
}

FVector2D SMiaIANetworkView::AutomaticPosition(
    int32 LayerIndex,
    int32 NeuronIndex) const
{
    const int32 layerCount = Snapshot.Layers.Num();
    const int32 neuronCount = Snapshot.Layers.IsValidIndex(LayerIndex)
        ? Snapshot.Layers[LayerIndex].Neurons.Num()
        : 0;
    const MiaIA::Studio::StudioPosition position =
        MiaIA::Studio::StudioTopologyBuilder::DetailedLayoutPosition(
            static_cast<std::size_t>(LayerIndex),
            static_cast<std::size_t>(layerCount),
            static_cast<std::size_t>(NeuronIndex),
            static_cast<std::size_t>(neuronCount),
            VisualizationSettings.Layout);
    return FVector2D(
        static_cast<float>(position.X),
        static_cast<float>(position.Y));
}

float SMiaIANetworkView::DisplayNeuronDiameter() const
{
    return LayoutUnitSize() * FMath::Clamp(
        VisualizationSettings.NeuronScale,
        0.25f,
        3.0f) * Zoom;
}

float SMiaIANetworkView::DisplaySelectionDiameter() const
{
    return DisplayNeuronDiameter() + SelectionPadding;
}

float SMiaIANetworkView::LayoutUnitSize() const
{
    return BaseNeuronDiameter;
}

FVector2D SMiaIANetworkView::LayoutPosition(
    const FVector2D& LayoutCoordinates,
    const FVector2D& Size) const
{
    return Size * 0.5f + LayoutCoordinates * LayoutUnitSize();
}

FVector2D SMiaIANetworkView::ViewPosition(
    const FVector2D& LayoutCoordinates,
    const FVector2D& Size) const
{
    const FVector2D center = Size * 0.5f;
    return center + ViewOffset +
        (LayoutPosition(LayoutCoordinates, Size) - center) * Zoom;
}

FVector2D SMiaIANetworkView::LocalToNormalized(
    const FVector2D& LocalPosition,
    const FVector2D& Size) const
{
    const FVector2D center = Size * 0.5f;
    const FVector2D layoutPosition = center +
        (LocalPosition - center - ViewOffset) / Zoom;
    return (layoutPosition - center) / LayoutUnitSize();
}

bool SMiaIANetworkView::ShouldDrawConnection(
    const FMiaIAConnectionSnapshot& Connection) const
{
    if (VisualizationSettings.ConnectionScale <= UE_KINDA_SMALL_NUMBER)
    {
        return false;
    }

    switch (VisualizationSettings.ConnectionDisplay)
    {
    case EMiaIAConnectionDisplayMode::Selected:
        return Connection.Id == SelectedConnectionId ||
            SelectedNeuronIds.Contains(Connection.FromNeuron) ||
            SelectedNeuronIds.Contains(Connection.ToNeuron);
    case EMiaIAConnectionDisplayMode::All:
    default:
        return true;
    }
}

bool SMiaIANetworkView::CanApplyDragTranslation(
    const FVector2D& Translation) const
{
    const double minimumDistance = FMath::Clamp(
        static_cast<double>(VisualizationSettings.NeuronScale),
        0.25,
        3.0);

    for (const TPair<int64, FVector2D>& dragged : DragStartPositions)
    {
        const FVector2D proposed = dragged.Value + Translation;

        for (int32 layerIndex = 0;
            layerIndex < Snapshot.Layers.Num();
            ++layerIndex)
        {
            const FMiaIALayerSnapshot& layer = Snapshot.Layers[layerIndex];

            for (int32 neuronIndex = 0;
                neuronIndex < layer.Neurons.Num();
                ++neuronIndex)
            {
                const FMiaIANeuronSnapshot& neuron =
                    layer.Neurons[neuronIndex];

                if (DragStartPositions.Contains(neuron.Id))
                {
                    continue;
                }

                const FVector2D* manual =
                    ManualNeuronPositions.Find(neuron.Id);
                const FVector2D other = manual
                    ? *manual
                    : AutomaticPosition(layerIndex, neuronIndex);

                if (FVector2D::Distance(proposed, other) +
                    UE_KINDA_SMALL_NUMBER < minimumDistance)
                {
                    return false;
                }
            }
        }
    }

    return true;
}

FString SMiaIANetworkView::BuildLayoutKey() const
{
    FString key;

    for (const FMiaIALayerSnapshot& layer : Snapshot.Layers)
    {
        key += FString::Printf(TEXT("L%lld:"), layer.Id);

        for (const FMiaIANeuronSnapshot& neuron : layer.Neurons)
        {
            key += FString::Printf(TEXT("%lld,"), neuron.Id);
        }
    }

    return key;
}

double SMiaIANetworkView::DistanceToSegment(
    const FVector2D& Point,
    const FVector2D& Start,
    const FVector2D& End)
{
    const FVector2D segment = End - Start;
    const double lengthSquared = segment.SizeSquared();

    if (lengthSquared <= UE_DOUBLE_SMALL_NUMBER)
    {
        return FVector2D::Distance(Point, Start);
    }

    const double projection = FMath::Clamp(
        FVector2D::DotProduct(Point - Start, segment) / lengthSquared,
        0.0,
        1.0);
    return FVector2D::Distance(Point, Start + segment * projection);
}

int32 SMiaIANetworkView::OnPaint(
    const FPaintArgs&,
    const FGeometry& AllottedGeometry,
    const FSlateRect&,
    FSlateWindowElementList& OutDrawElements,
    int32 LayerId,
    const FWidgetStyle&,
    bool) const
{
    NeuronPositions.Reset();
    ViewportSize = AllottedGeometry.GetLocalSize();
    const FMiaIAEditorPalette palette =
        FMiaIAEditorTheme::Palette(Theme);

    if (bCompactMode)
    {
        return PaintCompactOverview(
            AllottedGeometry,
            OutDrawElements,
            LayerId);
    }

    if (Snapshot.Layers.IsEmpty())
    {
        FSlateDrawElement::MakeText(
            OutDrawElements,
            LayerId,
            AllottedGeometry.ToPaintGeometry(
                FVector2D(320.0f, 24.0f),
                FSlateLayoutTransform(FVector2D(24.0f, 24.0f))),
            NSLOCTEXT(
                "MiaIAStudio",
                "EmptyNetwork",
                "Create or import a network to inspect its topology."),
            FAppStyle::GetFontStyle(TEXT("NormalFont")),
            ESlateDrawEffect::None,
            palette.SubduedText);
        return LayerId;
    }

    const FVector2D size = ViewportSize;
    const int32 layerCount = Snapshot.Layers.Num();

    for (int32 layerIndex = 0; layerIndex < layerCount; ++layerIndex)
    {
        const FMiaIALayerSnapshot& layer = Snapshot.Layers[layerIndex];
        const int32 neuronCount = layer.Neurons.Num();

        for (int32 neuronIndex = 0;
            neuronIndex < neuronCount;
            ++neuronIndex)
        {
            const FMiaIANeuronSnapshot& neuron =
                layer.Neurons[neuronIndex];
            const FVector2D* manual =
                ManualNeuronPositions.Find(neuron.Id);
            const FVector2D normalized = manual
                ? *manual
                : AutomaticPosition(layerIndex, neuronIndex);
            NeuronPositions.Add(
                neuron.Id,
                ViewPosition(normalized, size));
        }
    }

    for (const FMiaIAConnectionSnapshot& connection :
        Snapshot.Connections)
    {
        if (!ShouldDrawConnection(connection))
        {
            continue;
        }

        const FVector2D* from =
            NeuronPositions.Find(connection.FromNeuron);
        const FVector2D* to =
            NeuronPositions.Find(connection.ToNeuron);

        if (!from || !to)
        {
            continue;
        }

        const FMiaIADebugConnectionTelemetry* telemetry =
            ConnectionTelemetry.Find(connection.Id);
        const double displayedValue = telemetry
            ? ConnectionMetric(*telemetry)
            : connection.Weight;
        const bool displaysTelemetry = telemetry &&
            ((DebugPhase == EMiaIATrainingDebugPhase::BackwardComplete &&
                telemetry->bHasGradient) ||
                (DebugPhase == EMiaIATrainingDebugPhase::UpdateComplete &&
                    telemetry->bHasUpdate));
        const float weightStrength = FMath::Clamp(
            static_cast<float>(displaysTelemetry
                ? FMath::Abs(displayedValue) / MaximumConnectionMetric
                : FMath::Abs(displayedValue)),
            0.15f,
            1.0f);
        const bool selected = connection.Id == SelectedConnectionId;
        const FLinearColor connectionColor = selected
            ? palette.Selection
            : displaysTelemetry
                ? SignedConnectionColor(
                    displayedValue,
                    MaximumConnectionMetric)
                : displayedValue >= 0.0
                    ? palette.PositiveWeight.CopyWithNewOpacity(weightStrength)
                    : palette.NegativeWeight.CopyWithNewOpacity(weightStrength);
        const float connectionScale = FMath::Clamp(
            VisualizationSettings.ConnectionScale,
            0.0f,
            2.0f);
        const FLinearColor displayedConnectionColor =
            connectionColor.CopyWithNewOpacity(
                FMath::Clamp(
                    connectionColor.A * connectionScale,
                    0.0f,
                    1.0f));
        const TArray<FVector2D> points{ *from, *to };
        FSlateDrawElement::MakeLines(
            OutDrawElements,
            LayerId,
            AllottedGeometry.ToPaintGeometry(),
            points,
            ESlateDrawEffect::None,
            displayedConnectionColor,
            true,
            (selected ? 4.0f : 1.0f + weightStrength * 1.5f) *
                connectionScale);
    }

    int32 nodeLayer = LayerId + 1;

    for (const FMiaIALayerSnapshot& layer : Snapshot.Layers)
    {
        for (const FMiaIANeuronSnapshot& neuron : layer.Neurons)
        {
            const FVector2D* position = NeuronPositions.Find(neuron.Id);

            if (!position)
            {
                continue;
            }

            const float neuronDiameter = DisplayNeuronDiameter();
            const float selectionDiameter = DisplaySelectionDiameter();

            if (SelectedNeuronIds.Contains(neuron.Id))
            {
                FSlateDrawElement::MakeBox(
                    OutDrawElements,
                    nodeLayer,
                    AllottedGeometry.ToPaintGeometry(
                        FVector2D(selectionDiameter, selectionDiameter),
                        FSlateLayoutTransform(
                            *position - FVector2D(
                                selectionDiameter * 0.5f,
                                selectionDiameter * 0.5f))),
                    &SelectionBrush,
                    ESlateDrawEffect::None,
                    palette.Selection);
            }

            const FMiaIADebugNeuronTelemetry* telemetry =
                NeuronTelemetry.Find(neuron.Id);
            const bool displaysTelemetry = telemetry &&
                ((DebugPhase == EMiaIATrainingDebugPhase::BackwardComplete &&
                    telemetry->bHasGradients) ||
                    (DebugPhase == EMiaIATrainingDebugPhase::UpdateComplete &&
                        telemetry->bHasUpdate));
            const FLinearColor neuronColor = displaysTelemetry
                ? SignedNeuronColor(
                    NeuronMetric(*telemetry),
                    MaximumNeuronMetric)
                : ActivationColor(telemetry
                    ? telemetry->CandidateActivation
                    : neuron.Activation);

            FSlateDrawElement::MakeBox(
                OutDrawElements,
                nodeLayer + 1,
                AllottedGeometry.ToPaintGeometry(
                    FVector2D(neuronDiameter, neuronDiameter),
                    FSlateLayoutTransform(
                        *position - FVector2D(
                            neuronDiameter * 0.5f,
                            neuronDiameter * 0.5f))),
                &NeuronBrush,
                ESlateDrawEffect::None,
                neuronColor);

            const bool showLabel =
                VisualizationSettings.Layout.Mode !=
                    MiaIA::Studio::StudioLayoutMode::Packed &&
                neuronDiameter >= 12.0f;

            if (showLabel)
            {
                FSlateDrawElement::MakeText(
                OutDrawElements,
                nodeLayer + 2,
                AllottedGeometry.ToPaintGeometry(
                    FVector2D(70.0f, 18.0f),
                    FSlateLayoutTransform(
                        *position + FVector2D(
                            -35.0f,
                            neuronDiameter * 0.5f + 5.0f))),
                FText::Format(
                    NSLOCTEXT("MiaIAStudio", "NeuronId", "#{0}"),
                    FText::AsNumber(neuron.Id)),
                FAppStyle::GetFontStyle(TEXT("SmallFont")),
                ESlateDrawEffect::None,
                palette.SubduedText);
            }
        }
    }

    int32 finalLayer = nodeLayer + 2;

    if (bMarqueeSelecting)
    {
        const FVector2D minimum(
            FMath::Min(MarqueeStart.X, MarqueeEnd.X),
            FMath::Min(MarqueeStart.Y, MarqueeEnd.Y));
        const FVector2D maximum(
            FMath::Max(MarqueeStart.X, MarqueeEnd.X),
            FMath::Max(MarqueeStart.Y, MarqueeEnd.Y));
        const FVector2D marqueeSize = maximum - minimum;
        const int32 marqueeLayer = finalLayer + 1;
        FSlateDrawElement::MakeBox(
            OutDrawElements,
            marqueeLayer,
            AllottedGeometry.ToPaintGeometry(
                marqueeSize,
                FSlateLayoutTransform(minimum)),
            FAppStyle::GetBrush(TEXT("WhiteBrush")),
            ESlateDrawEffect::None,
            palette.Selection.CopyWithNewOpacity(0.10f));
        const TArray<FVector2D> borderPoints{
            minimum,
            FVector2D(maximum.X, minimum.Y),
            maximum,
            FVector2D(minimum.X, maximum.Y),
            minimum
        };
        FSlateDrawElement::MakeLines(
            OutDrawElements,
            marqueeLayer + 1,
            AllottedGeometry.ToPaintGeometry(),
            borderPoints,
            ESlateDrawEffect::None,
            palette.Selection,
            true,
            1.5f);
        finalLayer = marqueeLayer + 1;
    }

    return finalLayer;
}

FReply SMiaIANetworkView::OnMouseButtonDown(
    const FGeometry& MyGeometry,
    const FPointerEvent& MouseEvent)
{
    FSlateApplication::Get().SetKeyboardFocus(
        SharedThis(this),
        EFocusCause::Mouse);
    const FKey button = MouseEvent.GetEffectingButton();
    const FVector2D localPosition = MyGeometry.AbsoluteToLocal(
        MouseEvent.GetScreenSpacePosition());

    if (button == EKeys::MiddleMouseButton ||
        button == EKeys::RightMouseButton)
    {
        bPanning = true;
        LastPointerPosition = localPosition;
        return FReply::Handled().CaptureMouse(SharedThis(this));
    }

    if (button != EKeys::LeftMouseButton)
    {
        return FReply::Unhandled();
    }

    if (bCompactMode)
    {
        if (bNetworkAggregateMode)
        {
            const float radius = NetworkAggregateDiameter * 0.5f * Zoom;

            if (FVector2D::Distance(
                localPosition,
                NetworkAggregatePosition(MyGeometry.GetLocalSize())) <= radius)
            {
                OnNetworkOpenRequested.ExecuteIfBound();
            }

            return FReply::Handled();
        }

        int64 closestLayerId = -1;
        float closestDistance = FMath::Max(
            10.0f,
            BaseNeuronDiameter * 0.75f);

        for (const TPair<int64, FVector2D>& entry : CompactLayerPositions)
        {
            const float distance = FVector2D::Distance(
                localPosition,
                entry.Value);

            if (distance <= closestDistance)
            {
                closestLayerId = entry.Key;
                closestDistance = distance;
            }
        }

        if (closestLayerId >= 0)
        {
            OnLayerSelected.ExecuteIfBound(closestLayerId);
        }

        return FReply::Handled();
    }

    for (const TPair<int64, FVector2D>& entry : NeuronPositions)
    {
        if (FVector2D::Distance(localPosition, entry.Value) <=
            FMath::Max(6.0f, DisplaySelectionDiameter() * 0.5f))
        {
            const bool additive = MouseEvent.IsControlDown();

            if (additive)
            {
                if (SelectedNeuronIds.Contains(entry.Key))
                {
                    SelectedNeuronIds.Remove(entry.Key);

                    if (SelectedNeuronId == entry.Key)
                    {
                        SelectedNeuronId = -1;

                        for (const int64 selectedId : SelectedNeuronIds)
                        {
                            SelectedNeuronId = selectedId;
                            break;
                        }
                    }
                }
                else
                {
                    SelectedNeuronIds.Add(entry.Key);
                    SelectedNeuronId = entry.Key;
                }
            }
            else
            {
                if (!SelectedNeuronIds.Contains(entry.Key))
                {
                    SelectedNeuronIds.Reset();
                    SelectedNeuronIds.Add(entry.Key);
                }

                SelectedNeuronId = entry.Key;
            }

            SelectedConnectionId = -1;
            NotifyNeuronSelectionChanged();
            Invalidate(EInvalidateWidgetReason::Paint);

            if (additive || !SelectedNeuronIds.Contains(entry.Key))
            {
                return FReply::Handled();
            }

            DraggedNeuronId = entry.Key;
            LastPointerPosition = localPosition;
            DragStartNormalizedPointer = LocalToNormalized(
                localPosition,
                MyGeometry.GetLocalSize());
            DragStartPositions.Reset();

            for (const int64 selectedId : SelectedNeuronIds)
            {
                if (const FVector2D* selectedPosition =
                    NeuronPositions.Find(selectedId))
                {
                    DragStartPositions.Add(
                        selectedId,
                        LocalToNormalized(
                            *selectedPosition,
                            MyGeometry.GetLocalSize()));
                }
            }

            return FReply::Handled().CaptureMouse(SharedThis(this));
        }
    }

    const FMiaIAConnectionSnapshot* closestConnection = nullptr;
    double closestDistance = ConnectionSelectionDistance;

    for (const FMiaIAConnectionSnapshot& connection :
        Snapshot.Connections)
    {
        if (!ShouldDrawConnection(connection))
        {
            continue;
        }

        const FVector2D* from =
            NeuronPositions.Find(connection.FromNeuron);
        const FVector2D* to =
            NeuronPositions.Find(connection.ToNeuron);

        if (!from || !to)
        {
            continue;
        }

        const double distance = DistanceToSegment(
            localPosition,
            *from,
            *to);

        if (distance <= closestDistance)
        {
            closestConnection = &connection;
            closestDistance = distance;
        }
    }

    if (closestConnection)
    {
        SelectedNeuronIds.Reset();
        SelectedNeuronId = -1;
        SelectedConnectionId = closestConnection->Id;
        OnConnectionSelected.ExecuteIfBound(closestConnection->Id);
        Invalidate(EInvalidateWidgetReason::Paint);
        return FReply::Handled();
    }

    SelectedConnectionId = -1;
    bMarqueeSelecting = true;
    bMarqueeAdditive = MouseEvent.IsControlDown();
    MarqueeStart = localPosition;
    MarqueeEnd = localPosition;
    Invalidate(EInvalidateWidgetReason::Paint);
    return FReply::Handled().CaptureMouse(SharedThis(this));
}

FReply SMiaIANetworkView::OnMouseButtonDoubleClick(
    const FGeometry& MyGeometry,
    const FPointerEvent& MouseEvent)
{
    if (!bCompactMode ||
        MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
    {
        return FReply::Unhandled();
    }

    const FVector2D localPosition = MyGeometry.AbsoluteToLocal(
        MouseEvent.GetScreenSpacePosition());

    if (bNetworkAggregateMode)
    {
        const float radius = NetworkAggregateDiameter * 0.5f * Zoom;

        if (FVector2D::Distance(
            localPosition,
            NetworkAggregatePosition(MyGeometry.GetLocalSize())) <= radius)
        {
            OnNetworkOpenRequested.ExecuteIfBound();
            return FReply::Handled();
        }

        return FReply::Unhandled();
    }

    const float selectionRadius = FMath::Max(
        10.0f,
        BaseNeuronDiameter * 0.75f);
    int64 closestLayerId = -1;
    float closestDistance = selectionRadius;

    for (const TPair<int64, FVector2D>& entry : CompactLayerPositions)
    {
        const float distance = FVector2D::Distance(
            localPosition,
            entry.Value);

        if (distance <= closestDistance)
        {
            closestLayerId = entry.Key;
            closestDistance = distance;
        }
    }

    if (closestLayerId >= 0)
    {
        OnLayerOpenRequested.ExecuteIfBound(closestLayerId);
        return FReply::Handled();
    }

    return FReply::Unhandled();
}

FReply SMiaIANetworkView::OnMouseButtonUp(
    const FGeometry&,
    const FPointerEvent& MouseEvent)
{
    const FKey button = MouseEvent.GetEffectingButton();
    const bool completesNeuronDrag =
        button == EKeys::LeftMouseButton && DraggedNeuronId >= 0;
    const bool completesPan =
        (button == EKeys::MiddleMouseButton ||
            button == EKeys::RightMouseButton) &&
        bPanning;
    const bool completesMarquee =
        button == EKeys::LeftMouseButton && bMarqueeSelecting;

    if (!completesNeuronDrag && !completesPan && !completesMarquee)
    {
        return FReply::Unhandled();
    }

    if (completesMarquee)
    {
        CompleteMarqueeSelection();
    }

    DraggedNeuronId = -1;
    DragStartPositions.Reset();
    bPanning = false;
    bMarqueeSelecting = false;
    return FReply::Handled().ReleaseMouseCapture();
}

FReply SMiaIANetworkView::OnMouseMove(
    const FGeometry& MyGeometry,
    const FPointerEvent& MouseEvent)
{
    if (!HasMouseCapture())
    {
        return FReply::Unhandled();
    }

    const FVector2D localPosition = MyGeometry.AbsoluteToLocal(
        MouseEvent.GetScreenSpacePosition());

    if (bMarqueeSelecting)
    {
        MarqueeEnd = localPosition;
        Invalidate(EInvalidateWidgetReason::Paint);
        return FReply::Handled();
    }

    if (DraggedNeuronId >= 0)
    {
        const FVector2D currentNormalized = LocalToNormalized(
            localPosition,
            MyGeometry.GetLocalSize());
        const FVector2D translation =
            currentNormalized - DragStartNormalizedPointer;

        if (!CanApplyDragTranslation(translation))
        {
            return FReply::Handled();
        }

        for (const TPair<int64, FVector2D>& entry : DragStartPositions)
        {
            ManualNeuronPositions.Add(
                entry.Key,
                entry.Value + translation);
        }

        LastPointerPosition = localPosition;
        Invalidate(EInvalidateWidgetReason::Paint);
        return FReply::Handled();
    }

    if (bPanning)
    {
        ViewOffset += localPosition - LastPointerPosition;
        LastPointerPosition = localPosition;
        Invalidate(EInvalidateWidgetReason::Paint);
        return FReply::Handled();
    }

    return FReply::Unhandled();
}

FReply SMiaIANetworkView::OnMouseWheel(
    const FGeometry& MyGeometry,
    const FPointerEvent& MouseEvent)
{
    const float previousZoom = Zoom;
    const float zoomFactor = FMath::Pow(
        ZoomStep,
        MouseEvent.GetWheelDelta());
    Zoom = FMath::Clamp(
        previousZoom * zoomFactor,
        MinimumZoom,
        MaximumZoom);

    if (FMath::IsNearlyEqual(previousZoom, Zoom))
    {
        return FReply::Handled();
    }

    const FVector2D localPosition = MyGeometry.AbsoluteToLocal(
        MouseEvent.GetScreenSpacePosition());
    const FVector2D relativePointer =
        localPosition - MyGeometry.GetLocalSize() * 0.5f;
    ViewOffset = relativePointer -
        (relativePointer - ViewOffset) * (Zoom / previousZoom);
    Invalidate(EInvalidateWidgetReason::Paint);
    return FReply::Handled();
}

FReply SMiaIANetworkView::OnKeyDown(
    const FGeometry& MyGeometry,
    const FKeyEvent& KeyEvent)
{
    const FKey key = KeyEvent.GetKey();
    const bool isVertical = VisualizationSettings.Layout.Orientation ==
        MiaIA::Studio::StudioLayoutOrientation::Vertical;
    const bool isReverse = VisualizationSettings.Layout.Direction ==
        MiaIA::Studio::StudioLayoutDirection::Reverse;

    if (key == EKeys::Enter && bCompactMode && bNetworkAggregateMode)
    {
        OnNetworkOpenRequested.ExecuteIfBound();
        return FReply::Handled();
    }

    if (key == EKeys::Enter && bCompactMode && SelectedLayerId >= 0)
    {
        OnLayerOpenRequested.ExecuteIfBound(SelectedLayerId);
        return FReply::Handled();
    }

    if (key == EKeys::Escape &&
        OnLayerFocusExitRequested.IsBound() &&
        OnLayerFocusExitRequested.Execute())
    {
        return FReply::Handled();
    }

    if (key == EKeys::Up)
    {
        OnNeuronNavigationRequested.ExecuteIfBound(
            isVertical
                ? (isReverse
                    ? EMiaIANeuronNavigationDirection::NextLayer
                    : EMiaIANeuronNavigationDirection::PreviousLayer)
                : EMiaIANeuronNavigationDirection::PreviousNeuron);
        return FReply::Handled();
    }

    if (key == EKeys::Down)
    {
        OnNeuronNavigationRequested.ExecuteIfBound(
            isVertical
                ? (isReverse
                    ? EMiaIANeuronNavigationDirection::PreviousLayer
                    : EMiaIANeuronNavigationDirection::NextLayer)
                : EMiaIANeuronNavigationDirection::NextNeuron);
        return FReply::Handled();
    }

    if (key == EKeys::Left)
    {
        OnNeuronNavigationRequested.ExecuteIfBound(
            isVertical
                ? EMiaIANeuronNavigationDirection::PreviousNeuron
                : (isReverse
                    ? EMiaIANeuronNavigationDirection::NextLayer
                    : EMiaIANeuronNavigationDirection::PreviousLayer));
        return FReply::Handled();
    }

    if (key == EKeys::Right)
    {
        OnNeuronNavigationRequested.ExecuteIfBound(
            isVertical
                ? EMiaIANeuronNavigationDirection::NextNeuron
                : (isReverse
                    ? EMiaIANeuronNavigationDirection::PreviousLayer
                    : EMiaIANeuronNavigationDirection::NextLayer));
        return FReply::Handled();
    }

    return SLeafWidget::OnKeyDown(MyGeometry, KeyEvent);
}

void SMiaIANetworkView::OnMouseCaptureLost(
    const FCaptureLostEvent& CaptureLostEvent)
{
    DraggedNeuronId = -1;
    DragStartPositions.Reset();
    bPanning = false;
    bMarqueeSelecting = false;
    Invalidate(EInvalidateWidgetReason::Paint);
    SLeafWidget::OnMouseCaptureLost(CaptureLostEvent);
}

void SMiaIANetworkView::CompleteMarqueeSelection()
{
    const FVector2D minimum(
        FMath::Min(MarqueeStart.X, MarqueeEnd.X),
        FMath::Min(MarqueeStart.Y, MarqueeEnd.Y));
    const FVector2D maximum(
        FMath::Max(MarqueeStart.X, MarqueeEnd.X),
        FMath::Max(MarqueeStart.Y, MarqueeEnd.Y));
    TSet<int64> selection = bMarqueeAdditive
        ? SelectedNeuronIds
        : TSet<int64>{};
    int64 primaryNeuronId = bMarqueeAdditive
        ? SelectedNeuronId
        : -1;

    for (const TPair<int64, FVector2D>& entry : NeuronPositions)
    {
        if (entry.Value.X >= minimum.X &&
            entry.Value.X <= maximum.X &&
            entry.Value.Y >= minimum.Y &&
            entry.Value.Y <= maximum.Y)
        {
            selection.Add(entry.Key);
            primaryNeuronId = entry.Key;
        }
    }

    SelectedNeuronIds = MoveTemp(selection);
    SelectedNeuronId = SelectedNeuronIds.Contains(primaryNeuronId)
        ? primaryNeuronId
        : -1;
    NotifyNeuronSelectionChanged();
    Invalidate(EInvalidateWidgetReason::Paint);
}

void SMiaIANetworkView::NotifyNeuronSelectionChanged()
{
    OnNeuronSelectionChanged.ExecuteIfBound(
        SelectedNeuronIds,
        SelectedNeuronId);
}
