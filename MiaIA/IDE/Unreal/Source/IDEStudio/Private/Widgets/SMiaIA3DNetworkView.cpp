#include "Widgets/SMiaIA3DNetworkView.h"

#include "StudioTopology.h"
#include "Components/Viewport.h"
#include "DynamicMeshBuilder.h"
#include "Engine/Engine.h"
#include "Engine/EngineTypes.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
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
    constexpr float BaseNeuronSphereRadius = 24.0f;
    constexpr float CompactSphereRadius = 34.0f;
    constexpr float CompactSphereGap = 12.0f;
    constexpr float NetworkAggregateSphereRadius = 52.0f;
    constexpr float NetworkIndicatorLength = 120.0f;
    constexpr float NetworkIndicatorGap = 12.0f;
    constexpr int32 MaximumNetworkIndicators = 512;
    constexpr float NodeSelectionDistance = 17.0f;
    constexpr float ConnectionSelectionDistance = 7.0f;
    constexpr float MinimumCameraDistance = 80.0f;
    constexpr float MaximumCameraDistance = 12000.0f;
    constexpr float OrbitSpeed = 0.28f;
    constexpr float PanSpeed = 0.0015f;
    constexpr float ZoomStep = 1.15f;

    FVector DetailedWorldPosition(
        const MiaIA::Studio::StudioPosition& LayoutPosition,
        float LayoutUnit)
    {
        return FVector(
            LayoutPosition.X * LayoutUnit,
            0.0,
            -LayoutPosition.Y * LayoutUnit);
    }

    FVector CompactLayerWorldPosition(
        int32 LayerIndex,
        int32 LayerCount,
        MiaIA::Studio::StudioLayoutOrientation Orientation,
        MiaIA::Studio::StudioLayoutDirection Direction)
    {
        const int32 directedIndex = Direction ==
            MiaIA::Studio::StudioLayoutDirection::Reverse
            ? LayerCount - LayerIndex - 1
            : LayerIndex;
        const float spacing = CompactSphereRadius * 2.0f +
            CompactSphereGap;
        const float offset = LayerCount <= 1
            ? 0.0f
            : (static_cast<float>(directedIndex) -
                static_cast<float>(LayerCount - 1) * 0.5f) * spacing;

        return Orientation ==
            MiaIA::Studio::StudioLayoutOrientation::Vertical
            ? FVector(0.0, 0.0, -offset)
            : FVector(offset, 0.0, 0.0);
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
    OnNeuronSelectionChanged = InArgs._OnNeuronSelectionChanged;
    OnConnectionSelected = InArgs._OnConnectionSelected;
    OnLayerSelected = InArgs._OnLayerSelected;
    OnNeuronNavigationRequested =
        InArgs._OnNeuronNavigationRequested;
    OnLayerOpenRequested = InArgs._OnLayerOpenRequested;
    OnNetworkOpenRequested = InArgs._OnNetworkOpenRequested;
    OnLayerFocusExitRequested = InArgs._OnLayerFocusExitRequested;

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
    bSceneDirty = true;

    if (modeChanged ||
        (bNetworkAggregateMode && aggregateDimensionsChanged))
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

void SMiaIA3DNetworkView::SetForwardTraceOverlay(
    const TMap<int64, double>& InActivations,
    const TMap<int64, double>& InContributions,
    const TSet<int64>& InPlaybackConnections,
    bool bInPlaybackActive)
{
    ForwardTraceActivations = InActivations;
    ForwardTraceContributions = InContributions;
    ForwardTracePlaybackConnections = InPlaybackConnections;
    bForwardTracePlaybackActive = bInPlaybackActive;
    MaximumForwardTraceContribution = UE_DOUBLE_SMALL_NUMBER;

    for (const TPair<int64, double>& contribution :
        ForwardTraceContributions)
    {
        MaximumForwardTraceContribution = FMath::Max(
            MaximumForwardTraceContribution,
            FMath::Abs(contribution.Value));
    }

    bSceneDirty = true;
}

void SMiaIA3DNetworkView::SetSelectedNeurons(
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

    bSceneDirty = true;
}

void SMiaIA3DNetworkView::SetSelectedConnection(int64 InConnectionId)
{
    SelectedConnectionId = InConnectionId;

    if (SelectedConnectionId >= 0)
    {
        SelectedNeuronIds.Reset();
        SelectedNeuronId = -1;
        SelectedLayerId = -1;
    }

    bSceneDirty = true;
}

void SMiaIA3DNetworkView::SetSelectedLayer(int64 InLayerId)
{
    SelectedLayerId = InLayerId;

    if (SelectedLayerId >= 0)
    {
        SelectedNeuronIds.Reset();
        SelectedNeuronId = -1;
        SelectedConnectionId = -1;
    }

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

void SMiaIA3DNetworkView::SetVisualizationSettings(
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
        ManualNodePositions.Reset();
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
    float primitiveRadius = SphereRadius();

    if (bCompactMode)
    {
        if (bNetworkAggregateMode)
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
            primitiveRadius = NetworkAggregateSphereRadius;
            sceneBounds += FVector(
                -NetworkAggregateSphereRadius - NetworkIndicatorLength,
                0.0,
                -indicatorSpan * 0.5f);
            sceneBounds += FVector(
                NetworkAggregateSphereRadius + NetworkIndicatorLength,
                0.0,
                indicatorSpan * 0.5f);
        }
        else
        {
            primitiveRadius = CompactSphereRadius;

            for (int32 layerIndex = 0;
                layerIndex < Overview.Layers.Num();
                ++layerIndex)
            {
                sceneBounds += CompactLayerWorldPosition(
                    layerIndex,
                    Overview.Layers.Num(),
                    VisualizationSettings.Layout.Orientation,
                    VisualizationSettings.Layout.Direction);
            }
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

void SMiaIA3DNetworkView::RevealNeuron(int64 NeuronId)
{
    if (bSceneDirty)
    {
        RebuildScene();
    }

    const FNodeRenderData* node = RenderedNodes.FindByPredicate(
        [NeuronId](const FNodeRenderData& candidate)
        {
            return candidate.Id == NeuronId;
        });

    if (!node || !SceneViewport.IsValid())
    {
        return;
    }

    FVector2D projectedPosition;
    const FIntPoint viewportSize = SceneViewport->GetSizeXY();
    constexpr float revealMargin = 64.0f;
    const bool isVisible = ProjectToViewport(
        node->Position,
        projectedPosition) &&
        projectedPosition.X >= revealMargin &&
        projectedPosition.Y >= revealMargin &&
        projectedPosition.X <= viewportSize.X - revealMargin &&
        projectedPosition.Y <= viewportSize.Y - revealMargin;

    if (!isVisible)
    {
        LookAt = node->Position;
        UpdateCamera();
    }
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

    const FVector2D localSize = AllottedGeometry.GetLocalSize();
    const FIntPoint desiredSize(
        FMath::Max(1, FMath::RoundToInt(localSize.X)),
        FMath::Max(1, FMath::RoundToInt(localSize.Y)));

    if (SceneViewport->GetSizeXY() != desiredSize)
    {
        SceneViewport->ResizeFrame(
            desiredSize.X,
            desiredSize.Y,
            EWindowMode::Windowed);
    }

    if (bSceneDirty)
    {
        RebuildScene();
    }

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

    if (RenderedNodes.IsEmpty() ||
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
    const bool showLabels = VisualizationSettings.bShowNeuronLabels &&
        !bCompactMode &&
        RenderedNodes.Num() <= 500 &&
        VisualizationSettings.Layout.Mode !=
            MiaIA::Studio::StudioLayoutMode::Packed;
    const TSharedRef<FSlateFontMeasure> fontMeasure =
        FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
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
        const float compactRadius = bNetworkAggregateMode
            ? NetworkAggregateSphereRadius
            : CompactSphereRadius;
        const float projectedRadius = ProjectedSphereRadius(
            node.Position,
            bCompactMode ? compactRadius : SphereRadius()) *
            static_cast<float>((viewportToLocal.X + viewportToLocal.Y) * 0.5);

        const bool selected = bCompactMode && !bNetworkAggregateMode
            ? node.Id == SelectedLayerId
            : SelectedNeuronIds.Contains(node.Id);

        if (selected)
        {
            const FVector2D selectionCenter = localPosition;
            const float selectionRadius = FMath::Max(
                12.0f,
                projectedRadius + 4.0f);
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
                        FMath::Cos(angle) * selectionRadius,
                        FMath::Sin(angle) * selectionRadius));
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

        if (showLabels && projectedRadius * 2.0f >= 24.0f)
        {
            const FText label = FText::Format(
                NSLOCTEXT("MiaIAStudio", "NeuronId3D", "#{0}"),
                FText::AsNumber(node.Id));
            FSlateFontInfo labelFont =
                FAppStyle::GetFontStyle(TEXT("SmallFont"));
            labelFont.Size = FMath::Clamp(
                FMath::RoundToInt(projectedRadius * 2.0f * 0.10f),
                6,
                12);
            const FVector2D textSize =
                fontMeasure->Measure(label, labelFont);
            const FVector2D labelSize =
                textSize + FVector2D(4.0f, 2.0f);
            const FVector2D labelPosition = localPosition + FVector2D(
                -labelSize.X * 0.5f,
                FMath::Max(10.0f, projectedRadius) + 5.0f);

            FSlateDrawElement::MakeBox(
                OutDrawElements,
                overlayLayer + 1,
                AllottedGeometry.ToPaintGeometry(
                    labelSize,
                    FSlateLayoutTransform(labelPosition)),
                &NeuronLabelBrush,
                ESlateDrawEffect::None,
                selected
                    ? palette.Selection.CopyWithNewOpacity(0.90f)
                    : palette.SubduedText.CopyWithNewOpacity(0.35f));
            FSlateDrawElement::MakeBox(
                OutDrawElements,
                overlayLayer + 2,
                AllottedGeometry.ToPaintGeometry(
                    labelSize - FVector2D(2.0f, 2.0f),
                    FSlateLayoutTransform(
                        labelPosition + FVector2D(1.0f, 1.0f))),
                &NeuronLabelBrush,
                ESlateDrawEffect::None,
                palette.Surface.CopyWithNewOpacity(0.96f));
            FSlateDrawElement::MakeText(
                OutDrawElements,
                overlayLayer + 3,
                AllottedGeometry.ToPaintGeometry(
                    textSize,
                    FSlateLayoutTransform(
                        labelPosition +
                            (labelSize - textSize) * 0.5f)),
                label,
                labelFont,
                ESlateDrawEffect::None,
                selected ? palette.Selection : palette.SubduedText);
        }
    }

    int32 finalLayer = showLabels ? overlayLayer + 3 : overlayLayer;

    if (bMarqueeSelecting)
    {
        const FVector2D minimum(
            FMath::Min(MarqueeStart.X, MarqueeEnd.X),
            FMath::Min(MarqueeStart.Y, MarqueeEnd.Y));
        const FVector2D maximum(
            FMath::Max(MarqueeStart.X, MarqueeEnd.X),
            FMath::Max(MarqueeStart.Y, MarqueeEnd.Y));
        const FVector2D size = maximum - minimum;
        const int32 marqueeLayer = finalLayer + 1;
        FSlateDrawElement::MakeBox(
            OutDrawElements,
            marqueeLayer,
            AllottedGeometry.ToPaintGeometry(
                size,
                FSlateLayoutTransform(minimum)),
            FAppStyle::GetBrush(TEXT("WhiteBrush")),
            ESlateDrawEffect::None,
            palette.Selection.CopyWithNewOpacity(0.10f));
        TArray<FVector2D> borderPoints{
            minimum,
            FVector2D(maximum.X, minimum.Y),
            maximum,
            FVector2D(minimum.X, maximum.Y),
            minimum};
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

FReply SMiaIA3DNetworkView::OnMouseButtonDown(
    const FGeometry& MyGeometry,
    const FPointerEvent& MouseEvent)
{
    FSlateApplication::Get().SetKeyboardFocus(
        SharedThis(this),
        EFocusCause::Mouse);
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
        if (bCompactMode)
        {
            const FVector2D localSize = MyGeometry.GetLocalSize();
            const FIntPoint viewportSize = SceneViewport->GetSizeXY();

            if (localSize.X > 0.0f && localSize.Y > 0.0f &&
                viewportSize.X > 0 && viewportSize.Y > 0)
            {
                const FVector2D viewportPosition(
                    localPosition.X * viewportSize.X / localSize.X,
                    localPosition.Y * viewportSize.Y / localSize.Y);

                if (const FNodeRenderData* node =
                    FindNodeAt(viewportPosition))
                {
                    if (bNetworkAggregateMode)
                    {
                        OnNetworkOpenRequested.ExecuteIfBound();
                    }
                    else
                    {
                        OnLayerSelected.ExecuteIfBound(node->Id);
                    }
                }
            }

            return FReply::Handled();
        }

        const FVector2D localSize = MyGeometry.GetLocalSize();
        const FIntPoint viewportSize = SceneViewport->GetSizeXY();

        if (localSize.X > 0.0f && localSize.Y > 0.0f)
        {
            const FVector2D viewportPosition(
                localPosition.X * viewportSize.X / localSize.X,
                localPosition.Y * viewportSize.Y / localSize.Y);
            const FNodeRenderData* node = FindNodeAt(viewportPosition);

            if (node)
            {
                const FNodeRenderData nodeValue = *node;
                const bool additive = MouseEvent.IsControlDown();

                if (additive)
                {
                    if (SelectedNeuronIds.Contains(nodeValue.Id))
                    {
                        SelectedNeuronIds.Remove(nodeValue.Id);

                        if (SelectedNeuronId == nodeValue.Id)
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
                        SelectedNeuronIds.Add(nodeValue.Id);
                        SelectedNeuronId = nodeValue.Id;
                    }
                }
                else
                {
                    if (!SelectedNeuronIds.Contains(nodeValue.Id))
                    {
                        SelectedNeuronIds.Reset();
                        SelectedNeuronIds.Add(nodeValue.Id);
                    }

                    SelectedNeuronId = nodeValue.Id;
                }

                SelectedConnectionId = -1;
                NotifyNeuronSelectionChanged();
                RebuildScene();

                if (additive || !SelectedNeuronIds.Contains(nodeValue.Id))
                {
                    return FReply::Handled();
                }

                const bool startedDrag = BeginNodeDrag(
                    viewportPosition,
                    nodeValue);
                FReply reply = FReply::Handled();

                if (startedDrag)
                {
                    reply.CaptureMouse(SharedThis(this));
                }

                return reply;
            }

            if (const FConnectionRenderData* connection =
                FindConnectionAt(viewportPosition))
            {
                SelectedNeuronIds.Reset();
                SelectedNeuronId = -1;
                SelectedConnectionId = connection->Id;
                OnConnectionSelected.ExecuteIfBound(connection->Id);
                RebuildScene();
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

        return FReply::Handled();
    }

    return SViewport::OnMouseButtonDown(MyGeometry, MouseEvent);
}

FReply SMiaIA3DNetworkView::OnMouseButtonDoubleClick(
    const FGeometry& MyGeometry,
    const FPointerEvent& MouseEvent)
{
    if (!bCompactMode ||
        MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton ||
        !SceneViewport.IsValid())
    {
        return FReply::Unhandled();
    }

    const FVector2D localPosition = MyGeometry.AbsoluteToLocal(
        MouseEvent.GetScreenSpacePosition());
    const FVector2D localSize = MyGeometry.GetLocalSize();
    const FIntPoint viewportSize = SceneViewport->GetSizeXY();

    if (localSize.X <= 0.0f || localSize.Y <= 0.0f ||
        viewportSize.X <= 0 || viewportSize.Y <= 0)
    {
        return FReply::Unhandled();
    }

    const FVector2D viewportPosition(
        localPosition.X * viewportSize.X / localSize.X,
        localPosition.Y * viewportSize.Y / localSize.Y);

    if (const FNodeRenderData* node = FindNodeAt(viewportPosition))
    {
        if (bNetworkAggregateMode)
        {
            OnNetworkOpenRequested.ExecuteIfBound();
        }
        else
        {
            OnLayerOpenRequested.ExecuteIfBound(node->Id);
        }
        return FReply::Handled();
    }

    return FReply::Unhandled();
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
    const bool finishesMarquee =
        button == EKeys::LeftMouseButton && bMarqueeSelecting;

    if (!finishesOrbit && !finishesPan &&
        !finishesNodeDrag && !finishesMarquee)
    {
        return SViewport::OnMouseButtonUp(MyGeometry, MouseEvent);
    }

    if (finishesMarquee)
    {
        CompleteMarqueeSelection(MyGeometry);
    }

    bOrbiting = false;
    bPanning = false;
    bDraggingNode = false;
    bMarqueeSelecting = false;
    DraggedNeuronId = -1;
    DragStartPositions.Reset();
    return FReply::Handled().ReleaseMouseCapture();
}

FReply SMiaIA3DNetworkView::OnMouseMove(
    const FGeometry& MyGeometry,
    const FPointerEvent& MouseEvent)
{
    if (!HasMouseCapture() ||
        (!bOrbiting && !bPanning &&
            !bDraggingNode && !bMarqueeSelecting))
    {
        return SViewport::OnMouseMove(MyGeometry, MouseEvent);
    }

    const FVector2D localPosition = MyGeometry.AbsoluteToLocal(
        MouseEvent.GetScreenSpacePosition());
    const FVector2D delta = localPosition - LastPointerPosition;
    LastPointerPosition = localPosition;

    if (bMarqueeSelecting)
    {
        MarqueeEnd = localPosition;
        Invalidate(EInvalidateWidgetReason::Paint);
        return FReply::Handled();
    }

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

FReply SMiaIA3DNetworkView::OnKeyDown(
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

    return SViewport::OnKeyDown(MyGeometry, KeyEvent);
}

void SMiaIA3DNetworkView::OnMouseCaptureLost(
    const FCaptureLostEvent& CaptureLostEvent)
{
    bOrbiting = false;
    bPanning = false;
    bDraggingNode = false;
    bMarqueeSelecting = false;
    DraggedNeuronId = -1;
    DragStartPositions.Reset();
    Invalidate(EInvalidateWidgetReason::Paint);
    SViewport::OnMouseCaptureLost(CaptureLostEvent);
}

float SMiaIA3DNetworkView::SphereRadius() const
{
    return BaseNeuronSphereRadius * FMath::Clamp(
        VisualizationSettings.NeuronScale,
        0.25f,
        3.0f);
}

float SMiaIA3DNetworkView::LayoutUnitSize() const
{
    return BaseNeuronSphereRadius * 2.0f;
}

bool SMiaIA3DNetworkView::ShouldDrawConnection(
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

bool SMiaIA3DNetworkView::CanApplyDragTranslation(
    const FVector& Translation) const
{
    const float minimumDistance = SphereRadius() * 2.0f;

    for (const TPair<int64, FVector>& dragged : DragStartPositions)
    {
        const FVector proposed = dragged.Value + Translation;

        for (const FNodeRenderData& other : RenderedNodes)
        {
            if (DragStartPositions.Contains(other.Id))
            {
                continue;
            }

            if (FVector::Distance(proposed, other.Position) +
                UE_KINDA_SMALL_NUMBER < minimumDistance)
            {
                return false;
            }
        }
    }

    return true;
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
        if (bNetworkAggregateMode)
        {
            RebuildNetworkAggregateScene();
        }
        else
        {
            RebuildCompactScene();
        }
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
            const MiaIA::Studio::StudioPosition layoutPosition =
                MiaIA::Studio::StudioTopologyBuilder::DetailedLayoutPosition(
                    layerIndex,
                    Snapshot.Layers.Num(),
                    neuronIndex,
                    layer.Neurons.Num(),
                    VisualizationSettings.Layout);
            const FVector automaticPosition = DetailedWorldPosition(
                layoutPosition,
                LayoutUnitSize());
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
            const double* traceActivation =
                ForwardTraceActivations.Find(neuron.Id);
            const FLinearColor color = displaysTelemetry
                ? SignedNeuronColor(
                    NeuronMetric(*telemetry),
                    MaximumNeuronMetric)
                : bForwardTracePlaybackActive && !traceActivation
                    ? palette.InactiveNeuron.CopyWithNewOpacity(0.28f)
                : ActivationColor(traceActivation
                    ? *traceActivation
                    : telemetry
                        ? telemetry->CandidateActivation
                        : neuron.Activation);

            ViewportClient->AddSphere(
                position,
                color,
                SphereRadius());
        }
    }

    RenderedConnections.Reserve(Snapshot.Connections.Num());

    for (const FMiaIAConnectionSnapshot& connection :
        Snapshot.Connections)
    {
        if (!ShouldDrawConnection(connection))
        {
            continue;
        }

        const FVector* from = positions.Find(connection.FromNeuron);
        const FVector* to = positions.Find(connection.ToNeuron);

        if (!from || !to)
        {
            continue;
        }

        const FMiaIADebugConnectionTelemetry* telemetry =
            ConnectionTelemetry.Find(connection.Id);
        const double* traceContribution =
            ForwardTraceContributions.Find(connection.Id);
        const bool playbackConnection =
            ForwardTracePlaybackConnections.Contains(connection.Id);
        const double displayedValue = traceContribution
            ? *traceContribution
            : telemetry
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
            static_cast<float>(playbackConnection
                ? 1.0
                : traceContribution
                ? FMath::Abs(displayedValue) /
                    MaximumForwardTraceContribution
                : displaysTelemetry
                ? FMath::Abs(displayedValue) /
                    MaximumConnectionMetric
                : FMath::Abs(displayedValue)),
            0.15f,
            1.0f);
        const bool selected =
            connection.Id == SelectedConnectionId;
        const FLinearColor color = selected
            ? palette.Selection
            : traceContribution
                ? SignedConnectionColor(
                    displayedValue,
                    MaximumForwardTraceContribution)
                : playbackConnection
                    ? palette.Debug
                : bForwardTracePlaybackActive ||
                    !ForwardTraceActivations.IsEmpty()
                    ? palette.SubduedText.CopyWithNewOpacity(0.12f)
                : displaysTelemetry
                ? SignedConnectionColor(
                    displayedValue,
                    MaximumConnectionMetric)
                : displayedValue >= 0.0
                    ? palette.PositiveWeight.CopyWithNewOpacity(strength)
                    : palette.NegativeWeight.CopyWithNewOpacity(strength);
        const float connectionScale = FMath::Clamp(
            VisualizationSettings.ConnectionScale,
            0.0f,
            2.0f);
        const FLinearColor displayedColor = color.CopyWithNewOpacity(
            FMath::Clamp(
                color.A * connectionScale,
                0.0f,
                1.0f));

        ViewportClient->AddLine(
            *from,
            *to,
            displayedColor,
            (selected ? 4.0f : 1.0f + strength * 1.5f) *
                connectionScale);
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
        const FVector position = CompactLayerWorldPosition(
            layerIndex,
            layerCount,
            VisualizationSettings.Layout.Orientation,
            VisualizationSettings.Layout.Direction);
        ViewportClient->AddSphere(
            position,
            palette.ActiveNeuron,
            CompactSphereRadius);
        RenderedNodes.Add({Overview.Layers[layerIndex].Id, position});

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

void SMiaIA3DNetworkView::RebuildNetworkAggregateScene()
{
    const FMiaIAEditorPalette palette =
        FMiaIAEditorTheme::Palette(Theme);
    const int64 inputCount = Overview.Layers.IsEmpty()
        ? 0
        : Overview.Layers[0].NeuronCount;
    const int64 outputCount = Overview.Layers.IsEmpty()
        ? 0
        : Overview.Layers.Last().NeuronCount;
    const FVector center = FVector::ZeroVector;

    const auto visibleCount = [](int64 count)
    {
        return static_cast<int32>(FMath::Min<int64>(
            FMath::Max<int64>(0, count),
            MaximumNetworkIndicators));
    };
    const int32 visibleInputs = visibleCount(inputCount);
    const int32 visibleOutputs = visibleCount(outputCount);

    ViewportClient->ReservePrimitives(
        1,
        visibleInputs + visibleOutputs);
    ViewportClient->AddSphere(
        center,
        palette.Debug,
        NetworkAggregateSphereRadius);
    RenderedNodes.Add({-1, center});

    const auto addIndicators = [this](
        int32 count,
        const FVector& start,
        const FVector& direction,
        const FVector& spreadAxis,
        const FLinearColor& color)
    {
        for (int32 index = 0; index < count; ++index)
        {
            const float offset = (static_cast<float>(index) -
                static_cast<float>(count - 1) * 0.5f) *
                NetworkIndicatorGap;
            const FVector lineStart = start + spreadAxis * offset;
            ViewportClient->AddLine(
                lineStart,
                lineStart + direction * NetworkIndicatorLength,
                color,
                3.0f);
        }
    };

    addIndicators(
        visibleInputs,
        center + FVector(-NetworkAggregateSphereRadius, 0.0, 0.0),
        FVector(-1.0, 0.0, 0.0),
        FVector(0.0, 0.0, 1.0),
        palette.Debug);
    addIndicators(
        visibleOutputs,
        center + FVector(NetworkAggregateSphereRadius, 0.0, 0.0),
        FVector(1.0, 0.0, 0.0),
        FVector(0.0, 0.0, 1.0),
        palette.ActiveNeuron);
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
    const FVector& WorldPositionValue,
    float WorldRadius) const
{
    FVector2D center;
    FVector2D edge;
    const FVector cameraRight = FRotationMatrix(
        FRotator(CameraPitch, CameraYaw, 0.0f))
        .GetScaledAxis(EAxis::Y);

    if (!ProjectToViewport(WorldPositionValue, center) ||
        !ProjectToViewport(
            WorldPositionValue + cameraRight * WorldRadius,
            edge))
    {
        return 9.0f;
    }

    return static_cast<float>(FVector2D::Distance(center, edge));
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
    double closestDistance = UE_DOUBLE_BIG_NUMBER;

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

        const double selectionDistance = FMath::Max(
            static_cast<double>(NodeSelectionDistance),
            static_cast<double>(ProjectedSphereRadius(
                node.Position,
                bNetworkAggregateMode
                    ? NetworkAggregateSphereRadius
                    : bCompactMode
                        ? CompactSphereRadius
                        : SphereRadius())));

        if (distance <= selectionDistance &&
            distance <= closestDistance)
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
    DragStartIntersection = intersection;
    DragStartPositions.Reset();

    for (const FNodeRenderData& renderedNode : RenderedNodes)
    {
        if (SelectedNeuronIds.Contains(renderedNode.Id))
        {
            DragStartPositions.Add(
                renderedNode.Id,
                renderedNode.Position);
        }
    }

    if (DragStartPositions.IsEmpty())
    {
        DragStartPositions.Add(Node.Id, Node.Position);
    }

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
    const FVector translation = intersection - DragStartIntersection;

    if (!CanApplyDragTranslation(translation))
    {
        return;
    }

    for (const TPair<int64, FVector>& entry : DragStartPositions)
    {
        ManualNodePositions.Add(
            entry.Key,
            entry.Value + translation);
    }

    RebuildScene();
}

const SMiaIA3DNetworkView::FConnectionRenderData*
SMiaIA3DNetworkView::FindConnectionAt(
    const FVector2D& ViewportPosition)
    const
{
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

    return closestConnection;
}

void SMiaIA3DNetworkView::CompleteMarqueeSelection(
    const FGeometry& Geometry)
{
    if (!SceneViewport.IsValid())
    {
        return;
    }

    const FVector2D localSize = Geometry.GetLocalSize();
    const FIntPoint viewportSize = SceneViewport->GetSizeXY();

    if (localSize.X <= 0.0f || localSize.Y <= 0.0f ||
        viewportSize.X <= 0 || viewportSize.Y <= 0)
    {
        return;
    }

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

    for (const FNodeRenderData& node : RenderedNodes)
    {
        FVector2D projected;

        if (!ProjectToViewport(node.Position, projected))
        {
            continue;
        }

        const FVector2D localPosition(
            projected.X * localSize.X / viewportSize.X,
            projected.Y * localSize.Y / viewportSize.Y);

        if (localPosition.X >= minimum.X &&
            localPosition.X <= maximum.X &&
            localPosition.Y >= minimum.Y &&
            localPosition.Y <= maximum.Y)
        {
            selection.Add(node.Id);
            primaryNeuronId = node.Id;
        }
    }

    SelectedNeuronIds = MoveTemp(selection);
    SelectedNeuronId = SelectedNeuronIds.Contains(primaryNeuronId)
        ? primaryNeuronId
        : -1;
    NotifyNeuronSelectionChanged();
    RebuildScene();
}

void SMiaIA3DNetworkView::NotifyNeuronSelectionChanged()
{
    OnNeuronSelectionChanged.ExecuteIfBound(
        SelectedNeuronIds,
        SelectedNeuronId);
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
