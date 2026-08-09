#include "Widgets/SMiaIA3DNetworkView.h"

#include "StudioTopology.h"
#include "Components/Viewport.h"
#include "DynamicMeshBuilder.h"
#include "Engine/Engine.h"
#include "Engine/EngineTypes.h"
#include "Input/Events.h"
#include "Materials/Material.h"
#include "PreviewScene.h"
#include "Rendering/DrawElements.h"
#include "SceneManagement.h"
#include "SceneView.h"
#include "Slate/SceneViewport.h"
#include "Styling/AppStyle.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"

namespace
{
    struct FMiaIAViewportSphere
    {
        FVector Position{FVector::ZeroVector};
        FLinearColor Color{FLinearColor::White};
        float Radius{};
    };

    struct FMiaIAViewportLine
    {
        FVector From{FVector::ZeroVector};
        FVector To{FVector::ZeroVector};
        FLinearColor Color{FLinearColor::White};
        float Radius{};
    };
}

class FMiaIAViewportClient final : public FUMGViewportClient
{
public:
    explicit FMiaIAViewportClient(FPreviewScene* InPreviewScene)
        : FUMGViewportClient(InPreviewScene)
    {
        FEngineShowFlags showFlags(ESFIM_Game);
        showFlags.DisableAdvancedFeatures();
        showFlags.SetAntiAliasing(true);
        showFlags.SetTemporalAA(false);
        showFlags.SetMotionBlur(false);
        showFlags.SetBloom(false);
        showFlags.SetTonemapper(false);
        SetEngineShowFlags(showFlags);
    }

    const FMatrix& ViewProjectionMatrix() const
    {
        return CachedViewProjectionMatrix;
    }

    float HorizontalFieldOfView() const
    {
        return ViewInfo.FOV;
    }

    void ResetPrimitives()
    {
        Spheres.Reset();
        Lines.Reset();
    }

    void ReservePrimitives(int32 PointCount, int32 LineCount)
    {
        Spheres.Reserve(PointCount);
        Lines.Reserve(LineCount);
    }

    void AddSphere(
        const FVector& Position,
        const FLinearColor& Color,
        float Radius)
    {
        Spheres.Add({Position, Color, Radius});
    }

    void AddLine(
        const FVector& From,
        const FVector& To,
        const FLinearColor& Color,
        float Thickness)
    {
        const float sourceOpacity = FMath::Clamp(
            Color.A,
            0.0f,
            1.0f);
        FLinearColor opaqueColor = FMath::Lerp(
            BackgroundColor,
            Color.CopyWithNewOpacity(1.0f),
            sourceOpacity);
        opaqueColor.A = 1.0f;
        const float radius = 0.35f + Thickness * 0.30f;
        Lines.Add({From, To, opaqueColor, radius});
    }

    using FUMGViewportClient::Draw;

    virtual void Draw(
        const FSceneView*,
        FPrimitiveDrawInterface* PDI) override
    {
        UMaterial* vertexColorMaterial = ResolveVertexColorMaterial();

        if ((Spheres.IsEmpty() && Lines.IsEmpty()) ||
            !vertexColorMaterial)
        {
            return;
        }

        const int32 sphereSides = Spheres.Num() <= 250
            ? 24
            : Spheres.Num() <= 1000
                ? 14
                : 10;
        const int32 sphereRings = FMath::Max(5, sphereSides / 2);
        const int32 cylinderSides = Lines.Num() <= 1000 ? 10 : 6;
        FDynamicMeshBuilder meshBuilder(PDI->View->GetFeatureLevel());
        meshBuilder.ReserveVertices(
            Spheres.Num() *
                (sphereSides + 1) * (sphereRings + 1) +
            Lines.Num() * (cylinderSides + 1) * 2);
        meshBuilder.ReserveTriangles(
            Spheres.Num() * sphereSides * sphereRings * 2 +
            Lines.Num() * cylinderSides * 2);

        for (const FMiaIAViewportLine& line : Lines)
        {
            AddCylinderGeometry(
                meshBuilder,
                line,
                cylinderSides);
        }

        for (const FMiaIAViewportSphere& sphere : Spheres)
        {
            AddSphereGeometry(
                meshBuilder,
                sphere,
                sphereSides,
                sphereRings);
        }

        meshBuilder.Draw(
            PDI,
            FMatrix::Identity,
            vertexColorMaterial->GetRenderProxy(),
            SDPG_World,
            false,
            false);
    }

private:
    UMaterial* ResolveVertexColorMaterial()
    {
        if (GEngine && GEngine->VertexColorMaterial)
        {
            return GEngine->VertexColorMaterial;
        }

        if (!bAttemptedRuntimeMaterialLoad)
        {
            bAttemptedRuntimeMaterialLoad = true;
            RuntimeVertexColorMaterial.Reset(LoadObject<UMaterial>(
                nullptr,
                TEXT("/Engine/EngineDebugMaterials/VertexColorMaterial.VertexColorMaterial")));

            if (!RuntimeVertexColorMaterial.IsValid())
            {
                UE_LOG(
                    LogTemp,
                    Warning,
                    TEXT("MiaIA Studio could not load the runtime vertex-color material; 3D topology geometry is unavailable."));
            }
        }

        return RuntimeVertexColorMaterial.Get();
    }

