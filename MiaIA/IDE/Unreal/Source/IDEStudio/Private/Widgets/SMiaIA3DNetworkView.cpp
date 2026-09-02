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
    constexpr float SelectionCursorGap = 2.0f;
    constexpr float SelectionCursorThickness = 3.0f;
    constexpr float LayerDragActivationDistance = 4.0f;

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

    struct FMiaIAViewportDisc
    {
        FVector Position{FVector::ZeroVector};
        FVector Normal{FVector::ForwardVector};
        FLinearColor Color{FLinearColor::White};
        float Radius{};
        float HalfThickness{};
        int32 Sides{16};
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
        Discs.Reset();
    }

    void ReservePrimitives(
        int32 PointCount,
        int32 LineCount,
        int32 DiscCount = 0)
    {
        Spheres.Reserve(PointCount);
        Lines.Reserve(LineCount);
        Discs.Reserve(DiscCount);
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

    void AddDisc(
        const FVector& Position,
        const FVector& Normal,
        const FLinearColor& Color,
        float Radius,
        float HalfThickness,
        int32 Sides)
    {
        Discs.Add({
            Position,
            Normal.GetSafeNormal(),
            Color.CopyWithNewOpacity(1.0f),
            Radius,
            HalfThickness,
            FMath::Max(Sides, 6)
        });
    }

    using FUMGViewportClient::Draw;

    virtual void Draw(
        const FSceneView*,
        FPrimitiveDrawInterface* PDI) override
    {
        UMaterial* vertexColorMaterial = ResolveVertexColorMaterial();

        if ((Spheres.IsEmpty() && Lines.IsEmpty() && Discs.IsEmpty()) ||
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
        int32 discVertexCount{};
        int32 discTriangleCount{};

        for (const FMiaIAViewportDisc& disc : Discs)
        {
            discVertexCount += disc.Sides * 4 + 6;
            discTriangleCount += disc.Sides * 4;
        }

        FDynamicMeshBuilder meshBuilder(PDI->View->GetFeatureLevel());
        meshBuilder.ReserveVertices(
            Spheres.Num() *
                (sphereSides + 1) * (sphereRings + 1) +
            Lines.Num() * (cylinderSides + 1) * 2 +
            discVertexCount);
        meshBuilder.ReserveTriangles(
            Spheres.Num() * sphereSides * sphereRings * 2 +
            Lines.Num() * cylinderSides * 2 +
            discTriangleCount);

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

        for (const FMiaIAViewportDisc& disc : Discs)
        {
            AddDiscGeometry(meshBuilder, disc);
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

    static void AddDiscGeometry(
        FDynamicMeshBuilder& MeshBuilder,
        const FMiaIAViewportDisc& Disc)
    {
        const FVector normal = Disc.Normal.GetSafeNormal();

        if (normal.IsNearlyZero() || Disc.Radius <= 0.0f)
        {
            return;
        }

        const FVector reference = FMath::Abs(normal.Z) < 0.9
            ? FVector::UpVector
            : FVector::ForwardVector;
        const FVector axisX = FVector::CrossProduct(
            reference,
            normal).GetSafeNormal();
        const FVector axisY = FVector::CrossProduct(
            normal,
            axisX).GetSafeNormal();
        const FVector frontCenter = Disc.Position +
            normal * Disc.HalfThickness;
        const FVector backCenter = Disc.Position -
            normal * Disc.HalfThickness;
        int32 firstSideVertex = INDEX_NONE;

        for (int32 endpoint = 0; endpoint < 2; ++endpoint)
        {
            const FVector center = endpoint == 0
                ? frontCenter
                : backCenter;

            for (int32 side = 0; side <= Disc.Sides; ++side)
            {
                const float angle = static_cast<float>(side) /
                    Disc.Sides * 2.0f * UE_PI;
                const FVector radial =
                    axisX * FMath::Cos(angle) +
                    axisY * FMath::Sin(angle);
                const int32 vertexIndex = MeshBuilder.AddVertex(
                    FVector3f(center + radial * Disc.Radius),
                    FVector2f(
                        static_cast<float>(side) / Disc.Sides,
                        static_cast<float>(endpoint)),
                    FVector3f(normal),
                    FVector3f(FVector::CrossProduct(
                        radial,
                        normal).GetSafeNormal()),
                    FVector3f(radial),
                    ShadedColor(Disc.Color, radial));

                if (firstSideVertex == INDEX_NONE)
                {
                    firstSideVertex = vertexIndex;
                }
            }
        }

        const int32 secondSideVertex = firstSideVertex + Disc.Sides + 1;

        for (int32 side = 0; side < Disc.Sides; ++side)
        {
            const int32 front = firstSideVertex + side;
            const int32 back = secondSideVertex + side;
            MeshBuilder.AddTriangle(front, front + 1, back);
            MeshBuilder.AddTriangle(front + 1, back + 1, back);
        }

        const int32 frontCapCenter = MeshBuilder.AddVertex(
            FVector3f(frontCenter),
            FVector2f(0.5f, 0.5f),
            FVector3f(axisX),
            FVector3f(axisY),
            FVector3f(normal),
            ShadedColor(Disc.Color, normal));
        const int32 firstFrontCapVertex = frontCapCenter + 1;

        for (int32 side = 0; side <= Disc.Sides; ++side)
        {
            const float angle = static_cast<float>(side) /
                Disc.Sides * 2.0f * UE_PI;
            const FVector radial =
                axisX * FMath::Cos(angle) +
                axisY * FMath::Sin(angle);
            MeshBuilder.AddVertex(
                FVector3f(frontCenter + radial * Disc.Radius),
                FVector2f(
                    0.5f + FMath::Cos(angle) * 0.5f,
                    0.5f + FMath::Sin(angle) * 0.5f),
                FVector3f(axisX),
                FVector3f(axisY),
                FVector3f(normal),
                ShadedColor(Disc.Color, normal));
        }

        const int32 backCapCenter = MeshBuilder.AddVertex(
            FVector3f(backCenter),
            FVector2f(0.5f, 0.5f),
            FVector3f(axisX),
            FVector3f(-axisY),
            FVector3f(-normal),
            ShadedColor(Disc.Color, -normal));
        const int32 firstBackCapVertex = backCapCenter + 1;

        for (int32 side = 0; side <= Disc.Sides; ++side)
        {
            const float angle = static_cast<float>(side) /
                Disc.Sides * 2.0f * UE_PI;
            const FVector radial =
                axisX * FMath::Cos(angle) +
                axisY * FMath::Sin(angle);
            MeshBuilder.AddVertex(
                FVector3f(backCenter + radial * Disc.Radius),
                FVector2f(
                    0.5f + FMath::Cos(angle) * 0.5f,
                    0.5f + FMath::Sin(angle) * 0.5f),
                FVector3f(axisX),
                FVector3f(-axisY),
                FVector3f(-normal),
                ShadedColor(Disc.Color, -normal));
        }

        for (int32 side = 0; side < Disc.Sides; ++side)
        {
            MeshBuilder.AddTriangle(
                frontCapCenter,
                firstFrontCapVertex + side,
                firstFrontCapVertex + side + 1);
            MeshBuilder.AddTriangle(
                backCapCenter,
                firstBackCapVertex + side + 1,
                firstBackCapVertex + side);
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
    TArray<FMiaIAViewportDisc> Discs;
    TStrongObjectPtr<UMaterial> RuntimeVertexColorMaterial;
    bool bAttemptedRuntimeMaterialLoad{};
};

namespace
{
    constexpr float BaseNeuronSphereRadius = 24.0f;
    constexpr float CompactSphereRadius = 34.0f;
    constexpr float CompactLayerIndicatorLength = 80.0f;
    constexpr float CompactLayerIndicatorGap = 8.0f;
    constexpr float CompactLayerBlockGap = 18.0f;
    constexpr float NetworkAggregateSphereRadius = 52.0f;
    constexpr float NetworkIndicatorLength = 120.0f;
    constexpr float NetworkIndicatorGap = 12.0f;
    constexpr int32 MaximumNetworkIndicators = 512;
    constexpr float NodeSelectionDistance = 17.0f;
    constexpr float ConnectionSelectionDistance = 7.0f;
    constexpr float MinimumCameraDistance = 80.0f;
    constexpr float MaximumCameraDistance = 100000.0f;
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

    FVector CoaxialRingWorldPosition(
        int32 LayerIndex,
        int32 LayerCount,
        int32 NeuronIndex,
        int32 NeuronCount,
        const FMiaIAVisualizationSettings& Settings)
    {
        const float neuronRadius = BaseNeuronSphereRadius * FMath::Clamp(
            Settings.NeuronScale,
            0.25f,
            3.0f);
        const float neuronDiameter = neuronRadius * 2.0f;
        const float neuronGap = FMath::Max(
            0.0f,
            static_cast<float>(Settings.Layout.NeuronGap));
        const float layerGap = FMath::Max(
            0.0f,
            static_cast<float>(Settings.Layout.LayerGap));
        const float minimumNeuronSpacing = neuronDiameter *
            (1.0f + neuronGap);
        const float layerStep = FMath::Max(
            BaseNeuronSphereRadius * 10.0f,
            neuronDiameter * (1.0f + layerGap));
        const float centeredLayer = static_cast<float>(LayerIndex) -
            static_cast<float>(FMath::Max(LayerCount - 1, 0)) * 0.5f;
        const float direction = Settings.Layout.Direction ==
            MiaIA::Studio::StudioLayoutDirection::Reverse
            ? -1.0f
            : 1.0f;
        const float axisPosition = centeredLayer * layerStep * direction;

        if (NeuronCount <= 1)
        {
            return FVector(axisPosition, 0.0f, 0.0f);
        }

        const float angleStep = 2.0f * UE_PI /
            static_cast<float>(NeuronCount);
        const float halfChordAngle = UE_PI /
            static_cast<float>(NeuronCount);
        const float chordRadius = minimumNeuronSpacing /
            (2.0f * FMath::Max(FMath::Sin(halfChordAngle), 0.001f));
        const float ringRadius = FMath::Max(
            BaseNeuronSphereRadius * 4.0f,
            chordRadius);
        const float stagger = LayerIndex % 2 == 0
            ? 0.0f
            : angleStep * 0.5f;
        const float angle = -UE_PI * 0.5f + stagger +
            angleStep * static_cast<float>(NeuronIndex);

        return FVector(
            axisPosition,
            FMath::Cos(angle) * ringRadius,
            FMath::Sin(angle) * ringRadius);
    }

    constexpr float SpiralTokenHalfThickness = 4.0f;
    constexpr float SpiralTerminalHalfThickness = 1.5f;
    constexpr float SpiralTerminalSurfaceGap = 0.75f;
    constexpr float SpiralTokenMinimumRadiusFactor = 2.5f;
    constexpr float SpiralTokenPadding = 8.0f;
    constexpr float SpiralTokenMinimumLayerStep = 220.0f / 3.0f;
    constexpr int32 SpiralRingBaseCapacity = 8;

    float SpiralTerminalRadius(
        const FMiaIAVisualizationSettings& Settings)
    {
        return BaseNeuronSphereRadius * 0.32f * FMath::Clamp(
            Settings.NeuronScale,
            0.25f,
            3.0f);
    }

    int32 SpiralRingForNeuron(int32 NeuronIndex)
    {
        const int32 safeIndex = FMath::Max(NeuronIndex, 0);
        int32 ring = 1;

        while (safeIndex >=
            SpiralRingBaseCapacity * ring * (ring + 1) / 2)
        {
            ++ring;
        }

        return ring;
    }

    void SpiralRingLocation(
        int32 NeuronIndex,
        int32& OutRing,
        int32& OutSlot,
        int32& OutCapacity)
    {
        const int32 safeIndex = FMath::Max(NeuronIndex, 0);
        OutRing = SpiralRingForNeuron(safeIndex);
        OutCapacity = SpiralRingBaseCapacity * OutRing;
        const int32 precedingCapacity =
            SpiralRingBaseCapacity * OutRing * (OutRing - 1) / 2;
        OutSlot = safeIndex - precedingCapacity;
    }

    float SpiralTokenRadius(
        int32 NeuronCount,
        const FMiaIAVisualizationSettings& Settings)
    {
        const float terminalRadius = SpiralTerminalRadius(Settings);
        const float terminalGap = FMath::Max(
            0.0f,
            static_cast<float>(Settings.Layout.NeuronGap));
        const float terminalSpacing = terminalRadius * 2.0f *
            (1.0f + terminalGap);
        const float ringStep = terminalSpacing /
            (2.0f * FMath::Sin(
                UE_PI / SpiralRingBaseCapacity));
        const int32 outerRing = NeuronCount > 0
            ? SpiralRingForNeuron(NeuronCount - 1)
            : 0;

        const float outerTerminalCenter = NeuronCount <= 0
            ? 0.0f
            : ringStep * static_cast<float>(outerRing);
        return FMath::Max(
            terminalRadius * SpiralTokenMinimumRadiusFactor,
            outerTerminalCenter + terminalRadius + SpiralTokenPadding);
    }

    float SpiralTokenLayerPosition(
        int32 LayerIndex,
        int32 LayerCount,
        const FMiaIAVisualizationSettings& Settings)
    {
        const float terminalDiameter = SpiralTerminalRadius(Settings) *
            2.0f;
        const float layerGap = FMath::Max(
            0.0f,
            static_cast<float>(Settings.Layout.LayerGap));
        const float layerStep = FMath::Max(
            SpiralTokenMinimumLayerStep,
            SpiralTokenHalfThickness * 2.0f +
                terminalDiameter * (4.0f + layerGap));
        const float centeredLayer = static_cast<float>(LayerIndex) -
            static_cast<float>(FMath::Max(LayerCount - 1, 0)) * 0.5f;
        const float direction = Settings.Layout.Direction ==
            MiaIA::Studio::StudioLayoutDirection::Reverse
            ? -1.0f
            : 1.0f;
        return centeredLayer * layerStep * direction;
    }

    FVector SpiralTerminalPosition(
        int32 LayerIndex,
        int32 LayerCount,
        int32 NeuronIndex,
        const FMiaIAVisualizationSettings& Settings,
        bool bOutputFace)
    {
        const float terminalRadius = SpiralTerminalRadius(Settings);
        const float terminalGap = FMath::Max(
            0.0f,
            static_cast<float>(Settings.Layout.NeuronGap));
        const float terminalSpacing = terminalRadius * 2.0f *
            (1.0f + terminalGap);
        const float ringStep = terminalSpacing /
            (2.0f * FMath::Sin(
                UE_PI / SpiralRingBaseCapacity));
        int32 ring{};
        int32 slot{};
        int32 capacity{};
        SpiralRingLocation(
            NeuronIndex,
            ring,
            slot,
            capacity);
        const float spiralRadius = ringStep * static_cast<float>(ring);
        const float angleStep = 2.0f * UE_PI /
            static_cast<float>(capacity);
        const float ringOffset = ring % 2 == 0
            ? angleStep * 0.5f
            : 0.0f;
        const float angle = static_cast<float>(slot) * angleStep +
            ringOffset;
        const float direction = Settings.Layout.Direction ==
            MiaIA::Studio::StudioLayoutDirection::Reverse
            ? -1.0f
            : 1.0f;
        const float faceOffset =
            SpiralTokenHalfThickness + SpiralTerminalHalfThickness +
                SpiralTerminalSurfaceGap;
        const float axisPosition = SpiralTokenLayerPosition(
            LayerIndex,
            LayerCount,
            Settings) + direction * faceOffset *
                (bOutputFace ? 1.0f : -1.0f);

        return FVector(
            axisPosition,
            FMath::Cos(angle) * spiralRadius,
            FMath::Sin(angle) * spiralRadius);
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
            CompactLayerIndicatorLength * 2.0f +
            CompactLayerBlockGap;
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
        FMiaIAEditorTheme::StudioPalette(Theme).Background);
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
    TSet<int64> currentLayerIds;

    for (const FMiaIALayerSnapshot& layer : Snapshot.Layers)
    {
        currentLayerIds.Add(layer.Id);

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

    for (auto iterator = ManualLayerOffsets.CreateIterator(); iterator;
        ++iterator)
    {
        if (!currentLayerIds.Contains(iterator.Key()))
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
    bool compactDimensionsChanged =
        Overview.Layers.Num() != InOverview.Layers.Num();

    if (!compactDimensionsChanged)
    {
        for (int32 layerIndex = 0;
            layerIndex < Overview.Layers.Num();
            ++layerIndex)
        {
            if (Overview.Layers[layerIndex].NeuronCount !=
                InOverview.Layers[layerIndex].NeuronCount)
            {
                compactDimensionsChanged = true;
                break;
            }
        }
    }
    const bool modeChanged = bCompactMode != bInCompactMode ||
        bNetworkAggregateMode != bInNetworkAggregateMode;
    Overview = InOverview;
    bCompactMode = bInCompactMode;
    bNetworkAggregateMode = bInCompactMode &&
        bInNetworkAggregateMode;
    bSceneDirty = true;

    if (modeChanged ||
        (bNetworkAggregateMode && aggregateDimensionsChanged) ||
        (bCompactMode && !bNetworkAggregateMode &&
            compactDimensionsChanged))
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

void SMiaIA3DNetworkView::SetBackwardTraceOverlay(
    const TMap<int64, double>& InNeuronGradients,
    const TMap<int64, double>& InConnectionGradients,
    const TSet<int64>& InPlaybackConnections,
    bool bInPlaybackActive)
{
    BackwardTraceNeuronGradients = InNeuronGradients;
    BackwardTraceConnectionGradients = InConnectionGradients;
    BackwardTracePlaybackConnections = InPlaybackConnections;
    bBackwardTracePlaybackActive = bInPlaybackActive;
    MaximumBackwardTraceNeuronGradient = UE_DOUBLE_SMALL_NUMBER;
    MaximumBackwardTraceConnectionGradient = UE_DOUBLE_SMALL_NUMBER;

    for (const TPair<int64, double>& gradient :
        BackwardTraceNeuronGradients)
    {
        MaximumBackwardTraceNeuronGradient = FMath::Max(
            MaximumBackwardTraceNeuronGradient,
            FMath::Abs(gradient.Value));
    }

    for (const TPair<int64, double>& gradient :
        BackwardTraceConnectionGradients)
    {
        MaximumBackwardTraceConnectionGradient = FMath::Max(
            MaximumBackwardTraceConnectionGradient,
            FMath::Abs(gradient.Value));
    }

    bSceneDirty = true;
}

void SMiaIA3DNetworkView::SetSignalHealthOverlay(
    const TMap<int64, EMiaIASignalHealthVisualState>& InNeurons,
    const TMap<int64, EMiaIASignalHealthVisualState>& InConnections,
    bool bInActive)
{
    SignalHealthNeurons = InNeurons;
    SignalHealthConnections = InConnections;
    bSignalHealthActive = bInActive;
    bSceneDirty = true;
}

void SMiaIA3DNetworkView::SetModelComparisonOverlay(
    const TMap<int64, double>& InBiasDeltas,
    const TMap<int64, double>& InWeightDeltas,
    bool bInActive)
{
    ModelComparisonBiasDeltas = InBiasDeltas;
    ModelComparisonWeightDeltas = InWeightDeltas;
    bModelComparisonActive = bInActive;
    MaximumModelComparisonBiasDelta = UE_DOUBLE_SMALL_NUMBER;
    MaximumModelComparisonWeightDelta = UE_DOUBLE_SMALL_NUMBER;

    for (const TPair<int64, double>& delta : ModelComparisonBiasDeltas)
    {
        MaximumModelComparisonBiasDelta = FMath::Max(
            MaximumModelComparisonBiasDelta,
            FMath::Abs(delta.Value));
    }
    for (const TPair<int64, double>& delta : ModelComparisonWeightDeltas)
    {
        MaximumModelComparisonWeightDelta = FMath::Max(
            MaximumModelComparisonWeightDelta,
            FMath::Abs(delta.Value));
    }

    bSceneDirty = true;
}

void SMiaIA3DNetworkView::SetSelectedNeurons(
    const TSet<int64>& InNeuronIds,
    int64 InPrimaryNeuronId)
{
    const int64 previousPrimaryNeuronId = SelectedNeuronId;
    SelectedNeuronIds = InNeuronIds;
    SelectedNeuronId = SelectedNeuronIds.Contains(InPrimaryNeuronId)
        ? InPrimaryNeuronId
        : -1;

    if (SelectedNeuronIds.Num() > 0)
    {
        SelectedConnectionId = -1;
        SelectedLayerId = -1;
    }

    if (SelectedNeuronId != previousPrimaryNeuronId)
    {
        SelectionBlinkFrame = -1;
        bSelectionCursorVisible = true;
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
    const int64 previousLayerId = SelectedLayerId;
    SelectedLayerId = InLayerId;

    if (SelectedLayerId >= 0)
    {
        SelectedNeuronIds.Reset();
        SelectedNeuronId = -1;
        SelectedConnectionId = -1;
    }

    if (SelectedLayerId != previousLayerId)
    {
        SelectionBlinkFrame = -1;
        bSelectionCursorVisible = true;
    }

    bSceneDirty = true;
}

void SMiaIA3DNetworkView::SetTheme(EMiaIAEditorTheme InTheme)
{
    Theme = InTheme;

    if (ViewportClient.IsValid())
    {
        ViewportClient->SetBackgroundColor(
            FMiaIAEditorTheme::StudioPalette(Theme).Background);
    }

    bSceneDirty = true;
}

void SMiaIA3DNetworkView::SetVisualizationSettings(
    const FMiaIAVisualizationSettings& InSettings)
{
    const bool layoutChanged =
        VisualizationSettings.ThreeDimensionalVisualization !=
            InSettings.ThreeDimensionalVisualization ||
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
        ManualLayerOffsets.Reset();
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
    float primitiveRadius =
        VisualizationSettings.ThreeDimensionalVisualization ==
            EMiaIAVisualizationMode::SpiralTokens && !bCompactMode
        ? SpiralTerminalRadius(VisualizationSettings)
        : SphereRadius();

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
                const FVector position = CompactLayerWorldPosition(
                    layerIndex,
                    Overview.Layers.Num(),
                    VisualizationSettings.Layout.Orientation,
                    VisualizationSettings.Layout.Direction);
                const int64 inputCount = layerIndex == 0
                    ? Overview.Layers[layerIndex].NeuronCount
                    : Overview.Layers[layerIndex - 1].NeuronCount;
                const int64 outputCount =
                    Overview.Layers[layerIndex].NeuronCount;
                const int32 visibleCount = static_cast<int32>(
                    FMath::Min<int64>(
                        FMath::Max(inputCount, outputCount),
                        MaximumNetworkIndicators));
                const float halfIndicatorSpan = visibleCount <= 1
                    ? 0.0f
                    : static_cast<float>(visibleCount - 1) *
                        CompactLayerIndicatorGap * 0.5f;
                const float axisExtent = CompactSphereRadius +
                    CompactLayerIndicatorLength;

                if (VisualizationSettings.Layout.Orientation ==
                    MiaIA::Studio::StudioLayoutOrientation::Vertical)
                {
                    sceneBounds += position + FVector(
                        -halfIndicatorSpan,
                        0.0f,
                        -axisExtent);
                    sceneBounds += position + FVector(
                        halfIndicatorSpan,
                        0.0f,
                        axisExtent);
                }
                else
                {
                    sceneBounds += position + FVector(
                        -axisExtent,
                        0.0f,
                        -halfIndicatorSpan);
                    sceneBounds += position + FVector(
                        axisExtent,
                        0.0f,
                        halfIndicatorSpan);
                }
            }
        }
    }
    else
    {
        for (const FNodeRenderData& node : RenderedNodes)
        {
            sceneBounds += node.Position;

            if (node.bHasSecondaryPosition)
            {
                sceneBounds += node.SecondaryPosition;
            }
        }

        if (VisualizationSettings.ThreeDimensionalVisualization ==
            EMiaIAVisualizationMode::SpiralTokens)
        {
            for (const FLayerRenderData& layer : RenderedLayers)
            {
                sceneBounds += layer.Position - FVector(
                    layer.HalfThickness,
                    layer.Radius,
                    layer.Radius);
                sceneBounds += layer.Position + FVector(
                    layer.HalfThickness,
                    layer.Radius,
                    layer.Radius);
            }
        }
    }

    LookAt = sceneBounds.IsValid
        ? sceneBounds.GetCenter()
        : FVector::ZeroVector;
    CameraYaw = VisualizationSettings.ThreeDimensionalVisualization !=
        EMiaIAVisualizationMode::Classic && !bCompactMode
        ? -45.0f
        : -90.0f;
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
    ManualLayerOffsets.Reset();
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
    if (SelectedNeuronId >= 0 ||
        (bCompactMode && !bNetworkAggregateMode &&
            SelectedLayerId >= 0))
    {
        const int32 blinkFrame =
            FMath::FloorToInt(InCurrentTime * 2.0);

        if (blinkFrame != SelectionBlinkFrame)
        {
            SelectionBlinkFrame = blinkFrame;
            bSelectionCursorVisible = blinkFrame % 2 == 0;
            Invalidate(EInvalidateWidgetReason::Paint);
        }
    }

    if (!SceneViewport.IsValid() || !ViewportClient.IsValid())
    {
        return;
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
        FMiaIAEditorTheme::StudioPalette(Theme);
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
        const float nodeRadius = bCompactMode
            ? compactRadius
            : node.VisualRadius > 0.0f
                ? node.VisualRadius
                : SphereRadius();
        const float projectedViewportRadius = ProjectedSphereRadius(
            node.Position,
            nodeRadius);
        const float projectedRadiusX = projectedViewportRadius *
            static_cast<float>(viewportToLocal.X);
        const float projectedRadiusY = projectedViewportRadius *
            static_cast<float>(viewportToLocal.Y);
        const float projectedRadius =
            (projectedRadiusX + projectedRadiusY) * 0.5f;

        const bool selected = bCompactMode && !bNetworkAggregateMode
            ? node.Id == SelectedLayerId
            : SelectedNeuronIds.Contains(node.Id);

        if (selected && bCompactMode && bSelectionCursorVisible)
        {
            FVector2D projectedMinimum;
            FVector2D projectedMaximum;
            const bool hasProjectedBounds = ProjectedSphereBounds(
                node.Position,
                compactRadius,
                projectedMinimum,
                projectedMaximum);
            float cursorLeft = hasProjectedBounds
                ? projectedMinimum.X * viewportToLocal.X
                : localPosition.X - projectedRadiusX;
            float cursorRight = hasProjectedBounds
                ? projectedMaximum.X * viewportToLocal.X
                : localPosition.X + projectedRadiusX;
            const float cursorY = hasProjectedBounds
                ? projectedMaximum.Y * viewportToLocal.Y +
                    SelectionCursorGap
                : localPosition.Y + projectedRadiusY +
                    SelectionCursorGap;

            if (VisualizationSettings.bAlwaysShowSelectionCursor &&
                cursorRight - cursorLeft < 12.0f)
            {
                const float cursorCenter =
                    (cursorLeft + cursorRight) * 0.5f;
                cursorLeft = cursorCenter - 6.0f;
                cursorRight = cursorCenter + 6.0f;
            }

            const TArray<FVector2D> selectionPoints{
                FVector2D(cursorLeft, cursorY),
                FVector2D(cursorRight, cursorY)
            };

            FSlateDrawElement::MakeLines(
                OutDrawElements,
                overlayLayer,
                AllottedGeometry.ToPaintGeometry(),
                selectionPoints,
                ESlateDrawEffect::None,
                palette.Selection,
                true,
                SelectionCursorThickness);
        }

        const bool showSelectionCursor =
            !bCompactMode &&
            (VisualizationSettings.bAlwaysShowSelectionCursor ||
                (RenderedNodes.Num() <= 500 &&
                    projectedRadius * 2.0f >= 24.0f));

        if (node.Id == SelectedNeuronId &&
            bSelectionCursorVisible &&
            showSelectionCursor)
        {
            FVector2D projectedMinimum;
            FVector2D projectedMaximum;
            const bool hasProjectedBounds = ProjectedSphereBounds(
                node.Position,
                nodeRadius,
                projectedMinimum,
                projectedMaximum);
            const float cursorX = hasProjectedBounds
                ? projectedMinimum.X * viewportToLocal.X -
                    SelectionCursorGap
                : localPosition.X - projectedRadiusX -
                    SelectionCursorGap;
            float cursorTop = hasProjectedBounds
                ? projectedMinimum.Y * viewportToLocal.Y
                : localPosition.Y - projectedRadiusY;
            float cursorBottom = hasProjectedBounds
                ? projectedMaximum.Y * viewportToLocal.Y
                : localPosition.Y + projectedRadiusY;

            if (VisualizationSettings.bAlwaysShowSelectionCursor &&
                cursorBottom - cursorTop < 12.0f)
            {
                const float cursorCenter =
                    (cursorTop + cursorBottom) * 0.5f;
                cursorTop = cursorCenter - 6.0f;
                cursorBottom = cursorCenter + 6.0f;
            }

            const TArray<FVector2D> cursorPoints{
                FVector2D(cursorX, cursorTop),
                FVector2D(cursorX, cursorBottom)
            };

            FSlateDrawElement::MakeLines(
                OutDrawElements,
                overlayLayer,
                AllottedGeometry.ToPaintGeometry(),
                cursorPoints,
                ESlateDrawEffect::None,
                palette.Selection,
                true,
                SelectionCursorThickness);
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
                const FMiaIALayerSnapshot* selectedLayer =
                    VisualizationSettings.ThreeDimensionalVisualization ==
                            EMiaIAVisualizationMode::SpiralTokens &&
                        SelectedLayerId >= 0
                    ? Snapshot.Layers.FindByPredicate(
                        [this](const FMiaIALayerSnapshot& layer)
                        {
                            return layer.Id == SelectedLayerId;
                        })
                    : nullptr;
                const bool nodeBelongsToSelectedLayer = selectedLayer &&
                    selectedLayer->Neurons.ContainsByPredicate(
                        [&nodeValue](
                            const FMiaIANeuronSnapshot& neuron)
                        {
                            return neuron.Id == nodeValue.Id;
                        });

                if (nodeBelongsToSelectedLayer)
                {
                    PendingLayerDragId = SelectedLayerId;
                    PendingLayerClickNeuronId = nodeValue.Id;
                    PendingLayerDragStart = viewportPosition;
                    bPendingLayerClickAdditive = additive;
                    bPendingLayerDrag = true;
                    return FReply::Handled().CaptureMouse(
                        SharedThis(this));
                }

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

            if (const FLayerRenderData* layer =
                FindLayerTokenAt(viewportPosition))
            {
                const FLayerRenderData layerValue = *layer;
                SelectedConnectionId = -1;
                OnLayerSelected.ExecuteIfBound(layerValue.Id);
                RebuildScene();

                const bool startedDrag = BeginLayerDrag(
                    viewportPosition,
                    layerValue);
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
    const bool finishesLayerDrag =
        button == EKeys::LeftMouseButton && bDraggingLayer;
    const bool finishesPendingLayerDrag =
        button == EKeys::LeftMouseButton && bPendingLayerDrag;
    const bool finishesMarquee =
        button == EKeys::LeftMouseButton && bMarqueeSelecting;

    if (!finishesOrbit && !finishesPan &&
        !finishesNodeDrag && !finishesLayerDrag &&
        !finishesPendingLayerDrag && !finishesMarquee)
    {
        return SViewport::OnMouseButtonUp(MyGeometry, MouseEvent);
    }

    if (finishesMarquee)
    {
        CompleteMarqueeSelection(MyGeometry);
    }

    if (finishesPendingLayerDrag &&
        PendingLayerClickNeuronId >= 0)
    {
        if (bPendingLayerClickAdditive)
        {
            if (SelectedNeuronIds.Contains(PendingLayerClickNeuronId))
            {
                SelectedNeuronIds.Remove(PendingLayerClickNeuronId);
            }
            else
            {
                SelectedNeuronIds.Add(PendingLayerClickNeuronId);
            }
        }
        else
        {
            SelectedNeuronIds.Reset();
            SelectedNeuronIds.Add(PendingLayerClickNeuronId);
        }

        SelectedNeuronId = SelectedNeuronIds.Contains(
            PendingLayerClickNeuronId)
            ? PendingLayerClickNeuronId
            : -1;

        if (SelectedNeuronId < 0)
        {
            for (const int64 selectedId : SelectedNeuronIds)
            {
                SelectedNeuronId = selectedId;
                break;
            }
        }

        SelectedConnectionId = -1;
        NotifyNeuronSelectionChanged();
        RebuildScene();
    }

    bOrbiting = false;
    bPanning = false;
    bDraggingNode = false;
    bDraggingLayer = false;
    bPendingLayerDrag = false;
    bMarqueeSelecting = false;
    DraggedNeuronId = -1;
    DraggedLayerId = -1;
    PendingLayerDragId = -1;
    PendingLayerClickNeuronId = -1;
    bPendingLayerClickAdditive = false;
    DragStartPositions.Reset();
    return FReply::Handled().ReleaseMouseCapture();
}

FReply SMiaIA3DNetworkView::OnMouseMove(
    const FGeometry& MyGeometry,
    const FPointerEvent& MouseEvent)
{
    if (!HasMouseCapture() ||
        (!bOrbiting && !bPanning &&
            !bDraggingNode && !bDraggingLayer &&
            !bPendingLayerDrag && !bMarqueeSelecting))
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

    if (bPendingLayerDrag && SceneViewport.IsValid())
    {
        const FVector2D localSize = MyGeometry.GetLocalSize();
        const FIntPoint viewportSize = SceneViewport->GetSizeXY();

        if (localSize.X > 0.0f && localSize.Y > 0.0f)
        {
            const FVector2D viewportPosition(
                localPosition.X * viewportSize.X / localSize.X,
                localPosition.Y * viewportSize.Y / localSize.Y);

            if (FVector2D::Distance(
                    viewportPosition,
                    PendingLayerDragStart) >=
                LayerDragActivationDistance)
            {
                const FLayerRenderData* layer =
                    RenderedLayers.FindByPredicate(
                        [this](const FLayerRenderData& candidate)
                        {
                            return candidate.Id == PendingLayerDragId;
                        });

                if (layer && BeginLayerDrag(
                    PendingLayerDragStart,
                    *layer))
                {
                    bPendingLayerDrag = false;
                    PendingLayerClickNeuronId = -1;
                    UpdateLayerDrag(viewportPosition);
                }
            }
        }

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
    else if (bDraggingLayer && SceneViewport.IsValid())
    {
        const FVector2D localSize = MyGeometry.GetLocalSize();
        const FIntPoint viewportSize = SceneViewport->GetSizeXY();

        if (localSize.X > 0.0f && localSize.Y > 0.0f)
        {
            UpdateLayerDrag(FVector2D(
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
    bDraggingLayer = false;
    bPendingLayerDrag = false;
    bMarqueeSelecting = false;
    DraggedNeuronId = -1;
    DraggedLayerId = -1;
    PendingLayerDragId = -1;
    PendingLayerClickNeuronId = -1;
    bPendingLayerClickAdditive = false;
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
    if (!VisualizationSettings.bShowConnections ||
        VisualizationSettings.ConnectionScale <= UE_KINDA_SMALL_NUMBER)
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
    RenderedLayers.Reset();

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
        FMiaIAEditorTheme::StudioPalette(Theme);
    TMap<int64, FVector> positions;
    TMap<int64, FVector> inputPositions;
    const bool spiralTokens =
        VisualizationSettings.ThreeDimensionalVisualization ==
            EMiaIAVisualizationMode::SpiralTokens;
    int32 neuronCount = 0;

    for (const FMiaIALayerSnapshot& layer : Snapshot.Layers)
    {
        neuronCount += layer.Neurons.Num();
    }

    positions.Reserve(neuronCount);
    inputPositions.Reserve(neuronCount);
    RenderedNodes.Reserve(neuronCount);
    RenderedLayers.Reserve(spiralTokens ? Snapshot.Layers.Num() : 0);
    const int32 visibleConnectionCount =
        VisualizationSettings.bShowConnections
            ? Snapshot.Connections.Num()
            : 0;
    ViewportClient->ReservePrimitives(
        spiralTokens ? 0 : neuronCount,
        visibleConnectionCount,
        spiralTokens
            ? Snapshot.Layers.Num() + neuronCount * 2
            : 0);

    for (int32 layerIndex = 0;
        layerIndex < Snapshot.Layers.Num();
        ++layerIndex)
    {
        const FMiaIALayerSnapshot& layer = Snapshot.Layers[layerIndex];

        if (spiralTokens)
        {
            const float axisPosition = SpiralTokenLayerPosition(
                layerIndex,
                Snapshot.Layers.Num(),
                VisualizationSettings);
            const FVector layerOffset = ManualLayerOffsets.FindRef(
                layer.Id);
            const FVector layerPosition =
                FVector(axisPosition, 0.0f, 0.0f) + layerOffset;
            const float tokenRadius = SpiralTokenRadius(
                layer.Neurons.Num(),
                VisualizationSettings);
            FLinearColor tokenColor(
                palette.Background.R * 0.72f +
                    palette.InactiveNeuron.R * 0.28f,
                palette.Background.G * 0.72f +
                    palette.InactiveNeuron.G * 0.28f,
                palette.Background.B * 0.72f +
                    palette.InactiveNeuron.B * 0.28f,
                1.0f);

            if (layer.Id == SelectedLayerId)
            {
                tokenColor = palette.Selection.CopyWithNewOpacity(1.0f);
            }

            RenderedLayers.Add({
                layer.Id,
                layerPosition,
                tokenRadius,
                SpiralTokenHalfThickness
            });
            ViewportClient->AddDisc(
                layerPosition,
                FVector::ForwardVector,
                tokenColor,
                tokenRadius,
                SpiralTokenHalfThickness,
                48);
        }

        for (int32 neuronIndex = 0;
            neuronIndex < layer.Neurons.Num();
            ++neuronIndex)
        {
            const FMiaIANeuronSnapshot& neuron =
                layer.Neurons[neuronIndex];
            FVector automaticPosition;
            FVector inputPosition;

            if (spiralTokens)
            {
                const FVector layerOffset = ManualLayerOffsets.FindRef(
                    layer.Id);
                automaticPosition = SpiralTerminalPosition(
                    layerIndex,
                    Snapshot.Layers.Num(),
                    neuronIndex,
                    VisualizationSettings,
                    true) + layerOffset;
                inputPosition = SpiralTerminalPosition(
                    layerIndex,
                    Snapshot.Layers.Num(),
                    neuronIndex,
                    VisualizationSettings,
                    false) + layerOffset;
            }
            else if (VisualizationSettings.ThreeDimensionalVisualization ==
                EMiaIAVisualizationMode::CoaxialRings)
            {
                automaticPosition = CoaxialRingWorldPosition(
                    layerIndex,
                    Snapshot.Layers.Num(),
                    neuronIndex,
                    layer.Neurons.Num(),
                    VisualizationSettings);
                inputPosition = automaticPosition;
            }
            else
            {
                const MiaIA::Studio::StudioPosition layoutPosition =
                    MiaIA::Studio::StudioTopologyBuilder::
                        DetailedLayoutPosition(
                            layerIndex,
                            Snapshot.Layers.Num(),
                            neuronIndex,
                            layer.Neurons.Num(),
                            VisualizationSettings.Layout);
                automaticPosition = DetailedWorldPosition(
                    layoutPosition,
                    LayoutUnitSize());
                inputPosition = automaticPosition;
            }
            const FVector* manualPosition =
                spiralTokens
                ? nullptr
                : ManualNodePositions.Find(neuron.Id);
            const FVector position = manualPosition
                ? *manualPosition
                : automaticPosition;
            const FVector resolvedInputPosition = manualPosition
                ? *manualPosition
                : inputPosition;
            positions.Add(neuron.Id, position);
            inputPositions.Add(neuron.Id, resolvedInputPosition);

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
            const double* backwardGradient =
                BackwardTraceNeuronGradients.Find(neuron.Id);
            const EMiaIASignalHealthVisualState* signalHealth =
                SignalHealthNeurons.Find(neuron.Id);
            const double* comparisonDelta =
                ModelComparisonBiasDeltas.Find(neuron.Id);
            const FLinearColor color = comparisonDelta
                ? SignedNeuronColor(
                    *comparisonDelta,
                    MaximumModelComparisonBiasDelta)
                : bModelComparisonActive
                    ? palette.InactiveNeuron.CopyWithNewOpacity(0.15f)
                : signalHealth
                ? SignalHealthColor(*signalHealth)
                : bSignalHealthActive
                    ? palette.InactiveNeuron.CopyWithNewOpacity(0.18f)
                : backwardGradient
                ? SignedNeuronColor(
                    *backwardGradient,
                    MaximumBackwardTraceNeuronGradient)
                : bBackwardTracePlaybackActive
                    ? palette.InactiveNeuron.CopyWithNewOpacity(0.28f)
                : displaysTelemetry
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

            if (spiralTokens)
            {
                const float terminalRadius = SpiralTerminalRadius(
                    VisualizationSettings);
                RenderedNodes.Add({
                    neuron.Id,
                    position,
                    resolvedInputPosition,
                    terminalRadius,
                    true
                });
                ViewportClient->AddDisc(
                    position,
                    FVector::ForwardVector,
                    color,
                    terminalRadius,
                    SpiralTerminalHalfThickness,
                    12);
                ViewportClient->AddDisc(
                    resolvedInputPosition,
                    FVector::ForwardVector,
                    color,
                    terminalRadius,
                    SpiralTerminalHalfThickness,
                    12);
            }
            else
            {
                RenderedNodes.Add({
                    neuron.Id,
                    position,
                    FVector::ZeroVector,
                    SphereRadius(),
                    false
                });
                ViewportClient->AddSphere(
                    position,
                    color,
                    SphereRadius());
            }
        }
    }

    RenderedConnections.Reserve(visibleConnectionCount);

    for (int32 connectionIndex = 0;
        connectionIndex < visibleConnectionCount;
        ++connectionIndex)
    {
        const FMiaIAConnectionSnapshot& connection =
            Snapshot.Connections[connectionIndex];

        if (!ShouldDrawConnection(connection))
        {
            continue;
        }

        const FVector* from = positions.Find(connection.FromNeuron);
        const FVector* to = inputPositions.Find(connection.ToNeuron);

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
        const double* backwardGradient =
            BackwardTraceConnectionGradients.Find(connection.Id);
        const bool backwardPlaybackConnection =
            BackwardTracePlaybackConnections.Contains(connection.Id);
        const EMiaIASignalHealthVisualState* signalHealth =
            SignalHealthConnections.Find(connection.Id);
        const double* comparisonDelta =
            ModelComparisonWeightDeltas.Find(connection.Id);
        const double displayedValue = comparisonDelta
            ? *comparisonDelta
            : backwardGradient
            ? *backwardGradient
            : traceContribution
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
            static_cast<float>(backwardPlaybackConnection ||
                playbackConnection
                ? 1.0
                : comparisonDelta
                ? FMath::Abs(displayedValue) /
                    MaximumModelComparisonWeightDelta
                : backwardGradient
                ? FMath::Abs(displayedValue) /
                    MaximumBackwardTraceConnectionGradient
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
            : comparisonDelta
                ? SignedConnectionColor(
                    displayedValue,
                    MaximumModelComparisonWeightDelta)
            : bModelComparisonActive
                ? palette.SubduedText.CopyWithNewOpacity(0.06f)
            : signalHealth
                ? SignalHealthColor(*signalHealth).CopyWithNewOpacity(0.8f)
            : bSignalHealthActive
                ? palette.SubduedText.CopyWithNewOpacity(0.08f)
            : backwardGradient
                ? SignedConnectionColor(
                    displayedValue,
                    MaximumBackwardTraceConnectionGradient)
                : backwardPlaybackConnection
                    ? palette.Debug
            : traceContribution
                ? SignedConnectionColor(
                    displayedValue,
                    MaximumForwardTraceContribution)
                : playbackConnection
                    ? palette.Debug
                : bBackwardTracePlaybackActive ||
                    !BackwardTraceNeuronGradients.IsEmpty() ||
                    bForwardTracePlaybackActive ||
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
        FMiaIAEditorTheme::StudioPalette(Theme);
    const int32 layerCount = Overview.Layers.Num();
    const auto visibleCount = [](int64 count)
    {
        return static_cast<int32>(FMath::Min<int64>(
            FMath::Max<int64>(0, count),
            MaximumNetworkIndicators));
    };
    int32 indicatorCount{};

    for (int32 layerIndex = 0;
        layerIndex < layerCount;
        ++layerIndex)
    {
        const int64 inputCount = layerIndex == 0
            ? Overview.Layers[layerIndex].NeuronCount
            : Overview.Layers[layerIndex - 1].NeuronCount;
        indicatorCount += visibleCount(inputCount) +
            visibleCount(Overview.Layers[layerIndex].NeuronCount);
    }

    const bool isVertical = VisualizationSettings.Layout.Orientation ==
        MiaIA::Studio::StudioLayoutOrientation::Vertical;
    const bool isReverse = VisualizationSettings.Layout.Direction ==
        MiaIA::Studio::StudioLayoutDirection::Reverse;
    const FVector flowAxis = isVertical
        ? FVector(0.0f, 0.0f, isReverse ? 1.0f : -1.0f)
        : FVector(isReverse ? -1.0f : 1.0f, 0.0f, 0.0f);
    const FVector spreadAxis = isVertical
        ? FVector(1.0f, 0.0f, 0.0f)
        : FVector(0.0f, 0.0f, 1.0f);
    FVector previousPosition = FVector::ZeroVector;
    bool hasPrevious = false;

    ViewportClient->ReservePrimitives(
        layerCount,
        FMath::Max(0, layerCount - 1) + indicatorCount);

    const auto addIndicators = [this, &spreadAxis](
        int32 count,
        const FVector& center,
        const FVector& direction,
        const FLinearColor& color)
    {
        for (int32 indicatorIndex = 0;
            indicatorIndex < count;
            ++indicatorIndex)
        {
            const float offset =
                (static_cast<float>(indicatorIndex) -
                    static_cast<float>(count - 1) * 0.5f) *
                CompactLayerIndicatorGap;
            const FVector start = center +
                direction * CompactSphereRadius + spreadAxis * offset;
            ViewportClient->AddLine(
                start,
                start + direction * CompactLayerIndicatorLength,
                color.CopyWithNewOpacity(0.82f),
                3.0f);
        }
    };

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
        const int64 inputCount = layerIndex == 0
            ? Overview.Layers[layerIndex].NeuronCount
            : Overview.Layers[layerIndex - 1].NeuronCount;

        addIndicators(
            visibleCount(inputCount),
            position,
            -flowAxis,
            palette.Debug);
        addIndicators(
            visibleCount(Overview.Layers[layerIndex].NeuronCount),
            position,
            flowAxis,
            palette.ActiveNeuron);

        if (hasPrevious && VisualizationSettings.bShowConnections)
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
        FMiaIAEditorTheme::StudioPalette(Theme);
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

bool SMiaIA3DNetworkView::ProjectedSphereBounds(
    const FVector& WorldPositionValue,
    float WorldRadius,
    FVector2D& OutMinimum,
    FVector2D& OutMaximum) const
{
    if (WorldRadius <= UE_KINDA_SMALL_NUMBER)
    {
        return false;
    }

    const FRotator rotation(CameraPitch, CameraYaw, 0.0f);
    const FRotationMatrix rotationMatrix(rotation);
    const FVector forward = rotation.Vector();
    const FVector right = rotationMatrix.GetScaledAxis(EAxis::Y);
    const FVector up = rotationMatrix.GetScaledAxis(EAxis::Z);
    const FVector cameraLocation =
        LookAt - forward * CameraDistance;
    const FVector cameraToCenter =
        WorldPositionValue - cameraLocation;
    const double depth = FVector::DotProduct(cameraToCenter, forward);

    if (depth <= WorldRadius)
    {
        return false;
    }

    const auto projectTangents = [this,
        &cameraLocation,
        &cameraToCenter,
        &forward,
        depth,
        WorldRadius](
            const FVector& screenAxis,
            FVector2D& first,
            FVector2D& second)
    {
        const double lateral = FVector::DotProduct(
            cameraToCenter,
            screenAxis);
        const double distanceSquared =
            depth * depth + lateral * lateral;
        const double radiusSquared =
            static_cast<double>(WorldRadius) * WorldRadius;

        if (distanceSquared <= radiusSquared)
        {
            return false;
        }

        const double projectionScale =
            1.0 - radiusSquared / distanceSquared;
        const double tangentScale = WorldRadius * FMath::Sqrt(
            distanceSquared - radiusSquared) / distanceSquared;
        const double firstDepth =
            projectionScale * depth - tangentScale * lateral;
        const double firstLateral =
            projectionScale * lateral + tangentScale * depth;
        const double secondDepth =
            projectionScale * depth + tangentScale * lateral;
        const double secondLateral =
            projectionScale * lateral - tangentScale * depth;
        const FVector fixedOffset = cameraToCenter -
            forward * depth - screenAxis * lateral;

        return ProjectToViewport(
                cameraLocation + fixedOffset +
                    forward * firstDepth +
                    screenAxis * firstLateral,
                first) &&
            ProjectToViewport(
                cameraLocation + fixedOffset +
                    forward * secondDepth +
                    screenAxis * secondLateral,
                second);
    };

    FVector2D horizontalFirst;
    FVector2D horizontalSecond;
    FVector2D verticalFirst;
    FVector2D verticalSecond;

    if (!projectTangents(right, horizontalFirst, horizontalSecond) ||
        !projectTangents(up, verticalFirst, verticalSecond))
    {
        return false;
    }

    OutMinimum = FVector2D(
        FMath::Min(horizontalFirst.X, horizontalSecond.X),
        FMath::Min(verticalFirst.Y, verticalSecond.Y));
    OutMaximum = FVector2D(
        FMath::Max(horizontalFirst.X, horizontalSecond.X),
        FMath::Max(verticalFirst.Y, verticalSecond.Y));
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
    double closestDistance = UE_DOUBLE_BIG_NUMBER;

    for (const FNodeRenderData& node : RenderedNodes)
    {
        const float nodeRadius = bNetworkAggregateMode
            ? NetworkAggregateSphereRadius
            : bCompactMode
                ? CompactSphereRadius
                : node.VisualRadius > 0.0f
                    ? node.VisualRadius
                    : SphereRadius();
        const auto considerPosition = [&](const FVector& position)
        {
            FVector2D projected;

            if (!ProjectToViewport(position, projected))
            {
                return;
            }

            const double distance = FVector2D::Distance(
                ViewportPosition,
                projected);
            const double selectionDistance = FMath::Max(
                static_cast<double>(NodeSelectionDistance),
                static_cast<double>(ProjectedSphereRadius(
                    position,
                    nodeRadius)));

            if (distance <= selectionDistance &&
                distance <= closestDistance)
            {
                closestNode = &node;
                closestDistance = distance;
            }
        };

        considerPosition(node.Position);

        if (node.bHasSecondaryPosition)
        {
            considerPosition(node.SecondaryPosition);
        }
    }

    return closestNode;
}

const SMiaIA3DNetworkView::FLayerRenderData*
SMiaIA3DNetworkView::FindLayerTokenAt(
    const FVector2D& ViewportPosition) const
{
    if (bCompactMode ||
        VisualizationSettings.ThreeDimensionalVisualization !=
            EMiaIAVisualizationMode::SpiralTokens)
    {
        return nullptr;
    }

    FVector rayOrigin;
    FVector rayDirection;

    if (!DeprojectFromViewport(
            ViewportPosition,
            rayOrigin,
            rayDirection))
    {
        return nullptr;
    }

    const FLayerRenderData* closestLayer = nullptr;
    double closestRayDistance = UE_DOUBLE_BIG_NUMBER;

    for (const FLayerRenderData& layer : RenderedLayers)
    {
        const FVector localOrigin = rayOrigin - layer.Position;
        const auto considerDistance = [&](double distance)
        {
            if (distance >= 0.0 && distance < closestRayDistance)
            {
                closestRayDistance = distance;
                closestLayer = &layer;
            }
        };

        if (!FMath::IsNearlyZero(rayDirection.X))
        {
            const float faceOffsets[2]{
                -layer.HalfThickness,
                layer.HalfThickness
            };

            for (const float faceOffset : faceOffsets)
            {
                const double distance =
                    (faceOffset - localOrigin.X) / rayDirection.X;

                if (distance < 0.0)
                {
                    continue;
                }

                const FVector hit = localOrigin +
                    rayDirection * distance;

                if (hit.Y * hit.Y + hit.Z * hit.Z <=
                    layer.Radius * layer.Radius)
                {
                    considerDistance(distance);
                }
            }
        }

        const double radialDirectionSquared =
            rayDirection.Y * rayDirection.Y +
            rayDirection.Z * rayDirection.Z;

        if (radialDirectionSquared > UE_DOUBLE_SMALL_NUMBER)
        {
            const double radialDot =
                localOrigin.Y * rayDirection.Y +
                localOrigin.Z * rayDirection.Z;
            const double radialOriginSquared =
                localOrigin.Y * localOrigin.Y +
                localOrigin.Z * localOrigin.Z;
            const double discriminant = radialDot * radialDot -
                radialDirectionSquared *
                    (radialOriginSquared -
                        layer.Radius * layer.Radius);

            if (discriminant >= 0.0)
            {
                const double squareRoot = FMath::Sqrt(discriminant);
                const double distances[2]{
                    (-radialDot - squareRoot) /
                        radialDirectionSquared,
                    (-radialDot + squareRoot) /
                        radialDirectionSquared
                };

                for (const double distance : distances)
                {
                    if (distance < 0.0)
                    {
                        continue;
                    }

                    const double hitX = localOrigin.X +
                        rayDirection.X * distance;

                    if (FMath::Abs(hitX) <= layer.HalfThickness)
                    {
                        considerDistance(distance);
                    }
                }
            }
        }
    }

    return closestLayer;
}

bool SMiaIA3DNetworkView::BeginNodeDrag(
    const FVector2D& ViewportPosition,
    const FNodeRenderData& Node)
{
    if (VisualizationSettings.ThreeDimensionalVisualization ==
        EMiaIAVisualizationMode::SpiralTokens)
    {
        return false;
    }

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

bool SMiaIA3DNetworkView::BeginLayerDrag(
    const FVector2D& ViewportPosition,
    const FLayerRenderData& Layer)
{
    if (VisualizationSettings.ThreeDimensionalVisualization !=
            EMiaIAVisualizationMode::SpiralTokens ||
        bCompactMode)
    {
        return false;
    }

    FVector rayOrigin;
    FVector rayDirection;

    if (!DeprojectFromViewport(
            ViewportPosition,
            rayOrigin,
            rayDirection))
    {
        return false;
    }

    DraggedLayerId = Layer.Id;
    DragPlaneOrigin = Layer.Position;
    DragPlaneNormal = FRotator(
        CameraPitch,
        CameraYaw,
        0.0f).Vector();
    DragStartIntersection = FMath::RayPlaneIntersection(
        rayOrigin,
        rayDirection,
        FPlane(DragPlaneOrigin, DragPlaneNormal));
    DragStartLayerOffset = ManualLayerOffsets.FindRef(Layer.Id);
    bDraggingLayer = true;
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

void SMiaIA3DNetworkView::UpdateLayerDrag(
    const FVector2D& ViewportPosition)
{
    if (!bDraggingLayer || DraggedLayerId < 0)
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
    ManualLayerOffsets.Add(
        DraggedLayerId,
        DragStartLayerOffset + intersection - DragStartIntersection);
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
        const auto positionIsInside = [&](const FVector& position)
        {
            FVector2D projected;

            if (!ProjectToViewport(position, projected))
            {
                return false;
            }

            const FVector2D localPosition(
                projected.X * localSize.X / viewportSize.X,
                projected.Y * localSize.Y / viewportSize.Y);
            return localPosition.X >= minimum.X &&
                localPosition.X <= maximum.X &&
                localPosition.Y >= minimum.Y &&
                localPosition.Y <= maximum.Y;
        };

        if (positionIsInside(node.Position) ||
            (node.bHasSecondaryPosition &&
                positionIsInside(node.SecondaryPosition)))
        {
            selection.Add(node.Id);
            primaryNeuronId = node.Id;
        }
    }

    if (!bMarqueeAdditive &&
        VisualizationSettings.ThreeDimensionalVisualization ==
            EMiaIAVisualizationMode::SpiralTokens)
    {
        int64 completeLayerId = -1;
        int32 completeLayerCount = 0;

        for (const FMiaIALayerSnapshot& layer : Snapshot.Layers)
        {
            if (layer.Neurons.IsEmpty())
            {
                continue;
            }

            bool allNeuronsSelected = true;

            for (const FMiaIANeuronSnapshot& neuron : layer.Neurons)
            {
                if (!selection.Contains(neuron.Id))
                {
                    allNeuronsSelected = false;
                    break;
                }
            }

            if (allNeuronsSelected)
            {
                completeLayerId = layer.Id;
                ++completeLayerCount;
            }
        }

        if (completeLayerCount == 1)
        {
            SelectedNeuronIds.Reset();
            SelectedNeuronId = -1;
            SelectedConnectionId = -1;
            SelectedLayerId = completeLayerId;
            OnLayerSelected.ExecuteIfBound(completeLayerId);
            RebuildScene();
            return;
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
        FMiaIAEditorTheme::StudioPalette(Theme);
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
        FMiaIAEditorTheme::StudioPalette(Theme);
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
        FMiaIAEditorTheme::StudioPalette(Theme);
    const float strength = FMath::Clamp(
        static_cast<float>(FMath::Abs(Value) / Maximum),
        0.0f,
        1.0f);
    const FLinearColor target = Value >= 0.0
        ? palette.PositiveWeight
        : palette.NegativeWeight;
    return target.CopyWithNewOpacity(0.15f + strength * 0.85f);
}

FLinearColor SMiaIA3DNetworkView::SignalHealthColor(
    EMiaIASignalHealthVisualState State) const
{
    const FMiaIAVisualizationPalette palette =
        FMiaIAEditorTheme::VisualizationPalette(Theme);
    switch (State)
    {
    case EMiaIASignalHealthVisualState::Inactive:
        return palette.DiagnosticInactive;
    case EMiaIASignalHealthVisualState::Saturated:
        return palette.DiagnosticSaturated;
    case EMiaIASignalHealthVisualState::VanishingGradient:
        return palette.DiagnosticVanishing;
    case EMiaIASignalHealthVisualState::ExplodingGradient:
        return palette.DiagnosticExploding;
    case EMiaIASignalHealthVisualState::Mixed:
    default:
        return FLinearColor::LerpUsingHSV(
            palette.DiagnosticSaturated,
            palette.DiagnosticVanishing,
            0.5f);
    }
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
