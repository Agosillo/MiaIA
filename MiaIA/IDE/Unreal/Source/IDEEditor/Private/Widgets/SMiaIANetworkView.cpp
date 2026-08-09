#include "Widgets/SMiaIANetworkView.h"

#include "Framework/Application/SlateApplication.h"
#include "Input/Events.h"
#include "Rendering/DrawElements.h"
#include "Styling/AppStyle.h"

namespace
{
    constexpr float HorizontalPadding = 72.0f;
    constexpr float VerticalPadding = 58.0f;
    constexpr float NeuronDiameter = 34.0f;
    constexpr float SelectionDiameter = 42.0f;
    constexpr float ConnectionSelectionDistance = 8.0f;
    constexpr float MinimumZoom = 0.35f;
    constexpr float MaximumZoom = 4.0f;
    constexpr float ZoomStep = 1.15f;
}

SMiaIANetworkView::SMiaIANetworkView()
    : NeuronBrush(FLinearColor::White, NeuronDiameter * 0.5f)
    , SelectionBrush(FLinearColor::White, SelectionDiameter * 0.5f)
{
}

void SMiaIANetworkView::Construct(const FArguments& InArgs)
{
    OnNeuronSelected = InArgs._OnNeuronSelected;
    OnConnectionSelected = InArgs._OnConnectionSelected;
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
    bool bInCompactMode)
{
    const bool modeChanged = bCompactMode != bInCompactMode;
    Overview = InOverview;
    bCompactMode = bInCompactMode;

    if (modeChanged)
    {
        ManualNeuronPositions.Reset();
        ViewOffset = FVector2D::ZeroVector;
        Zoom = 1.0f;
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

void SMiaIANetworkView::SetSelectedNeuron(int64 InNeuronId)
{
    SelectedNeuronId = InNeuronId;
    Invalidate(EInvalidateWidgetReason::Paint);
}

void SMiaIANetworkView::SetSelectedConnection(int64 InConnectionId)
{
    SelectedConnectionId = InConnectionId;
    Invalidate(EInvalidateWidgetReason::Paint);
}

void SMiaIANetworkView::SetTheme(EMiaIAEditorTheme InTheme)
{
    Theme = InTheme;
    Invalidate(EInvalidateWidgetReason::Paint);
}

void SMiaIANetworkView::FitView()
{
    if (bCompactMode)
    {
        ViewOffset = FVector2D::ZeroVector;
        Zoom = 1.0f;
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
            const FVector2D position =
                LayoutPosition(normalized, ViewportSize);
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
    ViewOffset = ViewportSize * 0.5f -
        (minimum + maximum) * 0.5f * Zoom;
    Invalidate(EInvalidateWidgetReason::Paint);
}

void SMiaIANetworkView::ResetLayout()
{
    ManualNeuronPositions.Reset();
    ViewOffset = FVector2D::ZeroVector;
    Zoom = 1.0f;
    FitView();
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
    const FMiaIAEditorPalette palette =
        FMiaIAEditorTheme::Palette(Theme);
    const FVector2D size = AllottedGeometry.GetLocalSize();
    const int32 layerCount = Overview.Layers.Num();

    FSlateDrawElement::MakeText(
        OutDrawElements,
        LayerId,
        AllottedGeometry.ToPaintGeometry(
            FVector2D(FMath::Max(1.0, size.X - 32.0), 22.0),
            FSlateLayoutTransform(FVector2D(16.0f, 12.0f))),
        FText::Format(
            NSLOCTEXT(
                "MiaIAEditor",
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

    const float aspect = FMath::Max(
        0.5f,
        static_cast<float>(size.X / FMath::Max(1.0, size.Y)));
    const int32 columns = FMath::Max(
        1,
        FMath::CeilToInt(FMath::Sqrt(layerCount * aspect)));
    const int32 rows = FMath::Max(
        1,
        FMath::CeilToInt(
            static_cast<float>(layerCount) /
            static_cast<float>(columns)));
    TArray<FVector2D> positions;
    positions.Reserve(layerCount);

    for (int32 index = 0; index < layerCount; ++index)
    {
        const int32 row = index / columns;
        const int32 positionInRow = index % columns;
        const int32 column = row % 2 == 0
            ? positionInRow
            : columns - positionInRow - 1;
        const FVector2D normalized(
            columns <= 1
                ? 0.5f
                : static_cast<float>(column) /
                    static_cast<float>(columns - 1),
            rows <= 1
                ? 0.5f
                : static_cast<float>(row) /
                    static_cast<float>(rows - 1));
        FVector2D position = ViewPosition(normalized, size);
        position.Y += 18.0f;
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

    const bool showLabels = layerCount <= 40 || Zoom >= 1.5f;
    const int32 nodeLayer = LayerId + 1;

    for (int32 index = 0; index < layerCount; ++index)
    {
        const FVector2D position = positions[index];
        const FMiaIALayerOverview& layer = Overview.Layers[index];
        FSlateDrawElement::MakeBox(
            OutDrawElements,
            nodeLayer,
            AllottedGeometry.ToPaintGeometry(
                FVector2D(NeuronDiameter, NeuronDiameter),
                FSlateLayoutTransform(
                    position - FVector2D(
                        NeuronDiameter * 0.5f,
                        NeuronDiameter * 0.5f))),
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
                        position + FVector2D(-45.0f, 21.0f))),
                FText::Format(
                    NSLOCTEXT(
                        "MiaIAEditor",
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

FVector2D SMiaIANetworkView::AutomaticPosition(
    int32 LayerIndex,
    int32 NeuronIndex) const
{
    const int32 layerCount = Snapshot.Layers.Num();
    const int32 neuronCount = Snapshot.Layers.IsValidIndex(LayerIndex)
        ? Snapshot.Layers[LayerIndex].Neurons.Num()
        : 0;
    const float x = layerCount <= 1
        ? 0.5f
        : static_cast<float>(LayerIndex) /
            static_cast<float>(layerCount - 1);
    const float y = neuronCount <= 1
        ? 0.5f
        : static_cast<float>(NeuronIndex) /
            static_cast<float>(neuronCount - 1);
    return FVector2D(x, y);
}

FVector2D SMiaIANetworkView::LayoutPosition(
    const FVector2D& NormalizedPosition,
    const FVector2D& Size) const
{
    const FVector2D usableSize(
        FMath::Max(1.0f, static_cast<float>(Size.X) -
            HorizontalPadding * 2.0f),
        FMath::Max(1.0f, static_cast<float>(Size.Y) -
            VerticalPadding * 2.0f));
    return FVector2D(HorizontalPadding, VerticalPadding) +
        NormalizedPosition * usableSize;
}

FVector2D SMiaIANetworkView::ViewPosition(
    const FVector2D& NormalizedPosition,
    const FVector2D& Size) const
{
    return ViewOffset + LayoutPosition(NormalizedPosition, Size) * Zoom;
}

FVector2D SMiaIANetworkView::LocalToNormalized(
    const FVector2D& LocalPosition,
    const FVector2D& Size) const
{
    const FVector2D layoutPosition =
        (LocalPosition - ViewOffset) / Zoom;
    const FVector2D usableSize(
        FMath::Max(1.0f, static_cast<float>(Size.X) -
            HorizontalPadding * 2.0f),
        FMath::Max(1.0f, static_cast<float>(Size.Y) -
            VerticalPadding * 2.0f));
    return (layoutPosition -
        FVector2D(HorizontalPadding, VerticalPadding)) / usableSize;
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
                "MiaIAEditor",
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
        const TArray<FVector2D> points{ *from, *to };
        FSlateDrawElement::MakeLines(
            OutDrawElements,
            LayerId,
            AllottedGeometry.ToPaintGeometry(),
            points,
            ESlateDrawEffect::None,
            connectionColor,
            true,
            selected ? 4.0f : 1.0f + weightStrength * 1.5f);
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

            if (neuron.Id == SelectedNeuronId)
            {
                FSlateDrawElement::MakeBox(
                    OutDrawElements,
                    nodeLayer,
                    AllottedGeometry.ToPaintGeometry(
                        FVector2D(SelectionDiameter, SelectionDiameter),
                        FSlateLayoutTransform(
                            *position - FVector2D(
                                SelectionDiameter * 0.5f,
                                SelectionDiameter * 0.5f))),
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
                    FVector2D(NeuronDiameter, NeuronDiameter),
                    FSlateLayoutTransform(
                        *position - FVector2D(
                            NeuronDiameter * 0.5f,
                            NeuronDiameter * 0.5f))),
                &NeuronBrush,
                ESlateDrawEffect::None,
                neuronColor);

            FSlateDrawElement::MakeText(
                OutDrawElements,
                nodeLayer + 2,
                AllottedGeometry.ToPaintGeometry(
                    FVector2D(70.0f, 18.0f),
                    FSlateLayoutTransform(
                        *position + FVector2D(-35.0f, 22.0f))),
                FText::Format(
                    NSLOCTEXT("MiaIAEditor", "NeuronId", "#{0}"),
                    FText::AsNumber(neuron.Id)),
                FAppStyle::GetFontStyle(TEXT("SmallFont")),
                ESlateDrawEffect::None,
                palette.SubduedText);
        }
    }

    return nodeLayer + 2;
}

FReply SMiaIANetworkView::OnMouseButtonDown(
    const FGeometry& MyGeometry,
    const FPointerEvent& MouseEvent)
{
    const FKey button = MouseEvent.GetEffectingButton();
    const FVector2D localPosition = MyGeometry.AbsoluteToLocal(
        MouseEvent.GetScreenSpacePosition());

    if (button == EKeys::MiddleMouseButton)
    {
        bPanning = true;
        LastPointerPosition = localPosition;
        return FReply::Handled().CaptureMouse(SharedThis(this));
    }

    if (button != EKeys::LeftMouseButton)
    {
        return FReply::Unhandled();
    }

    for (const TPair<int64, FVector2D>& entry : NeuronPositions)
    {
        if (FVector2D::Distance(localPosition, entry.Value) <=
            SelectionDiameter * 0.5f)
        {
            SelectedNeuronId = entry.Key;
            SelectedConnectionId = -1;
            DraggedNeuronId = entry.Key;
            LastPointerPosition = localPosition;
            OnNeuronSelected.ExecuteIfBound(entry.Key);
            Invalidate(EInvalidateWidgetReason::Paint);
            return FReply::Handled().CaptureMouse(SharedThis(this));
        }
    }

    const FMiaIAConnectionSnapshot* closestConnection = nullptr;
    double closestDistance = ConnectionSelectionDistance;

    for (const FMiaIAConnectionSnapshot& connection :
        Snapshot.Connections)
    {
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
        SelectedNeuronId = -1;
        SelectedConnectionId = closestConnection->Id;
        OnConnectionSelected.ExecuteIfBound(closestConnection->Id);
        Invalidate(EInvalidateWidgetReason::Paint);
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
        button == EKeys::MiddleMouseButton && bPanning;

    if (!completesNeuronDrag && !completesPan)
    {
        return FReply::Unhandled();
    }

    DraggedNeuronId = -1;
    bPanning = false;
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

    if (DraggedNeuronId >= 0)
    {
        ManualNeuronPositions.Add(
            DraggedNeuronId,
            LocalToNormalized(localPosition, MyGeometry.GetLocalSize()));
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
    ViewOffset = localPosition -
        (localPosition - ViewOffset) * (Zoom / previousZoom);
    Invalidate(EInvalidateWidgetReason::Paint);
    return FReply::Handled();
}

void SMiaIANetworkView::OnMouseCaptureLost(
    const FCaptureLostEvent& CaptureLostEvent)
{
    DraggedNeuronId = -1;
    bPanning = false;
    SLeafWidget::OnMouseCaptureLost(CaptureLostEvent);
}