    static void AddSphereGeometry(
        FDynamicMeshBuilder& MeshBuilder,
        const FMiaIAViewportSphere& Sphere,
        int32 Sides,
        int32 Rings)
    {
        int32 firstGeneratedVertex = INDEX_NONE;

        for (int32 ring = 0; ring <= Rings; ++ring)
        {
            const float latitude =
                static_cast<float>(ring) / Rings * UE_PI;
            const float sinLatitude = FMath::Sin(latitude);
            const float cosLatitude = FMath::Cos(latitude);

            for (int32 side = 0; side <= Sides; ++side)
            {
                const float longitude =
                    static_cast<float>(side) / Sides * 2.0f * UE_PI;
                const float sinLongitude = FMath::Sin(longitude);
                const float cosLongitude = FMath::Cos(longitude);
                const FVector normal(
                    sinLatitude * cosLongitude,
                    sinLatitude * sinLongitude,
                    cosLatitude);
                const FVector tangentX(
                    -sinLongitude,
                    cosLongitude,
                    0.0);
                const FVector tangentY = FVector::CrossProduct(
                    normal,
                    tangentX).GetSafeNormal();

                const int32 vertexIndex = MeshBuilder.AddVertex(
                    FVector3f(Sphere.Position + normal * Sphere.Radius),
                    FVector2f(
                        static_cast<float>(side) / Sides,
                        static_cast<float>(ring) / Rings),
                    FVector3f(tangentX),
                    FVector3f(tangentY),
                    FVector3f(normal),
                    ShadedColor(Sphere.Color, normal));

                if (firstGeneratedVertex == INDEX_NONE)
                {
                    firstGeneratedVertex = vertexIndex;
                }
            }
        }

        for (int32 ring = 0; ring < Rings; ++ring)
        {
            for (int32 side = 0; side < Sides; ++side)
            {
                const int32 current = firstGeneratedVertex +
                    ring * (Sides + 1) + side;
                const int32 nextRing = current + Sides + 1;
                MeshBuilder.AddTriangle(
                    current,
                    nextRing,
                    current + 1);
                MeshBuilder.AddTriangle(
                    current + 1,
                    nextRing,
                    nextRing + 1);
            }
        }
    }

    static void AddCylinderGeometry(
        FDynamicMeshBuilder& MeshBuilder,
        const FMiaIAViewportLine& Line,
        int32 Sides)
    {
        const FVector direction = (Line.To - Line.From).GetSafeNormal();

        if (direction.IsNearlyZero())
        {
            return;
        }

        const FVector reference = FMath::Abs(direction.Z) < 0.9
            ? FVector::UpVector
            : FVector::ForwardVector;
        const FVector axisX = FVector::CrossProduct(
            reference,
            direction).GetSafeNormal();
        const FVector axisY = FVector::CrossProduct(
            direction,
            axisX).GetSafeNormal();
        int32 firstGeneratedVertex = INDEX_NONE;

        for (int32 endpoint = 0; endpoint < 2; ++endpoint)
        {
            const FVector center = endpoint == 0 ? Line.From : Line.To;

            for (int32 side = 0; side <= Sides; ++side)
            {
                const float angle =
                    static_cast<float>(side) / Sides * 2.0f * UE_PI;
                const FVector normal =
                    axisX * FMath::Cos(angle) +
                    axisY * FMath::Sin(angle);
                const FVector tangentY = FVector::CrossProduct(
                    normal,
                    direction).GetSafeNormal();
                const int32 vertexIndex = MeshBuilder.AddVertex(
                    FVector3f(center + normal * Line.Radius),
                    FVector2f(
                        static_cast<float>(side) / Sides,
                        static_cast<float>(endpoint)),
                    FVector3f(direction),
                    FVector3f(tangentY),
                    FVector3f(normal),
                    ShadedColor(Line.Color, normal));

                if (firstGeneratedVertex == INDEX_NONE)
                {
                    firstGeneratedVertex = vertexIndex;
                }
            }
        }

        const int32 secondEndpoint = firstGeneratedVertex + Sides + 1;

        for (int32 side = 0; side < Sides; ++side)
        {
            const int32 start = firstGeneratedVertex + side;
            const int32 end = secondEndpoint + side;
            MeshBuilder.AddTriangle(start, start + 1, end);
            MeshBuilder.AddTriangle(start + 1, end + 1, end);
        }
    }

    static FColor ShadedColor(
        const FLinearColor& Color,
        const FVector& Normal)
    {
        const FVector lightDirection = FVector(
            -0.35,
            -0.45,
            0.82).GetSafeNormal();
        const float brightness = 0.50f + 0.42f * static_cast<float>(
            FMath::Max(
                0.0,
                FVector::DotProduct(Normal, lightDirection)));
        const float highlight = 0.08f * FMath::Pow(
            FMath::Max(0.0f, brightness - 0.50f) / 0.42f,
            10.0f);
        return FLinearColor(
            FMath::Min(1.0f, Color.R * brightness + highlight),
            FMath::Min(1.0f, Color.G * brightness + highlight),
            FMath::Min(1.0f, Color.B * brightness + highlight),
            1.0f).ToFColor(false);
    }

    TArray<FMiaIAViewportSphere> Spheres;
    TArray<FMiaIAViewportLine> Lines;
    TStrongObjectPtr<UMaterial> RuntimeVertexColorMaterial;
    bool bAttemptedRuntimeMaterialLoad{};
};

namespace
{
    constexpr float LayerSpacing = 620.0f;
    constexpr float LayerDepthSpacing = 260.0f;
    constexpr float NeuronDepthSpacing = 90.0f;
    constexpr float LayerPlaneSize = 520.0f;
    constexpr float NeuronSphereRadius = 24.0f;
    constexpr float CompactSphereRadius = 34.0f;
    constexpr float NodeSelectionDistance = 17.0f;
    constexpr float ConnectionSelectionDistance = 7.0f;
    constexpr float MinimumCameraDistance = 80.0f;
    constexpr float MaximumCameraDistance = 12000.0f;
    constexpr float OrbitSpeed = 0.28f;
    constexpr float PanSpeed = 0.0015f;
    constexpr float ZoomStep = 1.15f;

