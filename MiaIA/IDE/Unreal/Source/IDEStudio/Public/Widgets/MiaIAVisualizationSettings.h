#pragma once

#include "CoreMinimal.h"
#include "StudioTopology.h"

enum class EMiaIAConnectionDisplayMode : uint8
{
    All,
    Selected
};

enum class EMiaIAVisualizationMode : uint8
{
    Classic,
    CoaxialRings,
    SpiralTokens
};

enum class EMiaIASignalHealthVisualState : uint8
{
    Inactive,
    Saturated,
    VanishingGradient,
    ExplodingGradient,
    Mixed
};

struct FMiaIAVisualizationSettings
{
    MiaIA::Studio::StudioLayoutPreferences Layout;
    EMiaIAVisualizationMode TwoDimensionalVisualization{
        EMiaIAVisualizationMode::Classic};
    EMiaIAVisualizationMode ThreeDimensionalVisualization{
        EMiaIAVisualizationMode::Classic};
    EMiaIAConnectionDisplayMode ConnectionDisplay{
        EMiaIAConnectionDisplayMode::All};
    float NeuronScale{1.0f};
    float ConnectionScale{1.0f};
    bool bShowConnections{true};
    bool bShowNeuronLabels{true};
    bool bAlwaysShowSelectionCursor{};
};
