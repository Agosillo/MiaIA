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
}

SMiaIANetworkView::SMiaIANetworkView()
    : NeuronBrush(FLinearColor::White, NeuronDiameter * 0.5f)
    , SelectionBrush(
        FLinearColor(0.12f, 0.48f, 0.92f, 1.0f),
        SelectionDiameter * 0.5f)
{
}

void SMiaIANetworkView::Construct(const FArguments& InArgs)
{
    OnNeuronSelected = InArgs._OnNeuronSelected;
    SetCanTick(false);
}

void SMiaIANetworkView::SetSnapshot(
    const FMiaIANetworkSnapshot& InSnapshot)
{
    Snapshot = InSnapshot;
    Invalidate(EInvalidateWidgetReason::Paint);
}

void SMiaIANetworkView::SetSelectedNeuron(int64 InNeuronId)
{
    SelectedNeuronId = InNeuronId;
    Invalidate(EInvalidateWidgetReason::Paint);
}

FVector2D SMiaIANetworkView::ComputeDesiredSize(float) const
{
    return FVector2D(640.0f, 420.0f);
}

FLinearColor SMiaIANetworkView::ActivationColor(double Activation) const
{
    const float strength = FMath::Clamp(
        static_cast<float>(FMath::Abs(Activation)),
        0.0f,
        1.0f);
    const FLinearColor inactive(0.20f, 0.22f, 0.25f, 1.0f);
    const FLinearColor active(0.12f, 0.72f, 0.31f, 1.0f);
    return FLinearColor::LerpUsingHSV(inactive, active, strength);
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
            FLinearColor(0.55f, 0.57f, 0.62f, 1.0f));
        return LayerId;
    }

    const FVector2D size = AllottedGeometry.GetLocalSize();
    const float usableWidth = FMath::Max(
        1.0f,
        static_cast<float>(size.X) - HorizontalPadding * 2.0f);
    const float usableHeight = FMath::Max(
        1.0f,
        static_cast<float>(size.Y) - VerticalPadding * 2.0f);
    const int32 layerCount = Snapshot.Layers.Num();

    for (int32 layerIndex = 0; layerIndex < layerCount; ++layerIndex)
    {
        const FMiaIALayerSnapshot& layer = Snapshot.Layers[layerIndex];
        const float x = layerCount == 1
            ? static_cast<float>(size.X) * 0.5f
            : HorizontalPadding + usableWidth *
                static_cast<float>(layerIndex) /
                static_cast<float>(layerCount - 1);
        const int32 neuronCount = layer.Neurons.Num();

        for (int32 neuronIndex = 0;
            neuronIndex < neuronCount;
            ++neuronIndex)
        {
            const float y = neuronCount == 1
                ? static_cast<float>(size.Y) * 0.5f
                : VerticalPadding + usableHeight *
                    static_cast<float>(neuronIndex) /
                    static_cast<float>(neuronCount - 1);
            NeuronPositions.Add(
                layer.Neurons[neuronIndex].Id,
                FVector2D(x, y));
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

        const float weightStrength = FMath::Clamp(
            static_cast<float>(FMath::Abs(connection.Weight)),
            0.15f,
            1.0f);
        const FLinearColor connectionColor = connection.Weight >= 0.0
            ? FLinearColor(0.24f, 0.52f, 0.86f, weightStrength)
            : FLinearColor(0.86f, 0.30f, 0.28f, weightStrength);
        const TArray<FVector2D> points{ *from, *to };
        FSlateDrawElement::MakeLines(
            OutDrawElements,
            LayerId,
            AllottedGeometry.ToPaintGeometry(),
            points,
            ESlateDrawEffect::None,
            connectionColor,
            true,
            1.0f + weightStrength * 1.5f);
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
                    &SelectionBrush);
            }

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
                ActivationColor(neuron.Activation));

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
                FLinearColor(0.62f, 0.64f, 0.69f, 1.0f));
        }
    }

    return nodeLayer + 2;
}

FReply SMiaIANetworkView::OnMouseButtonDown(
    const FGeometry& MyGeometry,
    const FPointerEvent& MouseEvent)
{
    if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
    {
        return FReply::Unhandled();
    }

    const FVector2D localPosition = MyGeometry.AbsoluteToLocal(
        MouseEvent.GetScreenSpacePosition());

    for (const TPair<int64, FVector2D>& entry : NeuronPositions)
    {
        if (FVector2D::Distance(localPosition, entry.Value) <=
            SelectionDiameter * 0.5f)
        {
            SelectedNeuronId = entry.Key;
            OnNeuronSelected.ExecuteIfBound(entry.Key);
            Invalidate(EInvalidateWidgetReason::Paint);
            return FReply::Handled();
        }
    }

    return FReply::Unhandled();
}