    FVector WorldPosition(
        const MiaIA::Studio::StudioPosition& Position2D,
        const MiaIA::Studio::StudioPosition& Position3D)
    {
        return FVector(
            (Position2D.X - 0.5) * LayerSpacing,
            (Position3D.Z - 0.5) * LayerDepthSpacing +
                (Position3D.X - 0.5) * NeuronDepthSpacing,
            (0.5 - Position2D.Y) * LayerPlaneSize);
    }
}

SMiaIA3DNetworkView::~SMiaIA3DNetworkView()
{
    SceneViewport.Reset();
    ViewportClient.Reset();
    PreviewScene.Reset();
}

void SMiaIA3DNetworkView::Construct(const FArguments& InArgs)
{
    OnNeuronSelected = InArgs._OnNeuronSelected;
    OnConnectionSelected = InArgs._OnConnectionSelected;

    SViewport::FArguments viewportArguments;
    viewportArguments
        .IgnoreTextureAlpha(false)
        .EnableBlending(false);
    SViewport::Construct(viewportArguments);
    SetClipping(EWidgetClipping::ClipToBoundsAlways);

    PreviewScene = MakeUnique<FPreviewScene>(
        FPreviewScene::ConstructionValues()
            .SetCreateDefaultLighting(false)
            .SetCreatePhysicsScene(false)
            .SetEditor(false)
            .SetTransactional(false));
    ViewportClient = MakeShareable(
        new FMiaIAViewportClient(PreviewScene.Get()));
    ViewportClient->SetBackgroundColor(
        FMiaIAEditorTheme::Palette(Theme).Background);
    SceneViewport = FSceneViewport::Create(
        ViewportClient,
        SharedThis(this));
    SceneViewport->SetGammaOverride(1.0f);

    UpdateCamera();
    RebuildScene();
}

void SMiaIA3DNetworkView::SetSnapshot(
    const FMiaIANetworkSnapshot& InSnapshot)
{
    Snapshot = InSnapshot;

    TSet<int64> currentNeuronIds;

    for (const FMiaIALayerSnapshot& layer : Snapshot.Layers)
    {
        for (const FMiaIANeuronSnapshot& neuron : layer.Neurons)
        {
            currentNeuronIds.Add(neuron.Id);
        }
    }

    for (auto iterator = ManualNodePositions.CreateIterator(); iterator; ++iterator)
    {
        if (!currentNeuronIds.Contains(iterator.Key()))
        {
            iterator.RemoveCurrent();
        }
    }

    bSceneDirty = true;
}

void SMiaIA3DNetworkView::SetOverview(
    const FMiaIANetworkOverview& InOverview,
    bool bInCompactMode)
{
    const bool modeChanged = bCompactMode != bInCompactMode;
    Overview = InOverview;
    bCompactMode = bInCompactMode;
    bSceneDirty = true;

    if (modeChanged)
    {
        FitView();
    }
}

void SMiaIA3DNetworkView::SetDebugSnapshot(
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

    bSceneDirty = true;
}

void SMiaIA3DNetworkView::SetSelectedNeuron(int64 InNeuronId)
{
    SelectedNeuronId = InNeuronId;
    bSceneDirty = true;
}

void SMiaIA3DNetworkView::SetSelectedConnection(int64 InConnectionId)
{
    SelectedConnectionId = InConnectionId;
    bSceneDirty = true;
}

void SMiaIA3DNetworkView::SetTheme(EMiaIAEditorTheme InTheme)
{
    Theme = InTheme;

    if (ViewportClient.IsValid())
    {
        ViewportClient->SetBackgroundColor(
            FMiaIAEditorTheme::Palette(Theme).Background);
    }

    bSceneDirty = true;
}

void SMiaIA3DNetworkView::FitView()
{
    if (bSceneDirty && ViewportClient.IsValid())
    {
        RebuildScene();
    }

    FBox sceneBounds(EForceInit::ForceInit);
    float primitiveRadius = NeuronSphereRadius;

    if (bCompactMode)
    {
        primitiveRadius = CompactSphereRadius;

        for (int32 layerIndex = 0;
            layerIndex < Overview.Layers.Num();
            ++layerIndex)
        {
            const MiaIA::Studio::StudioPosition position2D =
                MiaIA::Studio::StudioTopologyBuilder::DetailedPosition2D(
                    layerIndex,
                    Overview.Layers.Num(),
                    0,
                    1);
            const MiaIA::Studio::StudioPosition position3D =
                MiaIA::Studio::StudioTopologyBuilder::DetailedPosition3D(
                    layerIndex,
                    Overview.Layers.Num(),
                    0,
                    1);
            sceneBounds += WorldPosition(position2D, position3D);
        }
    }
    else
    {
        for (const FNodeRenderData& node : RenderedNodes)
        {
            sceneBounds += node.Position;
        }
    }

    LookAt = sceneBounds.IsValid
        ? sceneBounds.GetCenter()
        : FVector::ZeroVector;
    CameraYaw = -90.0f;
    CameraPitch = 0.0f;

    if (sceneBounds.IsValid && ViewportClient.IsValid())
    {
        const FIntPoint viewportSize = SceneViewport.IsValid()
            ? SceneViewport->GetSizeXY()
            : FIntPoint::ZeroValue;
        const float aspectRatio = viewportSize.X > 0 && viewportSize.Y > 0
            ? static_cast<float>(viewportSize.X) / viewportSize.Y
            : 16.0f / 9.0f;
        const FVector extent = sceneBounds.GetExtent();
        const float halfWidth = static_cast<float>(extent.X) +
            primitiveRadius;
        const float halfHeight = static_cast<float>(extent.Z) +
            primitiveRadius;
        const float halfDepth = static_cast<float>(extent.Y) +
            primitiveRadius;
        const float halfHorizontalFov = FMath::DegreesToRadians(
            FMath::Clamp(
                ViewportClient->HorizontalFieldOfView(),
                10.0f,
                170.0f) * 0.5f);
        const float horizontalTangent = FMath::Tan(halfHorizontalFov);
        const float verticalTangent = horizontalTangent /
            FMath::Max(aspectRatio, 0.1f);
        constexpr float fillFraction = 0.82f;
        const float widthDistance = halfWidth /
            FMath::Max(horizontalTangent, 0.01f);
        const float heightDistance = halfHeight /
            FMath::Max(verticalTangent, 0.01f);
        CameraDistance = FMath::Clamp(
            FMath::Max(widthDistance, heightDistance) /
                fillFraction + halfDepth,
            MinimumCameraDistance,
            MaximumCameraDistance);
    }
    else
    {
        CameraDistance = 700.0f;
    }

    UpdateCamera();
}

