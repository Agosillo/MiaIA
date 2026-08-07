#include "Styling/MiaIAEditorTheme.h"

#include "Misc/ConfigCacheIni.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateNoResource.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/AppStyle.h"
#include "Styling/StyleColors.h"

#define LOCTEXT_NAMESPACE "MiaIAEditorTheme"

namespace
{
    constexpr TCHAR SettingsSection[] = TEXT("MiaIA.Editor");
    constexpr TCHAR ThemeKey[] = TEXT("Theme");

    FString ThemeName(EMiaIAEditorTheme Theme)
    {
        switch (Theme)
        {
        case EMiaIAEditorTheme::Dark:
            return TEXT("Dark");
        case EMiaIAEditorTheme::Light:
            return TEXT("Light");
        default:
            return TEXT("FollowUnreal");
        }
    }
}

EMiaIAEditorTheme FMiaIAEditorTheme::Load()
{
    FString value;
    GConfig->GetString(
        SettingsSection,
        ThemeKey,
        value,
        GEditorPerProjectIni);

    if (value.Equals(TEXT("Dark"), ESearchCase::IgnoreCase))
    {
        return EMiaIAEditorTheme::Dark;
    }

    if (value.Equals(TEXT("Light"), ESearchCase::IgnoreCase))
    {
        return EMiaIAEditorTheme::Light;
    }

    return EMiaIAEditorTheme::FollowUnreal;
}

void FMiaIAEditorTheme::Save(EMiaIAEditorTheme Theme)
{
    GConfig->SetString(
        SettingsSection,
        ThemeKey,
        *ThemeName(Theme),
        GEditorPerProjectIni);
    GConfig->Flush(false, GEditorPerProjectIni);
}

FMiaIAEditorPalette FMiaIAEditorTheme::Palette(
    EMiaIAEditorTheme Theme)
{
    if (Theme == EMiaIAEditorTheme::Dark)
    {
        return {
            FLinearColor(0.035f, 0.041f, 0.052f, 1.0f),
            FLinearColor(0.070f, 0.078f, 0.094f, 1.0f),
            FLinearColor(0.105f, 0.116f, 0.137f, 1.0f),
            FLinearColor(0.88f, 0.90f, 0.93f, 1.0f),
            FLinearColor(0.55f, 0.58f, 0.64f, 1.0f),
            FLinearColor(0.25f, 0.27f, 0.31f, 1.0f),
            FLinearColor(0.12f, 0.72f, 0.31f, 1.0f),
            FLinearColor(0.24f, 0.52f, 0.86f, 1.0f),
            FLinearColor(0.86f, 0.30f, 0.28f, 1.0f),
            FLinearColor(1.0f, 0.76f, 0.16f, 1.0f),
            FLinearColor(0.30f, 0.68f, 1.0f, 1.0f)
        };
    }

    if (Theme == EMiaIAEditorTheme::Light)
    {
        return {
            FLinearColor(0.88f, 0.90f, 0.93f, 1.0f),
            FLinearColor(0.95f, 0.96f, 0.98f, 1.0f),
            FLinearColor(1.0f, 1.0f, 1.0f, 1.0f),
            FLinearColor(0.075f, 0.085f, 0.105f, 1.0f),
            FLinearColor(0.34f, 0.37f, 0.42f, 1.0f),
            FLinearColor(0.64f, 0.67f, 0.72f, 1.0f),
            FLinearColor(0.05f, 0.58f, 0.24f, 1.0f),
            FLinearColor(0.08f, 0.38f, 0.76f, 1.0f),
            FLinearColor(0.76f, 0.15f, 0.13f, 1.0f),
            FLinearColor(0.92f, 0.54f, 0.02f, 1.0f),
            FLinearColor(0.02f, 0.40f, 0.78f, 1.0f)
        };
    }

    return {
        FStyleColors::Background.GetSpecifiedColor(),
        FStyleColors::Panel.GetSpecifiedColor(),
        FStyleColors::Recessed.GetSpecifiedColor(),
        FStyleColors::Foreground.GetSpecifiedColor(),
        FStyleColors::ForegroundHover.GetSpecifiedColor(),
        FStyleColors::Hover.GetSpecifiedColor(),
        FStyleColors::AccentGreen.GetSpecifiedColor(),
        FStyleColors::AccentBlue.GetSpecifiedColor(),
        FStyleColors::AccentRed.GetSpecifiedColor(),
        FStyleColors::AccentYellow.GetSpecifiedColor(),
        FStyleColors::Primary.GetSpecifiedColor()
    };
}

