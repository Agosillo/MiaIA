#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateTypes.h"

enum class EMiaIAEditorTheme : uint8
{
    FollowUnreal,
    Dark,
    Light
};

enum class EMiaIAVisualizationPalettePreset : uint8
{
    MiaIAClassic,
    HighContrast,
    ColorBlindSafe,
    Monochrome,
    Custom
};

enum class EMiaIAVisualizationColorRole : uint8
{
    InactiveNeuron,
    ActiveNeuron,
    PositiveContribution,
    NegativeContribution,
    Selection,
    Debug
};

struct FMiaIAVisualizationPalette
{
    FLinearColor InactiveNeuron;
    FLinearColor ActiveNeuron;
    FLinearColor PositiveWeight;
    FLinearColor NegativeWeight;
    FLinearColor Selection;
    FLinearColor Debug;
};

struct FMiaIAEditorPalette
{
    FLinearColor Background;
    FLinearColor Panel;
    FLinearColor Surface;
    FLinearColor Text;
    FLinearColor SubduedText;
    FLinearColor InactiveNeuron;
    FLinearColor ActiveNeuron;
    FLinearColor PositiveWeight;
    FLinearColor NegativeWeight;
    FLinearColor Selection;
    FLinearColor Debug;
};

class IDESTUDIO_API FMiaIAEditorTheme
{
public:
    static EMiaIAEditorTheme Load();
    static void Save(EMiaIAEditorTheme Theme);
    static FMiaIAEditorPalette Palette(EMiaIAEditorTheme Theme);
    static FMiaIAEditorPalette StudioPalette(EMiaIAEditorTheme Theme);
    static FText DisplayName(EMiaIAEditorTheme Theme);
    static EMiaIAVisualizationPalettePreset LoadVisualizationPalettePreset();
    static void SaveVisualizationPalettePreset(
        EMiaIAVisualizationPalettePreset Preset);
    static FMiaIAVisualizationPalette LoadCustomVisualizationPalette(
        EMiaIAEditorTheme Theme);
    static void SaveCustomVisualizationPalette(
        const FMiaIAVisualizationPalette& Palette);
    static void SetVisualizationPalette(
        EMiaIAVisualizationPalettePreset Preset,
        const FMiaIAVisualizationPalette& CustomPalette);
    static FMiaIAVisualizationPalette VisualizationPalette(
        EMiaIAEditorTheme Theme);
    static FMiaIAVisualizationPalette VisualizationPaletteForPreset(
        EMiaIAVisualizationPalettePreset Preset,
        EMiaIAEditorTheme Theme,
        const FMiaIAVisualizationPalette& CustomPalette);
    static FText VisualizationPaletteDisplayName(
        EMiaIAVisualizationPalettePreset Preset);
    static FText VisualizationColorRoleDisplayName(
        EMiaIAVisualizationColorRole Role);
    static FButtonStyle ButtonStyle(EMiaIAEditorTheme Theme);
    static FButtonStyle ExplorerButtonStyle(EMiaIAEditorTheme Theme);
    static FComboButtonStyle ComboButtonStyle(EMiaIAEditorTheme Theme);
    static FEditableTextBoxStyle InputStyle(EMiaIAEditorTheme Theme);
    static FScrollBarStyle ScrollBarStyle(EMiaIAEditorTheme Theme);
    static FSplitterStyle SplitterStyle(EMiaIAEditorTheme Theme);
};