void SMiaIA3DNetworkView::ResetView()
{
    ManualNodePositions.Reset();
    bSceneDirty = true;
    FitView();
}

void SMiaIA3DNetworkView::Tick(
    const FGeometry& AllottedGeometry,
    const double InCurrentTime,
    const float InDeltaTime)
{
    if (!SceneViewport.IsValid() || !ViewportClient.IsValid())
    {
        return;
    }

    if (bSceneDirty)
    {
        RebuildScene();
    }

    SceneViewport->Invalidate();
    SceneViewport->Tick(
        AllottedGeometry,
        InCurrentTime,
        InDeltaTime);
    ViewportClient->Tick(InDeltaTime);
}

int32 SMiaIA3DNetworkView::OnPaint(
    const FPaintArgs& Args,
    const FGeometry& AllottedGeometry,
    const FSlateRect& MyCullingRect,
    FSlateWindowElementList& OutDrawElements,
    int32 LayerId,
    const FWidgetStyle& InWidgetStyle,
    bool bParentEnabled) const
{
    const int32 viewportLayer = SViewport::OnPaint(
        Args,
        AllottedGeometry,
        MyCullingRect,
        OutDrawElements,
        LayerId,
        InWidgetStyle,
        bParentEnabled);

    if (bCompactMode ||
        RenderedNodes.IsEmpty() ||
        !SceneViewport.IsValid())
    {
        return viewportLayer;
    }

    const FIntPoint viewportSize = SceneViewport->GetSizeXY();
    const FVector2D localSize = AllottedGeometry.GetLocalSize();

    if (viewportSize.X <= 0 ||
        viewportSize.Y <= 0 ||
        localSize.X <= 0.0f ||
        localSize.Y <= 0.0f)
    {
        return viewportLayer;
    }

    const FVector2D viewportToLocal(
        localSize.X / viewportSize.X,
        localSize.Y / viewportSize.Y);
    const FMiaIAEditorPalette palette =
        FMiaIAEditorTheme::Palette(Theme);
    const bool showLabels = RenderedNodes.Num() <= 500;
    int32 overlayLayer = viewportLayer + 1;

    for (const FNodeRenderData& node : RenderedNodes)
    {
        FVector2D projectedPosition;

        if (!ProjectToViewport(node.Position, projectedPosition))
        {
            continue;
        }

        const FVector2D localPosition(
            projectedPosition.X * viewportToLocal.X,
            projectedPosition.Y * viewportToLocal.Y);
        const float projectedRadius = ProjectedSphereRadius(
            node.Position) *
            static_cast<float>((viewportToLocal.X + viewportToLocal.Y) * 0.5);

        if (node.Id == SelectedNeuronId)
        {
            FVector2D selectionCenter = localPosition;
            FVector2D selectionRadius(
                projectedRadius,
                projectedRadius);
            FVector2D viewportSphereCenter;
            FVector2D viewportSphereRadius;

            if (ProjectedSphereBounds(
                node.Position,
                viewportSphereCenter,
                viewportSphereRadius))
            {
                selectionCenter = FVector2D(
                    viewportSphereCenter.X * viewportToLocal.X,
                    viewportSphereCenter.Y * viewportToLocal.Y);
                selectionRadius = FVector2D(
                    viewportSphereRadius.X * viewportToLocal.X,
                    viewportSphereRadius.Y * viewportToLocal.Y);
            }

            selectionRadius.X = FMath::Max(
                12.0,
                selectionRadius.X + 4.0);
            selectionRadius.Y = FMath::Max(
                12.0,
                selectionRadius.Y + 4.0);
            constexpr int32 selectionSegments = 40;
            TArray<FVector2D> selectionPoints;
            selectionPoints.Reserve(selectionSegments + 1);

            for (int32 segment = 0;
                segment <= selectionSegments;
                ++segment)
            {
                const float angle =
                    static_cast<float>(segment) /
                    selectionSegments * 2.0f * UE_PI;
                selectionPoints.Add(
                    selectionCenter + FVector2D(
                        FMath::Cos(angle) * selectionRadius.X,
                        FMath::Sin(angle) * selectionRadius.Y));
            }

            FSlateDrawElement::MakeLines(
                OutDrawElements,
                overlayLayer,
                AllottedGeometry.ToPaintGeometry(),
                selectionPoints,
                ESlateDrawEffect::None,
                palette.Selection,
                true,
                3.0f);
        }

        if (showLabels)
        {
            const FVector2D labelPosition = localPosition + FVector2D(
                -35.0f,
                FMath::Max(10.0f, projectedRadius) + 5.0f);
            const FText label = FText::Format(
                NSLOCTEXT("MiaIAStudio", "NeuronId3D", "#{0}"),
                FText::AsNumber(node.Id));
            FSlateDrawElement::MakeText(
                OutDrawElements,
                overlayLayer + 1,
                AllottedGeometry.ToPaintGeometry(
                    FVector2D(70.0f, 18.0f),
                    FSlateLayoutTransform(
                        labelPosition + FVector2D(1.0f, 1.0f))),
                label,
                FAppStyle::GetFontStyle(TEXT("SmallFont")),
                ESlateDrawEffect::None,
                FLinearColor::Black.CopyWithNewOpacity(0.75f));
            FSlateDrawElement::MakeText(
                OutDrawElements,
                overlayLayer + 2,
                AllottedGeometry.ToPaintGeometry(
                    FVector2D(70.0f, 18.0f),
                    FSlateLayoutTransform(labelPosition)),
                label,
                FAppStyle::GetFontStyle(TEXT("SmallFont")),
                ESlateDrawEffect::None,
                palette.SubduedText);
        }
    }

    return showLabels ? overlayLayer + 2 : overlayLayer;
}