FText FMiaIAEditorTheme::DisplayName(EMiaIAEditorTheme Theme)
{
    switch (Theme)
    {
    case EMiaIAEditorTheme::Dark:
        return LOCTEXT("Dark", "Dark");
    case EMiaIAEditorTheme::Light:
        return LOCTEXT("Light", "Light");
    default:
        return LOCTEXT("FollowUnreal", "Follow Unreal");
    }
}

FButtonStyle FMiaIAEditorTheme::ButtonStyle(EMiaIAEditorTheme Theme)
{
    FButtonStyle style =
        FAppStyle::Get().GetWidgetStyle<FButtonStyle>(TEXT("Button"));

    if (Theme != EMiaIAEditorTheme::Light)
    {
        return style;
    }

    const FMiaIAEditorPalette palette = Palette(Theme);
    const FLinearColor outline =
        palette.SubduedText.CopyWithNewOpacity(0.45f);

    return style
        .SetNormal(FSlateRoundedBoxBrush(
            palette.Surface,
            4.0f,
            outline,
            1.0f))
        .SetHovered(FSlateRoundedBoxBrush(
            FLinearColor(0.90f, 0.92f, 0.95f, 1.0f),
            4.0f,
            palette.PositiveWeight,
            1.0f))
        .SetPressed(FSlateRoundedBoxBrush(
            FLinearColor(0.82f, 0.86f, 0.91f, 1.0f),
            4.0f,
            palette.PositiveWeight,
            1.0f))
        .SetDisabled(FSlateRoundedBoxBrush(
            FLinearColor(0.88f, 0.89f, 0.91f, 1.0f),
            4.0f,
            outline,
            1.0f))
        .SetNormalForeground(palette.Text)
        .SetHoveredForeground(palette.Text)
        .SetPressedForeground(palette.Text)
        .SetDisabledForeground(
            palette.SubduedText.CopyWithNewOpacity(0.65f));
}

FButtonStyle FMiaIAEditorTheme::ExplorerButtonStyle(
    EMiaIAEditorTheme Theme)
{
    FButtonStyle style =
        FAppStyle::Get().GetWidgetStyle<FButtonStyle>(
            TEXT("SimpleButton"));

    if (Theme != EMiaIAEditorTheme::Light)
    {
        return style;
    }

    const FMiaIAEditorPalette palette = Palette(Theme);
    return style
        .SetNormal(FSlateNoResource())
        .SetHovered(FSlateRoundedBoxBrush(
            FLinearColor(0.90f, 0.92f, 0.95f, 1.0f),
            4.0f))
        .SetPressed(FSlateRoundedBoxBrush(
            FLinearColor(0.82f, 0.86f, 0.91f, 1.0f),
            4.0f))
        .SetDisabled(FSlateNoResource())
        .SetNormalForeground(palette.Text)
        .SetHoveredForeground(palette.Text)
        .SetPressedForeground(palette.Text)
        .SetDisabledForeground(
            palette.SubduedText.CopyWithNewOpacity(0.65f));
}

