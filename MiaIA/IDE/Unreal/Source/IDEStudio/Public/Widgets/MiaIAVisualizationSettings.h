#pragma once

#include "CoreMinimal.h"
#include "StudioTopology.h"

enum class EMiaIAConnectionDisplayMode : uint8
{
    All,
    Selected
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
    EMiaIAConnectionDisplayMode ConnectionDisplay{
        EMiaIAConnectionDisplayMode::All};
    float NeuronScale{1.0f};
    float ConnectionScale{1.0f};
    bool bShowNeuronLabels{true};
    bool bAlwaysShowSelectionCursor{};
};