FReply SMiaIA3DNetworkView::OnMouseButtonDown(
    const FGeometry& MyGeometry,
    const FPointerEvent& MouseEvent)
{
    const FKey button = MouseEvent.GetEffectingButton();
    const FVector2D localPosition = MyGeometry.AbsoluteToLocal(
        MouseEvent.GetScreenSpacePosition());
    LastPointerPosition = localPosition;

    if (button == EKeys::RightMouseButton)
    {
        bOrbiting = true;
        return FReply::Handled().CaptureMouse(SharedThis(this));
    }

    if (button == EKeys::MiddleMouseButton)
    {
        bPanning = true;
        return FReply::Handled().CaptureMouse(SharedThis(this));
    }

    if (button == EKeys::LeftMouseButton && SceneViewport.IsValid())
    {
        const FVector2D localSize = MyGeometry.GetLocalSize();
        const FIntPoint viewportSize = SceneViewport->GetSizeXY();

        if (localSize.X > 0.0f && localSize.Y > 0.0f)
        {
            const FVector2D viewportPosition(
                localPosition.X * viewportSize.X / localSize.X,
                localPosition.Y * viewportSize.Y / localSize.Y);
            const FNodeRenderData* node = bCompactMode
                ? nullptr
                : FindNodeAt(viewportPosition);

            if (node)
            {
                const FNodeRenderData nodeValue = *node;
                SelectedNeuronId = nodeValue.Id;
                SelectedConnectionId = -1;
                OnNeuronSelected.ExecuteIfBound(nodeValue.Id);
                const bool startedDrag = BeginNodeDrag(
                    viewportPosition,
                    nodeValue);
                RebuildScene();
                FReply reply = FReply::Handled();

                if (startedDrag)
                {
                    reply.CaptureMouse(SharedThis(this));
                }

                return reply;
            }

            SelectAt(viewportPosition);
        }

        return FReply::Handled();
    }

    return SViewport::OnMouseButtonDown(MyGeometry, MouseEvent);
}

FReply SMiaIA3DNetworkView::OnMouseButtonUp(
    const FGeometry& MyGeometry,
    const FPointerEvent& MouseEvent)
{
    const FKey button = MouseEvent.GetEffectingButton();
    const bool finishesOrbit =
        button == EKeys::RightMouseButton && bOrbiting;
    const bool finishesPan =
        button == EKeys::MiddleMouseButton && bPanning;
    const bool finishesNodeDrag =
        button == EKeys::LeftMouseButton && bDraggingNode;

    if (!finishesOrbit && !finishesPan && !finishesNodeDrag)
    {
        return SViewport::OnMouseButtonUp(MyGeometry, MouseEvent);
    }

    bOrbiting = false;
    bPanning = false;
    bDraggingNode = false;
    DraggedNeuronId = -1;
    return FReply::Handled().ReleaseMouseCapture();
}

FReply SMiaIA3DNetworkView::OnMouseMove(
    const FGeometry& MyGeometry,
    const FPointerEvent& MouseEvent)
{
    if (!HasMouseCapture() ||
        (!bOrbiting && !bPanning && !bDraggingNode))
    {
        return SViewport::OnMouseMove(MyGeometry, MouseEvent);
    }

    const FVector2D localPosition = MyGeometry.AbsoluteToLocal(
        MouseEvent.GetScreenSpacePosition());
    const FVector2D delta = localPosition - LastPointerPosition;
    LastPointerPosition = localPosition;

    if (bDraggingNode && SceneViewport.IsValid())
    {
        const FVector2D localSize = MyGeometry.GetLocalSize();
        const FIntPoint viewportSize = SceneViewport->GetSizeXY();

        if (localSize.X > 0.0f && localSize.Y > 0.0f)
        {
            UpdateNodeDrag(FVector2D(
                localPosition.X * viewportSize.X / localSize.X,
                localPosition.Y * viewportSize.Y / localSize.Y));
        }
    }
    else if (bOrbiting)
    {
        CameraYaw += delta.X * OrbitSpeed;
        CameraPitch = FMath::Clamp(
            CameraPitch + delta.Y * OrbitSpeed,
            -80.0f,
            80.0f);
    }
    else
    {
        const FRotationMatrix cameraRotation(
            FRotator(CameraPitch, CameraYaw, 0.0f));
        const float scale = CameraDistance * PanSpeed;
        LookAt += cameraRotation.GetScaledAxis(EAxis::Y) *
            (-delta.X * scale);
        LookAt += cameraRotation.GetScaledAxis(EAxis::Z) *
            (delta.Y * scale);
    }

    UpdateCamera();
    return FReply::Handled();
}