FComboButtonStyle FMiaIAEditorTheme::ComboButtonStyle(
    EMiaIAEditorTheme Theme)
{
    FComboButtonStyle style =
        FAppStyle::Get().GetWidgetStyle<FComboButtonStyle>(
            TEXT("ComboButton"));

    if (Theme != EMiaIAEditorTheme::Light)
    {
        return style;
    }

    const FMiaIAEditorPalette palette = Palette(Theme);
    style.SetButtonStyle(ButtonStyle(Theme));
    style.SetMenuBorderBrush(FSlateRoundedBoxBrush(
        palette.Surface,
        3.0f,
        palette.SubduedText.CopyWithNewOpacity(0.45f),
        1.0f));
    style.DownArrowImage.TintColor = FSlateColor(palette.Text);
    return style;
}

FScrollBarStyle FMiaIAEditorTheme::ScrollBarStyle(
    EMiaIAEditorTheme Theme)
{
    FScrollBarStyle style =
        FAppStyle::Get().GetWidgetStyle<FScrollBarStyle>(
            TEXT("ScrollBar"));

    if (Theme != EMiaIAEditorTheme::Light)
    {
        return style;
    }

    const FMiaIAEditorPalette palette = Palette(Theme);
    const FSlateColorBrush track(palette.Panel);

    return style
        .SetHorizontalBackgroundImage(track)
        .SetVerticalBackgroundImage(track)
        .SetNormalThumbImage(FSlateRoundedBoxBrush(
            palette.SubduedText.CopyWithNewOpacity(0.55f),
            4.0f))
        .SetHoveredThumbImage(FSlateRoundedBoxBrush(
            palette.PositiveWeight.CopyWithNewOpacity(0.75f),
            4.0f))
        .SetDraggedThumbImage(FSlateRoundedBoxBrush(
            palette.PositiveWeight,
            4.0f));
}

FEditableTextBoxStyle FMiaIAEditorTheme::InputStyle(
    EMiaIAEditorTheme Theme)
{
    FEditableTextBoxStyle style =
        FAppStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>(
            TEXT("NormalEditableTextBox"));

    if (Theme != EMiaIAEditorTheme::Light)
    {
        return style;
    }

    const FMiaIAEditorPalette palette = Palette(Theme);
    const FLinearColor outline =
        palette.SubduedText.CopyWithNewOpacity(0.55f);
    FTextBlockStyle textStyle = style.TextStyle;
    textStyle.SetColorAndOpacity(palette.Text);

    return style
        .SetTextStyle(textStyle)
        .SetBackgroundImageNormal(FSlateRoundedBoxBrush(
            palette.Surface,
            4.0f,
            outline,
            1.0f))
        .SetBackgroundImageHovered(FSlateRoundedBoxBrush(
            palette.Surface,
            4.0f,
            palette.PositiveWeight.CopyWithNewOpacity(0.65f),
            1.0f))
        .SetBackgroundImageFocused(FSlateRoundedBoxBrush(
            palette.Surface,
            4.0f,
            palette.PositiveWeight,
            1.0f))
        .SetBackgroundImageReadOnly(FSlateRoundedBoxBrush(
            palette.Panel,
            4.0f,
            outline,
            1.0f))
        .SetForegroundColor(palette.Text)
        .SetFocusedForegroundColor(palette.Text)
        .SetReadOnlyForegroundColor(palette.SubduedText)
        .SetBackgroundColor(FLinearColor::White)
        .SetScrollBarStyle(ScrollBarStyle(Theme));
}

FSplitterStyle FMiaIAEditorTheme::SplitterStyle(
    EMiaIAEditorTheme Theme)
{
    FSplitterStyle style =
        FAppStyle::Get().GetWidgetStyle<FSplitterStyle>(TEXT("Splitter"));

    if (Theme != EMiaIAEditorTheme::Light)
    {
        return style;
    }

    const FMiaIAEditorPalette palette = Palette(Theme);
    return style
        .SetHandleNormalBrush(FSlateColorBrush(palette.Background))
        .SetHandleHighlightBrush(FSlateColorBrush(
            palette.PositiveWeight.CopyWithNewOpacity(0.70f)));
}

#undef LOCTEXT_NAMESPACE
