#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateTypes.h"

enum class EMiaIAEditorTheme : uint8
{
    FollowUnreal,
    Dark,
    Light
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

class FMiaIAEditorTheme
{
public:
    static EMiaIAEditorTheme Load();
    static void Save(EMiaIAEditorTheme Theme);
    static FMiaIAEditorPalette Palette(EMiaIAEditorTheme Theme);
    static FText DisplayName(EMiaIAEditorTheme Theme);
    static FButtonStyle ButtonStyle(EMiaIAEditorTheme Theme);
    static FButtonStyle ExplorerButtonStyle(EMiaIAEditorTheme Theme);
    static FComboButtonStyle ComboButtonStyle(EMiaIAEditorTheme Theme);
    static FEditableTextBoxStyle InputStyle(EMiaIAEditorTheme Theme);
    static FScrollBarStyle ScrollBarStyle(EMiaIAEditorTheme Theme);
    static FSplitterStyle SplitterStyle(EMiaIAEditorTheme Theme);
};