FReply SMiaIA3DNetworkView::OnMouseWheel(
    const FGeometry&,
    const FPointerEvent& MouseEvent)
{
    CameraDistance = FMath::Clamp(
        CameraDistance / FMath::Pow(
            ZoomStep,
            MouseEvent.GetWheelDelta()),
        MinimumCameraDistance,
        MaximumCameraDistance);
    UpdateCamera();
    return FReply::Handled();
}

void SMiaIA3DNetworkView::OnMouseCaptureLost(
    const FCaptureLostEvent& CaptureLostEvent)
{
    bOrbiting = false;
    bPanning = false;
    bDraggingNode = false;
    DraggedNeuronId = -1;
    SViewport::OnMouseCaptureLost(CaptureLostEvent);
}

void SMiaIA3DNetworkView::RebuildScene()
{
    if (!PreviewScene.IsValid() || !ViewportClient.IsValid())
    {
        return;
    }

    ViewportClient->ResetPrimitives();
    RenderedNodes.Reset();
    RenderedConnections.Reset();

    if (bCompactMode)
    {
        RebuildCompactScene();
    }
    else
    {
        RebuildDetailedScene();
    }

    bSceneDirty = false;

    if (SceneViewport.IsValid())
    {
        SceneViewport->Invalidate();
    }
}

void SMiaIA3DNetworkView::RebuildDetailedScene()
{
    const FMiaIAEditorPalette palette =
        FMiaIAEditorTheme::Palette(Theme);
    TMap<int64, FVector> positions;
    int32 neuronCount = 0;

    for (const FMiaIALayerSnapshot& layer : Snapshot.Layers)
    {
        neuronCount += layer.Neurons.Num();
    }

    positions.Reserve(neuronCount);
    RenderedNodes.Reserve(neuronCount);
    ViewportClient->ReservePrimitives(
        neuronCount,
        Snapshot.Connections.Num());

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
            const MiaIA::Studio::StudioPosition position2D =
                MiaIA::Studio::StudioTopologyBuilder::DetailedPosition2D(
                    layerIndex,
                    Snapshot.Layers.Num(),
                    neuronIndex,
                    layer.Neurons.Num());
            const MiaIA::Studio::StudioPosition position3D =
                MiaIA::Studio::StudioTopologyBuilder::DetailedPosition3D(
                    layerIndex,
                    Snapshot.Layers.Num(),
                    neuronIndex,
                    layer.Neurons.Num());
            const FVector automaticPosition = WorldPosition(
                position2D,
                position3D);
            const FVector* manualPosition =
                ManualNodePositions.Find(neuron.Id);
            const FVector position = manualPosition
                ? *manualPosition
                : automaticPosition;
            positions.Add(neuron.Id, position);
            RenderedNodes.Add({neuron.Id, position});

            const FMiaIADebugNeuronTelemetry* telemetry =
                NeuronTelemetry.Find(neuron.Id);
            const bool displaysTelemetry = telemetry &&
                ((DebugPhase ==
                    EMiaIATrainingDebugPhase::BackwardComplete &&
                    telemetry->bHasGradients) ||
                    (DebugPhase ==
                        EMiaIATrainingDebugPhase::UpdateComplete &&
                        telemetry->bHasUpdate));
            const FLinearColor color = displaysTelemetry
                ? SignedNeuronColor(
                    NeuronMetric(*telemetry),
                    MaximumNeuronMetric)
                : ActivationColor(telemetry
                    ? telemetry->CandidateActivation
                    : neuron.Activation);

            ViewportClient->AddSphere(
                position,
                color,
                NeuronSphereRadius);
        }
    }

    RenderedConnections.Reserve(Snapshot.Connections.Num());

    for (const FMiaIAConnectionSnapshot& connection :
        Snapshot.Connections)
    {
        const FVector* from = positions.Find(connection.FromNeuron);
        const FVector* to = positions.Find(connection.ToNeuron);

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
            ((DebugPhase ==
                EMiaIATrainingDebugPhase::BackwardComplete &&
                telemetry->bHasGradient) ||
                (DebugPhase ==
                    EMiaIATrainingDebugPhase::UpdateComplete &&
                    telemetry->bHasUpdate));
        const float strength = FMath::Clamp(
            static_cast<float>(displaysTelemetry
                ? FMath::Abs(displayedValue) /
                    MaximumConnectionMetric
                : FMath::Abs(displayedValue)),
            0.15f,
            1.0f);
        const bool selected =
            connection.Id == SelectedConnectionId;
        const FLinearColor color = selected
            ? palette.Selection
            : displaysTelemetry
                ? SignedConnectionColor(
                    displayedValue,
                    MaximumConnectionMetric)
                : displayedValue >= 0.0
                    ? palette.PositiveWeight.CopyWithNewOpacity(strength)
                    : palette.NegativeWeight.CopyWithNewOpacity(strength);

        ViewportClient->AddLine(
            *from,
            *to,
            color,
            selected ? 4.0f : 1.0f + strength * 1.5f);
        RenderedConnections.Add({connection.Id, *from, *to});
    }
}

void SMiaIA3DNetworkView::RebuildCompactScene()
{
    const FMiaIAEditorPalette palette =
        FMiaIAEditorTheme::Palette(Theme);
    const int32 layerCount = Overview.Layers.Num();
    FVector previousPosition = FVector::ZeroVector;
    bool hasPrevious = false;

    ViewportClient->ReservePrimitives(
        layerCount,
        FMath::Max(0, layerCount - 1));

    for (int32 layerIndex = 0;
        layerIndex < layerCount;
        ++layerIndex)
    {
        const MiaIA::Studio::StudioPosition position2D =
            MiaIA::Studio::StudioTopologyBuilder::DetailedPosition2D(
                layerIndex,
                layerCount,
                0,
                1);
        const MiaIA::Studio::StudioPosition position3D =
            MiaIA::Studio::StudioTopologyBuilder::DetailedPosition3D(
                layerIndex,
                layerCount,
                0,
                1);
        const FVector position = WorldPosition(position2D, position3D);
        ViewportClient->AddSphere(
            position,
            palette.InactiveNeuron,
            CompactSphereRadius);

        if (hasPrevious)
        {
            ViewportClient->AddLine(
                previousPosition,
                position,
                palette.PositiveWeight.CopyWithNewOpacity(0.45f),
                2.0f);
        }

        previousPosition = position;
        hasPrevious = true;
    }
}

void SMiaIA3DNetworkView::UpdateCamera()
{
    if (!ViewportClient.IsValid())
    {
        return;
    }

    const FRotator rotation(CameraPitch, CameraYaw, 0.0f);
    ViewportClient->SetViewRotation(rotation);
    ViewportClient->SetViewLocation(
        LookAt - rotation.Vector() * CameraDistance);
    ViewportClient->SetLookAtLocation(LookAt);

    if (SceneViewport.IsValid())
    {
        SceneViewport->Invalidate();
    }
}

bool SMiaIA3DNetworkView::ProjectToViewport(
    const FVector& WorldPositionValue,
    FVector2D& OutPosition) const
{
    if (!SceneViewport.IsValid() || !ViewportClient.IsValid())
    {
        return false;
    }

    const FIntPoint size = SceneViewport->GetSizeXY();

    if (size.X <= 0 || size.Y <= 0)
    {
        return false;
    }

    return FSceneView::ProjectWorldToScreen(
        WorldPositionValue,
        FIntRect(0, 0, size.X, size.Y),
        ViewportClient->ViewProjectionMatrix(),
        OutPosition);
}

float SMiaIA3DNetworkView::ProjectedSphereRadius(
    const FVector& WorldPositionValue) const
{
    FVector2D center;
    FVector2D edge;
    const FVector cameraRight = FRotationMatrix(
        FRotator(CameraPitch, CameraYaw, 0.0f))
        .GetScaledAxis(EAxis::Y);

    if (!ProjectToViewport(WorldPositionValue, center) ||
        !ProjectToViewport(
            WorldPositionValue + cameraRight * NeuronSphereRadius,
            edge))
    {
        return 9.0f;
    }

    return static_cast<float>(FVector2D::Distance(center, edge));
}

bool SMiaIA3DNetworkView::ProjectedSphereBounds(
    const FVector& WorldPositionValue,
    FVector2D& OutCenter,
    FVector2D& OutRadius) const
{
    constexpr int32 latitudeSegments = 12;
    constexpr int32 longitudeSegments = 24;
    const FRotationMatrix cameraRotation(
        FRotator(CameraPitch, CameraYaw, 0.0f));
    const FVector cameraForward = cameraRotation.GetScaledAxis(EAxis::X);
    const FVector cameraRight = cameraRotation.GetScaledAxis(EAxis::Y);
    const FVector cameraUp = cameraRotation.GetScaledAxis(EAxis::Z);
    FVector2D minimum(UE_DOUBLE_BIG_NUMBER, UE_DOUBLE_BIG_NUMBER);
    FVector2D maximum(-UE_DOUBLE_BIG_NUMBER, -UE_DOUBLE_BIG_NUMBER);
    bool projectedAnyPoint = false;

    for (int32 latitude = 0;
        latitude <= latitudeSegments;
        ++latitude)
    {
        const float polarAngle = static_cast<float>(latitude) /
            latitudeSegments * UE_PI;
        const float polarSin = FMath::Sin(polarAngle);
        const float polarCos = FMath::Cos(polarAngle);

        for (int32 longitude = 0;
            longitude < longitudeSegments;
            ++longitude)
        {
            const float azimuth = static_cast<float>(longitude) /
                longitudeSegments * 2.0f * UE_PI;
            const FVector normal =
                cameraForward * polarCos +
                (cameraRight * FMath::Cos(azimuth) +
                    cameraUp * FMath::Sin(azimuth)) * polarSin;
            FVector2D projected;

            if (!ProjectToViewport(
                WorldPositionValue + normal * NeuronSphereRadius,
                projected))
            {
                continue;
            }

            minimum.X = FMath::Min(minimum.X, projected.X);
            minimum.Y = FMath::Min(minimum.Y, projected.Y);
            maximum.X = FMath::Max(maximum.X, projected.X);
            maximum.Y = FMath::Max(maximum.Y, projected.Y);
            projectedAnyPoint = true;
        }
    }

    if (!projectedAnyPoint)
    {
        return false;
    }

    OutCenter = (minimum + maximum) * 0.5;
    OutRadius = (maximum - minimum) * 0.5;
    return true;
}

bool SMiaIA3DNetworkView::DeprojectFromViewport(
    const FVector2D& ViewportPosition,
    FVector& OutOrigin,
    FVector& OutDirection) const
{
    if (!SceneViewport.IsValid() || !ViewportClient.IsValid())
    {
        return false;
    }

    const FIntPoint size = SceneViewport->GetSizeXY();

    if (size.X <= 0 || size.Y <= 0)
    {
        return false;
    }

    FSceneView::DeprojectScreenToWorld(
        ViewportPosition,
        FIntRect(0, 0, size.X, size.Y),
        ViewportClient->ViewProjectionMatrix().InverseFast(),
        OutOrigin,
        OutDirection);
    return !OutDirection.IsNearlyZero();
}

const SMiaIA3DNetworkView::FNodeRenderData*
SMiaIA3DNetworkView::FindNodeAt(
    const FVector2D& ViewportPosition) const
{
    const FNodeRenderData* closestNode = nullptr;
    double closestDistance = NodeSelectionDistance;

    for (const FNodeRenderData& node : RenderedNodes)
    {
        FVector2D projected;

        if (!ProjectToViewport(node.Position, projected))
        {
            continue;
        }

        const double distance = FVector2D::Distance(
            ViewportPosition,
            projected);

        if (distance <= closestDistance)
        {
            closestNode = &node;
            closestDistance = distance;
        }
    }

    return closestNode;
}

bool SMiaIA3DNetworkView::BeginNodeDrag(
    const FVector2D& ViewportPosition,
    const FNodeRenderData& Node)
{
    FVector rayOrigin;
    FVector rayDirection;

    if (!DeprojectFromViewport(
            ViewportPosition,
            rayOrigin,
            rayDirection))
    {
        return false;
    }

    DraggedNeuronId = Node.Id;
    DragPlaneOrigin = Node.Position;
    DragPlaneNormal = FRotator(
        CameraPitch,
        CameraYaw,
        0.0f).Vector();
    const FVector intersection = FMath::RayPlaneIntersection(
        rayOrigin,
        rayDirection,
        FPlane(DragPlaneOrigin, DragPlaneNormal));
    DragOffset = Node.Position - intersection;
    bDraggingNode = true;
    return true;
}

void SMiaIA3DNetworkView::UpdateNodeDrag(
    const FVector2D& ViewportPosition)
{
    if (!bDraggingNode || DraggedNeuronId < 0)
    {
        return;
    }

    FVector rayOrigin;
    FVector rayDirection;

    if (!DeprojectFromViewport(
            ViewportPosition,
            rayOrigin,
            rayDirection))
    {
        return;
    }

    const FVector intersection = FMath::RayPlaneIntersection(
        rayOrigin,
        rayDirection,
        FPlane(DragPlaneOrigin, DragPlaneNormal));
    ManualNodePositions.Add(
        DraggedNeuronId,
        intersection + DragOffset);
    RebuildScene();
}

void SMiaIA3DNetworkView::SelectAt(
    const FVector2D& ViewportPosition)
{
    const FNodeRenderData* closestNode = FindNodeAt(ViewportPosition);

    if (closestNode)
    {
        SelectedNeuronId = closestNode->Id;
        SelectedConnectionId = -1;
        OnNeuronSelected.ExecuteIfBound(closestNode->Id);
        RebuildScene();
        return;
    }

    const FConnectionRenderData* closestConnection = nullptr;
    double closestConnectionDistance = ConnectionSelectionDistance;

    for (const FConnectionRenderData& connection :
        RenderedConnections)
    {
        FVector2D from;
        FVector2D to;

        if (!ProjectToViewport(connection.From, from) ||
            !ProjectToViewport(connection.To, to))
        {
            continue;
        }

        const double distance = DistanceToSegment(
            ViewportPosition,
            from,
            to);

        if (distance <= closestConnectionDistance)
        {
            closestConnection = &connection;
            closestConnectionDistance = distance;
        }
    }

    if (closestConnection)
    {
        SelectedNeuronId = -1;
        SelectedConnectionId = closestConnection->Id;
        OnConnectionSelected.ExecuteIfBound(closestConnection->Id);
        RebuildScene();
    }
}

FLinearColor SMiaIA3DNetworkView::ActivationColor(
    double Activation) const
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

FLinearColor SMiaIA3DNetworkView::SignedNeuronColor(
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

FLinearColor SMiaIA3DNetworkView::SignedConnectionColor(
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

double SMiaIA3DNetworkView::NeuronMetric(
    const FMiaIADebugNeuronTelemetry& Telemetry) const
{
    if (DebugPhase ==
            EMiaIATrainingDebugPhase::BackwardComplete &&
        Telemetry.bHasGradients)
    {
        return Telemetry.LayerOrder == 0
            ? Telemetry.ActivationGradient
            : Telemetry.BiasGradient;
    }

    if (DebugPhase ==
            EMiaIATrainingDebugPhase::UpdateComplete &&
        Telemetry.bHasUpdate)
    {
        return Telemetry.Delta;
    }

    if (DebugPhase ==
            EMiaIATrainingDebugPhase::BackwardComplete ||
        DebugPhase == EMiaIATrainingDebugPhase::UpdateComplete)
    {
        return 0.0;
    }

    return Telemetry.CandidateActivation;
}

double SMiaIA3DNetworkView::ConnectionMetric(
    const FMiaIADebugConnectionTelemetry& Telemetry) const
{
    if (DebugPhase ==
            EMiaIATrainingDebugPhase::BackwardComplete &&
        Telemetry.bHasGradient)
    {
        return Telemetry.WeightGradient;
    }

    if (DebugPhase ==
            EMiaIATrainingDebugPhase::UpdateComplete &&
        Telemetry.bHasUpdate)
    {
        return Telemetry.Delta;
    }

    if (DebugPhase ==
            EMiaIATrainingDebugPhase::BackwardComplete ||
        DebugPhase == EMiaIATrainingDebugPhase::UpdateComplete)
    {
        return 0.0;
    }

    return Telemetry.CandidateWeight;
}

double SMiaIA3DNetworkView::DistanceToSegment(
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

    const double progress = FMath::Clamp(
        FVector2D::DotProduct(Point - Start, segment) /
            lengthSquared,
        0.0,
        1.0);
    return FVector2D::Distance(
        Point,
        Start + segment * progress);
}
