#include "Widgets/SMiaIAEditorPanel.h"

#include "MiaIACommandProcessor.h"
#include "MiaIABlueprintLibrary.h"
#include "StudioTopology.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformMisc.h"
#include "InputCoreTypes.h"
#include "Containers/UnrealString.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Text/SMultiLineEditableText.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBar.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SMiaIA3DNetworkView.h"
#include "Widgets/SMiaIANetworkView.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "MiaIAStudioPanel"

namespace
{
    constexpr TCHAR DataRefreshSettingsSection[] =
        TEXT("MiaIAStudio.UserSettings");
    constexpr TCHAR DataRefreshSettingsKey[] = TEXT("DataRefresh");
    constexpr TCHAR DetailedNeuronLimitSettingsKey[] =
        TEXT("DetailedNeuronLimit");
    constexpr TCHAR DetailedConnectionLimitSettingsKey[] =
        TEXT("DetailedConnectionLimit");
    constexpr TCHAR InspectorConnectionLimitSettingsKey[] =
        TEXT("InspectorConnectionLimit");
    constexpr TCHAR LayoutModeSettingsKey[] = TEXT("LayoutMode");
    constexpr TCHAR LayoutOrientationSettingsKey[] =
        TEXT("LayoutOrientation");
    constexpr TCHAR LayoutDirectionSettingsKey[] =
        TEXT("LayoutDirection");
    constexpr TCHAR ConnectionDisplaySettingsKey[] =
        TEXT("ConnectionDisplay");
    constexpr TCHAR NeuronScaleSettingsKey[] = TEXT("NeuronScale");
    constexpr TCHAR ConnectionScaleSettingsKey[] = TEXT("ConnectionScale");
    constexpr TCHAR ShowNeuronLabelsSettingsKey[] =
        TEXT("ShowNeuronLabels");
    constexpr TCHAR NeuronGapSettingsKey[] = TEXT("NeuronGap");
    constexpr TCHAR LayerGapSettingsKey[] = TEXT("LayerGap");
    constexpr int32 MinimumTopologyLimit = 1;
    constexpr int32 MaximumDetailedNeuronLimit = 100000000;
    constexpr int32 MaximumDetailedConnectionLimit = 1000000000;
    constexpr int32 MaximumNeuronSliderLimit = 100000;
    constexpr int32 MaximumConnectionSliderLimit = 1000000;
    constexpr int32 DefaultInspectorConnectionLimit = 5;
    constexpr int32 MaximumInspectorConnectionLimit = 1000;
    constexpr int32 MaximumInspectorConnectionSliderLimit = 50;
    constexpr double DefaultForwardTraceFrameDurationSeconds = 0.65;

    FString DataRefreshModeName(EMiaIADataRefreshMode Mode)
    {
        switch (Mode)
        {
        case EMiaIADataRefreshMode::OneHz:
            return TEXT("1Hz");
        case EMiaIADataRefreshMode::TwoHz:
            return TEXT("2Hz");
        case EMiaIADataRefreshMode::FourHz:
            return TEXT("4Hz");
        case EMiaIADataRefreshMode::TenHz:
            return TEXT("10Hz");
        case EMiaIADataRefreshMode::Adaptive:
        default:
            return TEXT("Adaptive");
        }
    }

    EMiaIADataRefreshMode LoadDataRefreshMode()
    {
        FString savedMode;

        if (GConfig && GConfig->GetString(
            DataRefreshSettingsSection,
            DataRefreshSettingsKey,
            savedMode,
            GGameUserSettingsIni))
        {
            if (savedMode.Equals(TEXT("1Hz"), ESearchCase::IgnoreCase))
            {
                return EMiaIADataRefreshMode::OneHz;
            }

            if (savedMode.Equals(TEXT("2Hz"), ESearchCase::IgnoreCase))
            {
                return EMiaIADataRefreshMode::TwoHz;
            }

            if (savedMode.Equals(TEXT("4Hz"), ESearchCase::IgnoreCase))
            {
                return EMiaIADataRefreshMode::FourHz;
            }

            if (savedMode.Equals(TEXT("10Hz"), ESearchCase::IgnoreCase))
            {
                return EMiaIADataRefreshMode::TenHz;
            }
        }

        return EMiaIADataRefreshMode::Adaptive;
    }

    void SaveDataRefreshMode(EMiaIADataRefreshMode Mode)
    {
        if (!GConfig)
        {
            return;
        }

        GConfig->SetString(
            DataRefreshSettingsSection,
            DataRefreshSettingsKey,
            *DataRefreshModeName(Mode),
            GGameUserSettingsIni);
        GConfig->Flush(false, GGameUserSettingsIni);
    }

    int32 LoadTopologyLimit(
        const TCHAR* Key,
        int32 DefaultValue,
        int32 MaximumValue)
    {
        int32 savedValue{};

        if (GConfig && GConfig->GetInt(
            DataRefreshSettingsSection,
            Key,
            savedValue,
            GGameUserSettingsIni))
        {
            return FMath::Clamp(
                savedValue,
                MinimumTopologyLimit,
                MaximumValue);
        }

        return DefaultValue;
    }

    void SaveTopologyLimits(
        int32 NeuronLimit,
        int32 ConnectionLimit,
        int32 InspectorLimit)
    {
        if (!GConfig)
        {
            return;
        }

        GConfig->SetInt(
            DataRefreshSettingsSection,
            DetailedNeuronLimitSettingsKey,
            NeuronLimit,
            GGameUserSettingsIni);
        GConfig->SetInt(
            DataRefreshSettingsSection,
            DetailedConnectionLimitSettingsKey,
            ConnectionLimit,
            GGameUserSettingsIni);
        GConfig->SetInt(
            DataRefreshSettingsSection,
            InspectorConnectionLimitSettingsKey,
            InspectorLimit,
            GGameUserSettingsIni);
        GConfig->Flush(false, GGameUserSettingsIni);
    }

    FMiaIAVisualizationSettings LoadVisualizationSettings()
    {
        FMiaIAVisualizationSettings settings;

        if (!GConfig)
        {
            return settings;
        }

        FString savedValue;

        if (GConfig->GetString(
            DataRefreshSettingsSection,
            LayoutModeSettingsKey,
            savedValue,
            GGameUserSettingsIni) &&
            savedValue.Equals(TEXT("Packed"), ESearchCase::IgnoreCase))
        {
            settings.Layout.Mode =
                MiaIA::Studio::StudioLayoutMode::Packed;
        }

        if (GConfig->GetString(
            DataRefreshSettingsSection,
            LayoutOrientationSettingsKey,
            savedValue,
            GGameUserSettingsIni) &&
            savedValue.Equals(TEXT("Vertical"), ESearchCase::IgnoreCase))
        {
            settings.Layout.Orientation =
                MiaIA::Studio::StudioLayoutOrientation::Vertical;
        }

        if (GConfig->GetString(
            DataRefreshSettingsSection,
            LayoutDirectionSettingsKey,
            savedValue,
            GGameUserSettingsIni) &&
            savedValue.Equals(TEXT("Reverse"), ESearchCase::IgnoreCase))
        {
            settings.Layout.Direction =
                MiaIA::Studio::StudioLayoutDirection::Reverse;
        }

        if (GConfig->GetString(
            DataRefreshSettingsSection,
            ConnectionDisplaySettingsKey,
            savedValue,
            GGameUserSettingsIni))
        {
            if (savedValue.Equals(
                TEXT("Selected"),
                ESearchCase::IgnoreCase))
            {
                settings.ConnectionDisplay =
                    EMiaIAConnectionDisplayMode::Selected;
            }
        }

        GConfig->GetFloat(
            DataRefreshSettingsSection,
            NeuronScaleSettingsKey,
            settings.NeuronScale,
            GGameUserSettingsIni);
        GConfig->GetFloat(
            DataRefreshSettingsSection,
            ConnectionScaleSettingsKey,
            settings.ConnectionScale,
            GGameUserSettingsIni);
        GConfig->GetDouble(
            DataRefreshSettingsSection,
            NeuronGapSettingsKey,
            settings.Layout.NeuronGap,
            GGameUserSettingsIni);
        GConfig->GetDouble(
            DataRefreshSettingsSection,
            LayerGapSettingsKey,
            settings.Layout.LayerGap,
            GGameUserSettingsIni);
        GConfig->GetBool(
            DataRefreshSettingsSection,
            ShowNeuronLabelsSettingsKey,
            settings.bShowNeuronLabels,
            GGameUserSettingsIni);
        settings.NeuronScale = FMath::Clamp(
            settings.NeuronScale,
            0.25f,
            3.0f);
        settings.ConnectionScale = FMath::Clamp(
            settings.ConnectionScale,
            0.0f,
            2.0f);
        settings.Layout.NeuronGap = FMath::Clamp(
            settings.Layout.NeuronGap,
            0.0,
            5.0);
        settings.Layout.LayerGap = FMath::Clamp(
            settings.Layout.LayerGap,
            0.0,
            10.0);
        return settings;
    }

    void SaveVisualizationSettings(
        const FMiaIAVisualizationSettings& Settings)
    {
        if (!GConfig)
        {
            return;
        }

        const TCHAR* layoutMode = Settings.Layout.Mode ==
            MiaIA::Studio::StudioLayoutMode::Packed
            ? TEXT("Packed")
            : TEXT("Expanded");
        const TCHAR* layoutOrientation = Settings.Layout.Orientation ==
            MiaIA::Studio::StudioLayoutOrientation::Vertical
            ? TEXT("Vertical")
            : TEXT("Horizontal");
        const TCHAR* layoutDirection = Settings.Layout.Direction ==
            MiaIA::Studio::StudioLayoutDirection::Reverse
            ? TEXT("Reverse")
            : TEXT("Forward");
        const TCHAR* connectionDisplay = TEXT("All");

        if (Settings.ConnectionDisplay ==
            EMiaIAConnectionDisplayMode::Selected)
        {
            connectionDisplay = TEXT("Selected");
        }

        GConfig->SetString(
            DataRefreshSettingsSection,
            LayoutModeSettingsKey,
            layoutMode,
            GGameUserSettingsIni);
        GConfig->SetString(
            DataRefreshSettingsSection,
            LayoutOrientationSettingsKey,
            layoutOrientation,
            GGameUserSettingsIni);
        GConfig->SetString(
            DataRefreshSettingsSection,
            LayoutDirectionSettingsKey,
            layoutDirection,
            GGameUserSettingsIni);
        GConfig->SetString(
            DataRefreshSettingsSection,
            ConnectionDisplaySettingsKey,
            connectionDisplay,
            GGameUserSettingsIni);
        GConfig->SetFloat(
            DataRefreshSettingsSection,
            NeuronScaleSettingsKey,
            Settings.NeuronScale,
            GGameUserSettingsIni);
        GConfig->SetFloat(
            DataRefreshSettingsSection,
            ConnectionScaleSettingsKey,
            Settings.ConnectionScale,
            GGameUserSettingsIni);
        GConfig->SetDouble(
            DataRefreshSettingsSection,
            NeuronGapSettingsKey,
            Settings.Layout.NeuronGap,
            GGameUserSettingsIni);
        GConfig->SetDouble(
            DataRefreshSettingsSection,
            LayerGapSettingsKey,
            Settings.Layout.LayerGap,
            GGameUserSettingsIni);
        GConfig->SetBool(
            DataRefreshSettingsSection,
            ShowNeuronLabelsSettingsKey,
            Settings.bShowNeuronLabels,
            GGameUserSettingsIni);
        GConfig->Flush(false, GGameUserSettingsIni);
    }

    FText DataRefreshModeDisplayName(EMiaIADataRefreshMode Mode)
    {
        switch (Mode)
        {
        case EMiaIADataRefreshMode::OneHz:
            return LOCTEXT("DataRefreshOneHz", "1 Hz");
        case EMiaIADataRefreshMode::TwoHz:
            return LOCTEXT("DataRefreshTwoHz", "2 Hz");
        case EMiaIADataRefreshMode::FourHz:
            return LOCTEXT("DataRefreshFourHz", "4 Hz");
        case EMiaIADataRefreshMode::TenHz:
            return LOCTEXT("DataRefreshTenHz", "10 Hz");
        case EMiaIADataRefreshMode::Adaptive:
        default:
            return LOCTEXT("DataRefreshAdaptive", "Adaptive");
        }
    }

    FText SessionStatusName(EMiaIATrainingSessionStatus Status)
    {
        switch (Status)
        {
        case EMiaIATrainingSessionStatus::Idle:
            return LOCTEXT("SessionIdle", "Idle");
        case EMiaIATrainingSessionStatus::Active:
            return LOCTEXT("SessionActive", "Active");
        case EMiaIATrainingSessionStatus::Running:
            return LOCTEXT("SessionRunning", "Running");
        case EMiaIATrainingSessionStatus::Completed:
            return LOCTEXT("SessionCompleted", "Completed");
        case EMiaIATrainingSessionStatus::Cancelled:
            return LOCTEXT("SessionCancelled", "Cancelled");
        }

        return LOCTEXT("SessionUnknown", "Unknown");
    }

    FText DebugPhaseName(EMiaIATrainingDebugPhase Phase)
    {
        switch (Phase)
        {
        case EMiaIATrainingDebugPhase::Idle:
            return LOCTEXT("DebugIdle", "Idle");
        case EMiaIATrainingDebugPhase::BeforeForward:
            return LOCTEXT("DebugBeforeForward", "Before forward");
        case EMiaIATrainingDebugPhase::ForwardComplete:
            return LOCTEXT("DebugForward", "Forward complete");
        case EMiaIATrainingDebugPhase::BackwardComplete:
            return LOCTEXT("DebugBackward", "Backward complete");
        case EMiaIATrainingDebugPhase::UpdateComplete:
            return LOCTEXT("DebugUpdate", "Update complete");
        case EMiaIATrainingDebugPhase::Verified:
            return LOCTEXT("DebugVerified", "Verified");
        case EMiaIATrainingDebugPhase::Committed:
            return LOCTEXT("DebugCommitted", "Committed");
        }

        return LOCTEXT("DebugUnknown", "Unknown");
    }

    FString ActivationName(EMiaIAActivationType Activation)
    {
        switch (Activation)
        {
        case EMiaIAActivationType::Sigmoid:
            return TEXT("Sigmoid");
        case EMiaIAActivationType::ReLU:
            return TEXT("ReLU");
        case EMiaIAActivationType::Tanh:
            return TEXT("Tanh");
        case EMiaIAActivationType::Linear:
            return TEXT("Linear");
        }

        return TEXT("Unknown");
    }

    FText BreakpointKindName(EMiaIATrainingBreakpointKind Kind)
    {
        switch (Kind)
        {
        case EMiaIATrainingBreakpointKind::Phase:
            return LOCTEXT("BreakpointPhase", "Phase reached");
        case EMiaIATrainingBreakpointKind::NeuronActivationAbove:
            return LOCTEXT("BreakpointActivationAbove", "Activation above");
        case EMiaIATrainingBreakpointKind::NeuronActivationBelow:
            return LOCTEXT("BreakpointActivationBelow", "Activation below");
        case EMiaIATrainingBreakpointKind::NeuronGradientMagnitudeAbove:
            return LOCTEXT("BreakpointGradientAbove", "Gradient above");
        case EMiaIATrainingBreakpointKind::ConnectionUpdateMagnitudeAbove:
            return LOCTEXT("BreakpointUpdateAbove", "Weight update above");
        }

        return LOCTEXT("BreakpointUnknown", "Unknown");
    }

    FString BuildBreakpointKey(
        const TArray<FMiaIATrainingBreakpoint>& Breakpoints,
        const FMiaIATrainingSessionSnapshot& Session)
    {
        FString key = FString::Printf(
            TEXT("%d:%d:%lld"),
            Breakpoints.Num(),
            Session.bHasBreakpointHit ? 1 : 0,
            Session.LastBreakpointHit.BreakpointId);

        for (const FMiaIATrainingBreakpoint& breakpoint : Breakpoints)
        {
            key += FString::Printf(
                TEXT("|%lld:%d:%d:%d:%lld:%g:%lld"),
                breakpoint.Id,
                breakpoint.bEnabled ? 1 : 0,
                static_cast<int32>(breakpoint.Kind),
                static_cast<int32>(breakpoint.Phase),
                breakpoint.TargetId,
                breakpoint.Threshold,
                breakpoint.HitCount);
        }

        return key;
    }

    FString BuildTopologyKey(const FMiaIANetworkSnapshot& Network)
    {
        FString key = FString::Printf(
            TEXT("L%dC%d"),
            Network.Layers.Num(),
            Network.Connections.Num());

        for (const FMiaIALayerSnapshot& layer : Network.Layers)
        {
            key += FString::Printf(TEXT("|%lld:"), layer.Id);

            for (const FMiaIANeuronSnapshot& neuron : layer.Neurons)
            {
                key += FString::Printf(TEXT("%lld,"), neuron.Id);
            }
        }

        for (const FMiaIAConnectionSnapshot& connection :
            Network.Connections)
        {
            key += FString::Printf(
                TEXT("|C%lld:%lld>%lld"),
                connection.Id,
                connection.FromNeuron,
                connection.ToNeuron);
        }

        return key;
    }

    FString BuildOverviewKey(const FMiaIANetworkOverview& Overview)
    {
        FString key = FString::Printf(
            TEXT("Compact:L%dN%lldC%lld"),
            Overview.Layers.Num(),
            Overview.NeuronCount,
            Overview.ConnectionCount);

        for (const FMiaIALayerOverview& layer : Overview.Layers)
        {
            key += FString::Printf(
                TEXT("|%lld:%lld:%lld"),
                layer.Id,
                layer.Order,
                layer.NeuronCount);
        }

        return key;
    }
}

void SMiaIAEditorPanel::Construct(const FArguments& InArgs)
{
    const auto panelBorder = FAppStyle::GetBrush(TEXT("WhiteBrush"));
    bStandaloneMode = InArgs._StandaloneMode;
    MiaIAInstance = FMiaIAInstanceService::DefaultInstance();
    Theme = FMiaIAEditorTheme::Load();
    DataRefreshMode = LoadDataRefreshMode();
    VisualizationSettings = LoadVisualizationSettings();
    DetailedNeuronLimit = LoadTopologyLimit(
        DetailedNeuronLimitSettingsKey,
        static_cast<int32>(
            MiaIA::Studio::StudioTopologyBuilder::DetailedNeuronLimit),
        MaximumDetailedNeuronLimit);
    DetailedConnectionLimit = LoadTopologyLimit(
        DetailedConnectionLimitSettingsKey,
        static_cast<int32>(
            MiaIA::Studio::StudioTopologyBuilder::
                DetailedConnectionLimit),
        MaximumDetailedConnectionLimit);
    InspectorConnectionLimit = LoadTopologyLimit(
        InspectorConnectionLimitSettingsKey,
        DefaultInspectorConnectionLimit,
        MaximumInspectorConnectionLimit);
    RefreshWidgetStyles();
    ConsoleHistory = TEXT(
        "MiaIA Studio Console\n"
        "Type 'help' to list the shared CLI commands. "
        "Use Up/Down for history and Tab for completion.\n");
    ConsoleHistoryIndex = 0;
    SAssignNew(ConsoleOutputScrollBar, SScrollBar)
        .Style(&ScrollBarStyle)
        .Orientation(Orient_Vertical)
        .AlwaysShowScrollbar(true)
        .AlwaysShowScrollbarTrack(true)
        .HideWhenNotInUse(false)
        .Thickness(FVector2D(12.0f, 12.0f))
        .Padding(FMargin(1.0f));

    ChildSlot
    [
        SNew(SOverlay)
        + SOverlay::Slot()
        [
        SNew(SBorder)
        .BorderImage(panelBorder)
        .BorderBackgroundColor(this, &SMiaIAEditorPanel::BackgroundColor)
        .ForegroundColor(this, &SMiaIAEditorPanel::TextColor)
        .Padding(0.0f)
        [
        SNew(SVerticalBox)
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SBorder)
            .BorderImage(panelBorder)
            .BorderBackgroundColor(this, &SMiaIAEditorPanel::PanelColor)
            .Padding(FMargin(6.0f, 4.0f))
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot()
                .AutoHeight()
                [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(2.0f, 0.0f, 12.0f, 0.0f)
                [
                    SNew(SComboButton)
                    .ComboButtonStyle(&ComboButtonStyle)
                    .ToolTipText(LOCTEXT(
                        "ProjectMenuTooltip",
                        "Create, open, save, or inspect a versioned .mai project."))
                    .ButtonContent()
                    [
                        SNew(STextBlock)
                        .Text(LOCTEXT("ProjectMenuLabel", "Project"))
                    ]
                    .OnGetMenuContent(
                        this,
                        &SMiaIAEditorPanel::BuildProjectMenu)
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(2.0f, 0.0f, 8.0f, 0.0f)
                [
                    SNew(SComboButton)
                    .ComboButtonStyle(&ComboButtonStyle)
                    .ToolTipText(LOCTEXT(
                        "LayoutMenuTooltip",
                        "Choose expanded or packed placement, neuron size, spacing, and connection detail."))
                    .ButtonContent()
                    [
                        SNew(STextBlock)
                        .Text(this, &SMiaIAEditorPanel::LayoutModeText)
                    ]
                    .OnGetMenuContent(
                        this,
                        &SMiaIAEditorPanel::BuildLayoutMenu)
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(2.0f)
                [
                    SNew(SButton)
                    .ButtonStyle(&ButtonStyle)
                    .Text(LOCTEXT("Refresh", "Refresh"))
                    .OnClicked(this, &SMiaIAEditorPanel::HandleRefresh)
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(2.0f)
                [
                    SNew(SButton)
                    .ButtonStyle(&ButtonStyle)
                    .Text(LOCTEXT("FitView", "Fit view"))
                    .ToolTipText(LOCTEXT(
                        "FitViewTooltip",
                        "Fit every neuron in the current topology view."))
                    .OnClicked(this, &SMiaIAEditorPanel::HandleFitView)
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(2.0f)
                [
                    SNew(SButton)
                    .ButtonStyle(&ButtonStyle)
                    .Text(LOCTEXT("ResetLayout", "Reset layout"))
                    .ToolTipText(LOCTEXT(
                        "ResetLayoutTooltip",
                        "Restore automatic neuron positions and the default camera framing."))
                    .OnClicked(this, &SMiaIAEditorPanel::HandleResetLayout)
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(2.0f)
                [
                    SNew(SButton)
                    .ButtonStyle(&ButtonStyle)
                    .Text(this, &SMiaIAEditorPanel::TopologyWorkspaceText)
                    .ToolTipText(LOCTEXT(
                        "TopologyWorkspaceTooltip",
                        "Expand the topology canvas or restore the surrounding panels."))
                    .OnClicked(
                        this,
                        &SMiaIAEditorPanel::HandleToggleTopologyWorkspace)
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(10.0f, 0.0f, 2.0f, 0.0f)
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("ViewLabel", "View"))
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(2.0f)
                [
                    SNew(SComboButton)
                    .ComboButtonStyle(&ComboButtonStyle)
                    .ToolTipText(LOCTEXT(
                        "ViewModeTooltip",
                        "Switch between the editable 2D canvas and the 3D orbit view."))
                    .ButtonContent()
                    [
                        SNew(STextBlock)
                        .Text(this, &SMiaIAEditorPanel::ViewModeText)
                    ]
                    .OnGetMenuContent(
                        this,
                        &SMiaIAEditorPanel::BuildViewModeMenu)
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(10.0f, 0.0f, 2.0f, 0.0f)
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("ThemeLabel", "Theme"))
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(2.0f)
                [
                    SNew(SComboButton)
                    .ComboButtonStyle(&ComboButtonStyle)
                    .ButtonContent()
                    [
                        SNew(STextBlock)
                        .Text(this, &SMiaIAEditorPanel::ThemeText)
                    ]
                    .OnGetMenuContent(
                        this,
                        &SMiaIAEditorPanel::BuildThemeMenu)
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(10.0f, 0.0f, 2.0f, 0.0f)
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("DataRefreshLabel", "Data refresh"))
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(2.0f)
                [
                    SNew(SComboButton)
                    .ComboButtonStyle(&ComboButtonStyle)
                    .ToolTipText(LOCTEXT(
                        "DataRefreshTooltip",
                        "Control automatic model-data polling. Commands and debug controls always refresh immediately."))
                    .ButtonContent()
                    [
                        SNew(STextBlock)
                        .Text(this, &SMiaIAEditorPanel::DataRefreshText)
                    ]
                    .OnGetMenuContent(
                        this,
                        &SMiaIAEditorPanel::BuildDataRefreshMenu)
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(10.0f, 0.0f, 2.0f, 0.0f)
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("TopologyLimitsLabel", "Detail limits"))
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(2.0f)
                [
                    SNew(SComboButton)
                    .ComboButtonStyle(&ComboButtonStyle)
                    .ToolTipText(LOCTEXT(
                        "TopologyLimitsTooltip",
                        "Set the largest neuron and connection counts rendered in detailed mode. Larger networks use compact mode."))
                    .ButtonContent()
                    [
                        SNew(STextBlock)
                        .Text(this, &SMiaIAEditorPanel::TopologyLimitsText)
                    ]
                    .OnGetMenuContent(
                        this,
                        &SMiaIAEditorPanel::BuildTopologyLimitsMenu)
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(10.0f, 0.0f, 2.0f, 0.0f)
                [
                    SNew(SComboButton)
                    .ComboButtonStyle(&ComboButtonStyle)
                    .ToolTipText(LOCTEXT(
                        "HelpMenuTooltip",
                        "Open interaction help or MiaIA Studio information."))
                    .ButtonContent()
                    [
                        SNew(STextBlock)
                        .Text(LOCTEXT("HelpMenuLabel", "Help"))
                    ]
                    .OnGetMenuContent(
                        this,
                        &SMiaIAEditorPanel::BuildHelpMenu)
                ]
                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                [
                    SNullWidget::NullWidget
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(8.0f, 0.0f, 2.0f, 0.0f)
                [
                    SNew(SButton)
                    .ButtonStyle(&ButtonStyle)
                    .Visibility_Lambda([this]()
                    {
                        return bStandaloneMode
                            ? EVisibility::Visible
                            : EVisibility::Collapsed;
                    })
                    .Text(LOCTEXT("Exit", "Exit"))
                    .ToolTipText(LOCTEXT(
                        "ExitTooltip",
                        "Close MiaIA Studio."))
                    .OnClicked(this, &SMiaIAEditorPanel::HandleExit)
                ]
                ]
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0.0f, 2.0f, 0.0f, 0.0f)
                [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(2.0f, 0.0f, 8.0f, 0.0f)
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT(
                        "TrainingDebugToolbarGroup",
                        "Training debug"))
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(2.0f)
                [
                    SNew(SButton)
                    .ButtonStyle(&ButtonStyle)
                    .Text(LOCTEXT("Resume", "Continue"))
                    .IsEnabled(this, &SMiaIAEditorPanel::CanResume)
                    .OnClicked(this, &SMiaIAEditorPanel::HandleResume)
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(2.0f)
                [
                    SNew(SButton)
                    .ButtonStyle(&ButtonStyle)
                    .Text(LOCTEXT("Pause", "Pause"))
                    .IsEnabled(this, &SMiaIAEditorPanel::CanPause)
                    .OnClicked(this, &SMiaIAEditorPanel::HandlePause)
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(2.0f)
                [
                    SNew(SButton)
                    .ButtonStyle(&ButtonStyle)
                    .Text(LOCTEXT("StartDebug", "Start debug"))
                    .IsEnabled(this, &SMiaIAEditorPanel::CanStartDebug)
                    .OnClicked(this, &SMiaIAEditorPanel::HandleStartDebug)
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(2.0f)
                [
                    SNew(SButton)
                    .ButtonStyle(&ButtonStyle)
                    .Text(LOCTEXT("Advance", "Step phase"))
                    .IsEnabled(this, &SMiaIAEditorPanel::CanAdvanceDebug)
                    .OnClicked(this, &SMiaIAEditorPanel::HandleAdvanceDebug)
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(2.0f)
                [
                    SNew(SButton)
                    .ButtonStyle(&ButtonStyle)
                    .Text(LOCTEXT("CancelDebug", "Cancel debug"))
                    .IsEnabled(this, &SMiaIAEditorPanel::CanCancelDebug)
                    .OnClicked(this, &SMiaIAEditorPanel::HandleCancelDebug)
                ]
                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                [
                    SNullWidget::NullWidget
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(8.0f, 0.0f)
                [
                    SNew(STextBlock)
                    .Text(this, &SMiaIAEditorPanel::SessionStatusText)
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(8.0f, 0.0f)
                [
                    SNew(STextBlock)
                    .Text(this, &SMiaIAEditorPanel::DebugPhaseText)
                    .ColorAndOpacity_Lambda([this]()
                    {
                        return FSlateColor(
                            FMiaIAEditorTheme::Palette(Theme).Debug);
                    })
                ]
                ]
            ]
        ]
        + SVerticalBox::Slot()
        .FillHeight(1.0f)
        [
            SNew(SSplitter)
            .Style(&SplitterStyle)
            .Orientation(Orient_Vertical)
            + SSplitter::Slot()
            .Value(0.72f)
            [
                SNew(SSplitter)
                .Style(&SplitterStyle)
                .Orientation(Orient_Horizontal)
                + SSplitter::Slot()
                .Value(0.20f)
                [
                    SNew(SBorder)
                    .Visibility_Lambda([this]()
                    {
                        return bTopologyWorkspaceExpanded
                            ? EVisibility::Collapsed
                            : EVisibility::Visible;
                    })
                    .BorderImage(panelBorder)
                    .BorderBackgroundColor(this, &SMiaIAEditorPanel::PanelColor)
                    .Padding(6.0f)
                    [
                        SNew(SScrollBox)
                        .ScrollBarStyle(&ScrollBarStyle)
                        + SScrollBox::Slot()
                        [
                            SAssignNew(ExplorerContent, SVerticalBox)
                        ]
                    ]
                ]
                + SSplitter::Slot()
                .Value(0.58f)
                [
                    SNew(SBorder)
                    .BorderImage(panelBorder)
                    .BorderBackgroundColor(this, &SMiaIAEditorPanel::PanelColor)
                    .Padding(2.0f)
                    [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(8.0f, 5.0f)
                        [
                            SNew(SHorizontalBox)
                            + SHorizontalBox::Slot()
                            .AutoWidth()
                            .Padding(0.0f, 0.0f, 8.0f, 0.0f)
                            [
                                SNew(SButton)
                                .Visibility(
                                    this,
                                    &SMiaIAEditorPanel::LayerDetailVisibility)
                                .ButtonStyle(&ButtonStyle)
                                .Text(LOCTEXT("BackFromLayerDetail", "Back"))
                                .ToolTipText(LOCTEXT(
                                    "BackFromLayerDetailTooltip",
                                    "Return one level in the network overview. Esc provides the same action."))
                                .OnClicked(
                                    this,
                                    &SMiaIAEditorPanel::
                                        HandleNavigateBackFromTopology)
                            ]
                            + SHorizontalBox::Slot()
                            .FillWidth(1.0f)
                            .VAlign(VAlign_Center)
                            [
                                SNew(STextBlock)
                                .Text(
                                    this,
                                    &SMiaIAEditorPanel::NetworkSummaryText)
                                .Font(FAppStyle::GetFontStyle(
                                    TEXT("NormalFontBold")))
                            ]
                        ]
                        + SVerticalBox::Slot()
                        .FillHeight(1.0f)
                        [
                            SAssignNew(
                                TopologySwitcher,
                                SWidgetSwitcher)
                            + SWidgetSwitcher::Slot()
                            [
                                SAssignNew(NetworkView, SMiaIANetworkView)
                                .OnNeuronSelectionChanged(
                                    this,
                                    &SMiaIAEditorPanel::SelectNeurons)
                                .OnConnectionSelected(
                                    this,
                                    &SMiaIAEditorPanel::SelectConnection)
                                .OnLayerSelected(
                                    this,
                                    &SMiaIAEditorPanel::SelectLayer)
                                .OnNeuronNavigationRequested(
                                    this,
                                    &SMiaIAEditorPanel::NavigateNeuron)
                                .OnLayerOpenRequested(
                                    this,
                                    &SMiaIAEditorPanel::OpenLayerDetail)
                                .OnNetworkOpenRequested(
                                    this,
                                    &SMiaIAEditorPanel::
                                        OpenNetworkFromPreview)
                                .OnLayerFocusExitRequested(
                                    this,
                                    &SMiaIAEditorPanel::
                                        NavigateBackFromTopology)
                            ]
                            + SWidgetSwitcher::Slot()
                            [
                                SAssignNew(
                                    Network3DView,
                                    SMiaIA3DNetworkView)
                                .OnNeuronSelectionChanged(
                                    this,
                                    &SMiaIAEditorPanel::SelectNeurons)
                                .OnConnectionSelected(
                                    this,
                                    &SMiaIAEditorPanel::SelectConnection)
                                .OnLayerSelected(
                                    this,
                                    &SMiaIAEditorPanel::SelectLayer)
                                .OnNeuronNavigationRequested(
                                    this,
                                    &SMiaIAEditorPanel::NavigateNeuron)
                                .OnLayerOpenRequested(
                                    this,
                                    &SMiaIAEditorPanel::OpenLayerDetail)
                                .OnNetworkOpenRequested(
                                    this,
                                    &SMiaIAEditorPanel::
                                        OpenNetworkFromPreview)
                                .OnLayerFocusExitRequested(
                                    this,
                                    &SMiaIAEditorPanel::
                                        NavigateBackFromTopology)
                            ]
                        ]
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(8.0f, 4.0f, 8.0f, 6.0f)
                        [
                            SNew(SHorizontalBox)
                            .Visibility_Lambda([this]()
                            {
                                return bCompactTopology
                                    ? EVisibility::Collapsed
                                    : EVisibility::Visible;
                            })
                            + SHorizontalBox::Slot()
                            .AutoWidth()
                            .Padding(0.0f, 0.0f, 14.0f, 0.0f)
                            [
                                SNew(STextBlock)
                                .Text(LOCTEXT("InactiveLegend", "Inactive neuron"))
                                .ColorAndOpacity_Lambda([this]()
                                {
                                    return FSlateColor(
                                        FMiaIAEditorTheme::Palette(Theme)
                                            .InactiveNeuron);
                                })
                            ]
                            + SHorizontalBox::Slot()
                            .AutoWidth()
                            .Padding(0.0f, 0.0f, 14.0f, 0.0f)
                            [
                                SNew(STextBlock)
                                .Text(LOCTEXT("ActiveLegend", "Active neuron"))
                                .ColorAndOpacity_Lambda([this]()
                                {
                                    return FSlateColor(
                                        FMiaIAEditorTheme::Palette(Theme)
                                            .ActiveNeuron);
                                })
                            ]
                            + SHorizontalBox::Slot()
                            .AutoWidth()
                            .Padding(0.0f, 0.0f, 14.0f, 0.0f)
                            [
                                SNew(STextBlock)
                                .Text(this, &SMiaIAEditorPanel::PositiveMetricLegendText)
                                .ColorAndOpacity_Lambda([this]()
                                {
                                    return FSlateColor(
                                        FMiaIAEditorTheme::Palette(Theme)
                                            .PositiveWeight);
                                })
                            ]
                            + SHorizontalBox::Slot()
                            .AutoWidth()
                            .Padding(0.0f, 0.0f, 14.0f, 0.0f)
                            [
                                SNew(STextBlock)
                                .Text(this, &SMiaIAEditorPanel::NegativeMetricLegendText)
                                .ColorAndOpacity_Lambda([this]()
                                {
                                    return FSlateColor(
                                        FMiaIAEditorTheme::Palette(Theme)
                                            .NegativeWeight);
                                })
                            ]
                            + SHorizontalBox::Slot()
                            .AutoWidth()
                            [
                                SNew(STextBlock)
                                .Text(LOCTEXT("SelectedLegend", "Selected"))
                                .ColorAndOpacity_Lambda([this]()
                                {
                                    return FSlateColor(
                                        FMiaIAEditorTheme::Palette(Theme)
                                            .Selection);
                                })
                            ]
                        ]
                    ]
                ]
                + SSplitter::Slot()
                .Value(0.22f)
                [
                    SNew(SBorder)
                    .BorderImage(panelBorder)
                    .BorderBackgroundColor(this, &SMiaIAEditorPanel::PanelColor)
                    .Padding(10.0f)
                    [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(0.0f, 0.0f, 0.0f, 12.0f)
                        [
                            SNew(STextBlock)
                            .Text(LOCTEXT("Inspector", "Inspector"))
                            .Font(FAppStyle::GetFontStyle(TEXT("NormalFontBold")))
                        ]
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(0.0f, 3.0f)
                        [
                            SNew(STextBlock)
                            .Text(this, &SMiaIAEditorPanel::SelectionTitle)
                        ]
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(0.0f, 3.0f)
                        [
                            SNew(STextBlock)
                            .Text(this, &SMiaIAEditorPanel::SelectionContextText)
                        ]
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(0.0f, 3.0f)
                        [
                            SNew(STextBlock)
                            .Text(this, &SMiaIAEditorPanel::SelectionPrimaryText)
                        ]
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(0.0f, 3.0f)
                        [
                            SNew(STextBlock)
                            .Text(this, &SMiaIAEditorPanel::SelectionSecondaryText)
                        ]
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(0.0f, 5.0f)
                        [
                            SNew(STextBlock)
                            .Text(
                                this,
                                &SMiaIAEditorPanel::ForwardTraceSelectionText)
                            .AutoWrapText(true)
                            .ColorAndOpacity_Lambda([this]()
                            {
                                return FSlateColor(
                                    FMiaIAEditorTheme::Palette(Theme).Debug);
                            })
                        ]
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(0.0f, 7.0f, 0.0f, 3.0f)
                        [
                            SNew(SHorizontalBox)
                            .Visibility(
                                this,
                                &SMiaIAEditorPanel::NeuronBiasEditorVisibility)
                            .IsEnabled(
                                this,
                                &SMiaIAEditorPanel::CanEditNetworkParameters)
                            + SHorizontalBox::Slot()
                            .FillWidth(1.0f)
                            [
                                SNew(SSpinBox<double>)
                                .MinSliderValue(-1.0)
                                .MaxSliderValue(1.0)
                                .Delta(0.01)
                                .Value_Lambda([this]()
                                {
                                    return PendingNeuronBias;
                                })
                                .OnValueChanged_Lambda([this](double value)
                                {
                                    PendingNeuronBias = value;
                                    bPendingNeuronBiasDirty = true;
                                })
                                .ToolTipText(LOCTEXT(
                                    "NeuronBiasEditorTooltip",
                                    "Set the selected non-input neuron's bias."))
                            ]
                            + SHorizontalBox::Slot()
                            .AutoWidth()
                            .Padding(5.0f, 0.0f, 0.0f, 0.0f)
                            [
                                SNew(SButton)
                                .ButtonStyle(&ButtonStyle)
                                .Text(LOCTEXT("ApplyNeuronBias", "Apply bias"))
                                .OnClicked(
                                    this,
                                    &SMiaIAEditorPanel::
                                        HandleApplySelectedNeuronBias)
                            ]
                        ]
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(0.0f, 4.0f)
                        [
                            SNew(STextBlock)
                            .Visibility(
                                this,
                                &SMiaIAEditorPanel::
                                    InputNeuronBiasNoticeVisibility)
                            .Text(LOCTEXT(
                                "InputNeuronBiasNotice",
                                "Input neurons do not have an editable bias."))
                            .AutoWrapText(true)
                        ]
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(0.0f, 7.0f, 0.0f, 3.0f)
                        [
                            SNew(SHorizontalBox)
                            .Visibility(
                                this,
                                &SMiaIAEditorPanel::
                                    ConnectionWeightEditorVisibility)
                            .IsEnabled(
                                this,
                                &SMiaIAEditorPanel::CanEditNetworkParameters)
                            + SHorizontalBox::Slot()
                            .FillWidth(1.0f)
                            [
                                SNew(SSpinBox<double>)
                                .MinSliderValue(-1.0)
                                .MaxSliderValue(1.0)
                                .Delta(0.01)
                                .Value_Lambda([this]()
                                {
                                    return PendingConnectionWeight;
                                })
                                .OnValueChanged_Lambda([this](double value)
                                {
                                    PendingConnectionWeight = value;
                                    bPendingConnectionWeightDirty = true;
                                })
                                .ToolTipText(LOCTEXT(
                                    "ConnectionWeightEditorTooltip",
                                    "Set the selected connection's weight."))
                            ]
                            + SHorizontalBox::Slot()
                            .AutoWidth()
                            .Padding(5.0f, 0.0f, 0.0f, 0.0f)
                            [
                                SNew(SButton)
                                .ButtonStyle(&ButtonStyle)
                                .Text(LOCTEXT(
                                    "ApplyConnectionWeight",
                                    "Apply weight"))
                                .OnClicked(
                                    this,
                                    &SMiaIAEditorPanel::
                                        HandleApplySelectedConnectionWeight)
                            ]
                        ]
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(0.0f, 3.0f)
                        [
                            SNew(STextBlock)
                            .Text(this, &SMiaIAEditorPanel::SelectionGradientText)
                        ]
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(0.0f, 3.0f)
                        [
                            SNew(STextBlock)
                            .Text(this, &SMiaIAEditorPanel::SelectionUpdateText)
                        ]
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(0.0f, 7.0f, 0.0f, 3.0f)
                        [
                            SNew(SHorizontalBox)
                            .Visibility(
                                this,
                                &SMiaIAEditorPanel::
                                    ConnectionWeightEditorVisibility)
                            + SHorizontalBox::Slot()
                            .FillWidth(1.0f)
                            .Padding(0.0f, 0.0f, 2.0f, 0.0f)
                            [
                                SNew(SButton)
                                .ButtonStyle(&ButtonStyle)
                                .Text_Lambda([this]()
                                {
                                    return SelectedConnectionEndpointText(
                                        false);
                                })
                                .ToolTipText(LOCTEXT(
                                    "NavigateFromEndpointTooltip",
                                    "Select the source neuron of this connection."))
                                .OnClicked(
                                    this,
                                    &SMiaIAEditorPanel::
                                        HandleNavigateSelectedConnectionEndpoint,
                                    false)
                            ]
                            + SHorizontalBox::Slot()
                            .FillWidth(1.0f)
                            .Padding(2.0f, 0.0f, 0.0f, 0.0f)
                            [
                                SNew(SButton)
                                .ButtonStyle(&ButtonStyle)
                                .Text_Lambda([this]()
                                {
                                    return SelectedConnectionEndpointText(
                                        true);
                                })
                                .ToolTipText(LOCTEXT(
                                    "NavigateToEndpointTooltip",
                                    "Select the destination neuron of this connection."))
                                .OnClicked(
                                    this,
                                    &SMiaIAEditorPanel::
                                        HandleNavigateSelectedConnectionEndpoint,
                                    true)
                            ]
                        ]
                        + SVerticalBox::Slot()
                        .FillHeight(1.0f)
                        .Padding(0.0f, 8.0f, 0.0f, 0.0f)
                        [
                            SNew(SScrollBox)
                            .ScrollBarStyle(&ScrollBarStyle)
                            + SScrollBox::Slot()
                            [
                                SNew(SVerticalBox)
                                + SVerticalBox::Slot()
                                .AutoHeight()
                                [
                                    SNew(STextBlock)
                                    .Text(
                                        this,
                                        &SMiaIAEditorPanel::
                                            SelectionRelationshipsText)
                                    .AutoWrapText(true)
                                ]
                                + SVerticalBox::Slot()
                                .AutoHeight()
                                .Padding(0.0f, 8.0f, 0.0f, 4.0f)
                                [
                                    SNew(SHorizontalBox)
                                    .Visibility(
                                        this,
                                        &SMiaIAEditorPanel::
                                            RelationshipExplorerVisibility)
                                    + SHorizontalBox::Slot()
                                    .FillWidth(1.0f)
                                    .Padding(0.0f, 0.0f, 2.0f, 0.0f)
                                    [
                                        SNew(SButton)
                                        .ButtonStyle(&ButtonStyle)
                                        .Text_Lambda([this]()
                                        {
                                            return RelationshipDirection ==
                                                EMiaIANeuronRelationshipDirection::
                                                    Incoming
                                                ? LOCTEXT(
                                                    "IncomingRelationshipsActive",
                                                    "[Incoming]")
                                                : LOCTEXT(
                                                    "IncomingRelationships",
                                                    "Incoming");
                                        })
                                        .OnClicked(
                                            this,
                                            &SMiaIAEditorPanel::
                                                SelectRelationshipDirection,
                                            EMiaIANeuronRelationshipDirection::
                                                Incoming)
                                    ]
                                    + SHorizontalBox::Slot()
                                    .FillWidth(1.0f)
                                    .Padding(2.0f, 0.0f, 0.0f, 0.0f)
                                    [
                                        SNew(SButton)
                                        .ButtonStyle(&ButtonStyle)
                                        .Text_Lambda([this]()
                                        {
                                            return RelationshipDirection ==
                                                EMiaIANeuronRelationshipDirection::
                                                    Outgoing
                                                ? LOCTEXT(
                                                    "OutgoingRelationshipsActive",
                                                    "[Outgoing]")
                                                : LOCTEXT(
                                                    "OutgoingRelationships",
                                                    "Outgoing");
                                        })
                                        .OnClicked(
                                            this,
                                            &SMiaIAEditorPanel::
                                                SelectRelationshipDirection,
                                            EMiaIANeuronRelationshipDirection::
                                                Outgoing)
                                    ]
                                ]
                                + SVerticalBox::Slot()
                                .AutoHeight()
                                .Padding(0.0f, 2.0f)
                                [
                                    SNew(SHorizontalBox)
                                    .Visibility(
                                        this,
                                        &SMiaIAEditorPanel::
                                            RelationshipExplorerVisibility)
                                    + SHorizontalBox::Slot()
                                    .FillWidth(1.0f)
                                    [
                                        SNew(SComboButton)
                                        .ComboButtonStyle(&ComboButtonStyle)
                                        .OnGetMenuContent(
                                            this,
                                            &SMiaIAEditorPanel::
                                                BuildRelationshipSortMenu)
                                        .ButtonContent()
                                        [
                                            SNew(STextBlock)
                                            .Text(
                                                this,
                                                &SMiaIAEditorPanel::
                                                    RelationshipSortText)
                                        ]
                                    ]
                                    + SHorizontalBox::Slot()
                                    .AutoWidth()
                                    .Padding(4.0f, 0.0f, 0.0f, 0.0f)
                                    [
                                        SNew(SButton)
                                        .ButtonStyle(&ButtonStyle)
                                        .Text_Lambda([this]()
                                        {
                                            return bRelationshipSortDescending
                                                ? LOCTEXT(
                                                    "RelationshipDescending",
                                                    "Desc")
                                                : LOCTEXT(
                                                    "RelationshipAscending",
                                                    "Asc");
                                        })
                                        .OnClicked(
                                            this,
                                            &SMiaIAEditorPanel::
                                                ToggleRelationshipSortDirection)
                                    ]
                                ]
                                + SVerticalBox::Slot()
                                .AutoHeight()
                                .Padding(0.0f, 2.0f)
                                [
                                    SNew(SVerticalBox)
                                    .Visibility(
                                        this,
                                        &SMiaIAEditorPanel::
                                            RelationshipExplorerVisibility)
                                    + SVerticalBox::Slot()
                                    .AutoHeight()
                                    [
                                        SNew(STextBlock)
                                        .Text(LOCTEXT(
                                            "RelationshipWeightFilterLabel",
                                            "Minimum |weight|"))
                                    ]
                                    + SVerticalBox::Slot()
                                    .AutoHeight()
                                    [
                                        SNew(SSpinBox<double>)
                                        .MinValue(0.0)
                                        .MinSliderValue(0.0)
                                        .MaxSliderValue(1.0)
                                        .Delta(0.01)
                                        .Value_Lambda([this]()
                                        {
                                            return RelationshipMinimumAbsoluteWeight;
                                        })
                                        .OnValueCommitted(
                                            this,
                                            &SMiaIAEditorPanel::
                                                HandleRelationshipMinimumWeightCommitted)
                                        .ToolTipText(LOCTEXT(
                                            "RelationshipWeightFilterTooltip",
                                            "Show only connections whose absolute weight is at least this value."))
                                    ]
                                ]
                                + SVerticalBox::Slot()
                                .AutoHeight()
                                [
                                    SAssignNew(
                                        RelationshipContent,
                                        SVerticalBox)
                                ]
                            ]
                        ]
                    ]
                ]
            ]
            + SSplitter::Slot()
            .Value(0.28f)
            [
                SNew(SBorder)
                .Visibility_Lambda([this]()
                {
                    return bTopologyWorkspaceExpanded
                        ? EVisibility::Collapsed
                        : EVisibility::Visible;
                })
                .BorderImage(panelBorder)
                .BorderBackgroundColor(this, &SMiaIAEditorPanel::PanelColor)
                .Padding(6.0f)
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .Padding(2.0f)
                        [
                            SNew(SButton)
                            .ButtonStyle(&ButtonStyle)
                            .Text(LOCTEXT("ConsoleTab", "Console"))
                            .OnClicked(
                                this,
                                &SMiaIAEditorPanel::SelectBottomTab,
                                1)
                        ]
                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .Padding(2.0f)
                        [
                            SNew(SButton)
                            .ButtonStyle(&ButtonStyle)
                            .Text(LOCTEXT("TimelineTab", "Training timeline"))
                            .OnClicked(
                                this,
                                &SMiaIAEditorPanel::SelectBottomTab,
                                0)
                        ]
                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .Padding(2.0f)
                        [
                            SNew(SButton)
                            .ButtonStyle(&ButtonStyle)
                            .Text(LOCTEXT("BreakpointsTab", "Breakpoints"))
                            .OnClicked(
                                this,
                                &SMiaIAEditorPanel::SelectBottomTab,
                                2)
                        ]
                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .Padding(2.0f)
                        [
                            SNew(SButton)
                            .ButtonStyle(&ButtonStyle)
                            .Text(LOCTEXT(
                                "ForwardTraceTab",
                                "Execution trace"))
                            .OnClicked(
                                this,
                                &SMiaIAEditorPanel::SelectBottomTab,
                                3)
                        ]
                    ]
                    + SVerticalBox::Slot()
                    .FillHeight(1.0f)
                    .Padding(4.0f)
                    [
                        SAssignNew(BottomSwitcher, SWidgetSwitcher)
                        + SWidgetSwitcher::Slot()
                        [
                            SNew(SUniformGridPanel)
                            .SlotPadding(FMargin(4.0f))
                            + SUniformGridPanel::Slot(0, 0)
                            [
                                SNew(STextBlock)
                                .Text(LOCTEXT("Before", "1  Before"))
                                .ColorAndOpacity_Lambda([this]()
                                {
                                    return PhaseColor(
                                        EMiaIATrainingDebugPhase::BeforeForward);
                                })
                            ]
                            + SUniformGridPanel::Slot(1, 0)
                            [
                                SNew(STextBlock)
                                .Text(LOCTEXT("Forward", "2  Forward"))
                                .ColorAndOpacity_Lambda([this]()
                                {
                                    return PhaseColor(
                                        EMiaIATrainingDebugPhase::ForwardComplete);
                                })
                            ]
                            + SUniformGridPanel::Slot(2, 0)
                            [
                                SNew(STextBlock)
                                .Text(LOCTEXT("Backward", "3  Backward"))
                                .ColorAndOpacity_Lambda([this]()
                                {
                                    return PhaseColor(
                                        EMiaIATrainingDebugPhase::BackwardComplete);
                                })
                            ]
                            + SUniformGridPanel::Slot(3, 0)
                            [
                                SNew(STextBlock)
                                .Text(LOCTEXT("Update", "4  Update"))
                                .ColorAndOpacity_Lambda([this]()
                                {
                                    return PhaseColor(
                                        EMiaIATrainingDebugPhase::UpdateComplete);
                                })
                            ]
                            + SUniformGridPanel::Slot(4, 0)
                            [
                                SNew(STextBlock)
                                .Text(LOCTEXT("Verify", "5  Verify"))
                                .ColorAndOpacity_Lambda([this]()
                                {
                                    return PhaseColor(
                                        EMiaIATrainingDebugPhase::Verified);
                                })
                            ]
                            + SUniformGridPanel::Slot(5, 0)
                            [
                                SNew(STextBlock)
                                .Text(LOCTEXT("Commit", "6  Commit"))
                                .ColorAndOpacity_Lambda([this]()
                                {
                                    return PhaseColor(
                                        EMiaIATrainingDebugPhase::Committed);
                                })
                            ]
                        ]
                        + SWidgetSwitcher::Slot()
                        [
                            SNew(SSplitter)
                            .Style(&SplitterStyle)
                            .Orientation(Orient_Horizontal)
                            + SSplitter::Slot()
                            .Value(0.24f)
                            [
                                SNew(SBorder)
                                .BorderImage(panelBorder)
                                .BorderBackgroundColor(
                                    this,
                                    &SMiaIAEditorPanel::PanelColor)
                                .Padding(FMargin(4.0f, 2.0f))
                                [
                                    SNew(SVerticalBox)
                                    + SVerticalBox::Slot()
                                    .AutoHeight()
                                    .Padding(2.0f, 1.0f, 2.0f, 3.0f)
                                    [
                                        SNew(STextBlock)
                                        .Text(LOCTEXT(
                                            "ConsoleCommands",
                                            "Commands"))
                                        .Font(FAppStyle::GetFontStyle(
                                            TEXT("SmallFontBold")))
                                    ]
                                    + SVerticalBox::Slot()
                                    .FillHeight(1.0f)
                                    [
                                        SNew(SScrollBox)
                                        .ScrollBarStyle(&ScrollBarStyle)
                                        + SScrollBox::Slot()
                                        [
                                            SAssignNew(
                                                ConsoleSuggestionsContent,
                                                SVerticalBox)
                                        ]
                                    ]
                                ]
                            ]
                            + SSplitter::Slot()
                            .Value(0.76f)
                            [
                                SNew(SVerticalBox)
                                + SVerticalBox::Slot()
                                .FillHeight(1.0f)
                                [
                                    SNew(SHorizontalBox)
                                    + SHorizontalBox::Slot()
                                    .AutoWidth()
                                    .Padding(0.0f, 0.0f, 4.0f, 0.0f)
                                    [
                                        ConsoleOutputScrollBar.ToSharedRef()
                                    ]
                                    + SHorizontalBox::Slot()
                                    .FillWidth(1.0f)
                                    [
                                        SAssignNew(
                                            ConsoleOutput,
                                            SMultiLineEditableText)
                                        .IsReadOnly(true)
                                        .Text(FText::FromString(ConsoleHistory))
                                        .VScrollBar(ConsoleOutputScrollBar)
                                    ]
                                ]
                                + SVerticalBox::Slot()
                                .AutoHeight()
                                .Padding(4.0f, 6.0f, 0.0f, 0.0f)
                                [
                                    SNew(SHorizontalBox)
                                    + SHorizontalBox::Slot()
                                    .FillWidth(1.0f)
                                    [
                                        SAssignNew(
                                            ConsoleInput,
                                            SEditableTextBox)
                                        .Style(&InputStyle)
                                        .ClearKeyboardFocusOnCommit(false)
                                        .HintText(LOCTEXT(
                                            "ConsoleInputHint",
                                            "Enter a MiaIA command and press Enter"))
                                        .OnTextChanged(
                                            this,
                                            &SMiaIAEditorPanel::HandleConsoleTextChanged)
                                        .OnTextCommitted(
                                            this,
                                            &SMiaIAEditorPanel::HandleConsoleCommandCommitted)
                                        .OnKeyDownHandler(
                                            this,
                                            &SMiaIAEditorPanel::HandleConsoleInputKeyDown)
                                    ]
                                    + SHorizontalBox::Slot()
                                    .AutoWidth()
                                    .Padding(6.0f, 0.0f, 0.0f, 0.0f)
                                    [
                                        SNew(SButton)
                                        .ButtonStyle(&ButtonStyle)
                                        .Text(LOCTEXT(
                                            "ConsoleSend",
                                            "Send"))
                                        .OnClicked(
                                            this,
                                            &SMiaIAEditorPanel::HandleConsoleSend)
                                    ]
                                ]
                            ]
                        ]
                        + SWidgetSwitcher::Slot()
                        [
                            SNew(SVerticalBox)
                            + SVerticalBox::Slot()
                            .AutoHeight()
                            .Padding(2.0f, 2.0f, 2.0f, 6.0f)
                            [
                                SNew(SHorizontalBox)
                                + SHorizontalBox::Slot()
                                .AutoWidth()
                                .Padding(2.0f)
                                [
                                    SNew(SComboButton)
                                    .ComboButtonStyle(&ComboButtonStyle)
                                    .OnGetMenuContent(
                                        this,
                                        &SMiaIAEditorPanel::BuildBreakpointKindMenu)
                                    .ButtonContent()
                                    [
                                        SNew(STextBlock)
                                        .Text(
                                            this,
                                            &SMiaIAEditorPanel::BreakpointKindText)
                                    ]
                                ]
                                + SHorizontalBox::Slot()
                                .AutoWidth()
                                .Padding(2.0f)
                                [
                                    SNew(SBox)
                                    .Visibility(
                                        this,
                                        &SMiaIAEditorPanel::BreakpointPhaseVisibility)
                                    [
                                        SNew(SComboButton)
                                        .ComboButtonStyle(&ComboButtonStyle)
                                        .OnGetMenuContent(
                                            this,
                                            &SMiaIAEditorPanel::BuildBreakpointPhaseMenu)
                                        .ButtonContent()
                                        [
                                            SNew(STextBlock)
                                            .Text(
                                                this,
                                                &SMiaIAEditorPanel::BreakpointPhaseText)
                                        ]
                                    ]
                                ]
                                + SHorizontalBox::Slot()
                                .AutoWidth()
                                .Padding(2.0f)
                                [
                                    SNew(SBox)
                                    .WidthOverride(120.0f)
                                    .Visibility(
                                        this,
                                        &SMiaIAEditorPanel::BreakpointTargetVisibility)
                                    [
                                        SAssignNew(
                                            BreakpointTargetInput,
                                            SEditableTextBox)
                                        .Style(&InputStyle)
                                        .HintText(LOCTEXT(
                                            "BreakpointTargetHint",
                                            "Neuron/connection ID"))
                                    ]
                                ]
                                + SHorizontalBox::Slot()
                                .AutoWidth()
                                .Padding(2.0f)
                                [
                                    SNew(SBox)
                                    .WidthOverride(110.0f)
                                    .Visibility(
                                        this,
                                        &SMiaIAEditorPanel::BreakpointTargetVisibility)
                                    [
                                        SAssignNew(
                                            BreakpointThresholdInput,
                                            SEditableTextBox)
                                        .Style(&InputStyle)
                                        .HintText(LOCTEXT(
                                            "BreakpointThresholdHint",
                                            "Threshold"))
                                    ]
                                ]
                                + SHorizontalBox::Slot()
                                .AutoWidth()
                                .Padding(2.0f)
                                [
                                    SNew(SButton)
                                    .ButtonStyle(&ButtonStyle)
                                    .Text(LOCTEXT("AddBreakpoint", "Add"))
                                    .OnClicked(
                                        this,
                                        &SMiaIAEditorPanel::HandleAddBreakpoint)
                                ]
                                + SHorizontalBox::Slot()
                                .AutoWidth()
                                .Padding(2.0f)
                                [
                                    SNew(SButton)
                                    .ButtonStyle(&ButtonStyle)
                                    .Text(LOCTEXT("ClearBreakpoints", "Clear"))
                                    .OnClicked(
                                        this,
                                        &SMiaIAEditorPanel::HandleClearBreakpoints)
                                ]
                            ]
                            + SVerticalBox::Slot()
                            .AutoHeight()
                            .Padding(4.0f, 0.0f, 4.0f, 5.0f)
                            [
                                SNew(STextBlock)
                                .Text(
                                    this,
                                    &SMiaIAEditorPanel::LastBreakpointHitText)
                                .AutoWrapText(true)
                            ]
                            + SVerticalBox::Slot()
                            .FillHeight(1.0f)
                            [
                                SNew(SScrollBox)
                                .ScrollBarStyle(&ScrollBarStyle)
                                + SScrollBox::Slot()
                                [
                                    SAssignNew(
                                        BreakpointContent,
                                        SVerticalBox)
                                ]
                            ]
                        ]
                        + SWidgetSwitcher::Slot()
                        [
                            SNew(SVerticalBox)
                            + SVerticalBox::Slot()
                            .AutoHeight()
                            .Padding(2.0f, 2.0f, 2.0f, 6.0f)
                            [
                                SNew(SHorizontalBox)
                                + SHorizontalBox::Slot()
                                .FillWidth(1.0f)
                                [
                                    SAssignNew(
                                        ForwardTraceInput,
                                        SEditableTextBox)
                                    .Style(&InputStyle)
                                    .HintText(LOCTEXT(
                                        "ForwardTraceInputHint",
                                        "Input vector, for example: 1 1"))
                                ]
                                + SHorizontalBox::Slot()
                                .AutoWidth()
                                .Padding(6.0f, 0.0f, 0.0f, 0.0f)
                                [
                                    SNew(SButton)
                                    .ButtonStyle(&ButtonStyle)
                                    .Text(LOCTEXT(
                                        "RunForwardTrace",
                                        "Run trace"))
                                    .OnClicked(
                                        this,
                                        &SMiaIAEditorPanel::
                                            HandleRunForwardTrace)
                                ]
                                + SHorizontalBox::Slot()
                                .AutoWidth()
                                .Padding(4.0f, 0.0f, 0.0f, 0.0f)
                                [
                                    SNew(SButton)
                                    .ButtonStyle(&ButtonStyle)
                                    .Text(LOCTEXT(
                                        "ClearForwardTrace",
                                        "Clear"))
                                    .OnClicked(
                                        this,
                                        &SMiaIAEditorPanel::
                                            HandleClearForwardTrace)
                                ]
                            ]
                            + SVerticalBox::Slot()
                            .AutoHeight()
                            .Padding(2.0f, 0.0f, 2.0f, 5.0f)
                            [
                                SNew(SHorizontalBox)
                                + SHorizontalBox::Slot()
                                .AutoWidth()
                                .Padding(2.0f)
                                [
                                    SNew(SButton)
                                    .ButtonStyle(&ButtonStyle)
                                    .Text(LOCTEXT(
                                        "RestartForwardTrace",
                                        "Reset"))
                                    .OnClicked(
                                        this,
                                        &SMiaIAEditorPanel::
                                            HandleRestartForwardTrace)
                                ]
                                + SHorizontalBox::Slot()
                                .AutoWidth()
                                .Padding(2.0f)
                                [
                                    SNew(SButton)
                                    .ButtonStyle(&ButtonStyle)
                                    .Text(LOCTEXT(
                                        "PreviousForwardTraceFrame",
                                        "Previous frame"))
                                    .OnClicked(
                                        this,
                                        &SMiaIAEditorPanel::
                                            HandlePreviousForwardTraceFrame)
                                ]
                                + SHorizontalBox::Slot()
                                .AutoWidth()
                                .Padding(2.0f)
                                [
                                    SNew(SButton)
                                    .ButtonStyle(&ButtonStyle)
                                    .Text(
                                        this,
                                        &SMiaIAEditorPanel::
                                            ForwardTracePlayPauseText)
                                    .OnClicked(
                                        this,
                                        &SMiaIAEditorPanel::
                                            HandleToggleForwardTracePlayback)
                                ]
                                + SHorizontalBox::Slot()
                                .AutoWidth()
                                .Padding(2.0f)
                                [
                                    SNew(SButton)
                                    .ButtonStyle(&ButtonStyle)
                                    .Text(LOCTEXT(
                                        "NextForwardTraceFrame",
                                        "Next frame"))
                                    .OnClicked(
                                        this,
                                        &SMiaIAEditorPanel::
                                            HandleNextForwardTraceFrame)
                                ]
                                + SHorizontalBox::Slot()
                                .AutoWidth()
                                .VAlign(VAlign_Center)
                                .Padding(8.0f, 2.0f, 2.0f, 2.0f)
                                [
                                    SNew(SComboButton)
                                    .ComboButtonStyle(&ComboButtonStyle)
                                    .ButtonContent()
                                    [
                                        SNew(STextBlock)
                                        .Text(
                                            this,
                                            &SMiaIAEditorPanel::
                                                ForwardTraceSpeedText)
                                    ]
                                    .OnGetMenuContent(
                                        this,
                                        &SMiaIAEditorPanel::
                                            BuildForwardTraceSpeedMenu)
                                ]
                            ]
                            + SVerticalBox::Slot()
                            .AutoHeight()
                            .Padding(4.0f, 0.0f, 4.0f, 6.0f)
                            [
                                SNew(STextBlock)
                                .Text(
                                    this,
                                    &SMiaIAEditorPanel::
                                        ForwardTraceSummaryText)
                                .AutoWrapText(true)
                            ]
                            + SVerticalBox::Slot()
                            .FillHeight(1.0f)
                            [
                                SNew(SScrollBox)
                                .ScrollBarStyle(&ScrollBarStyle)
                                + SScrollBox::Slot()
                                [
                                    SAssignNew(
                                        ForwardTraceContent,
                                        SVerticalBox)
                                ]
                            ]
                            + SVerticalBox::Slot()
                            .AutoHeight()
                            .HAlign(HAlign_Right)
                            .Padding(2.0f, 5.0f, 2.0f, 0.0f)
                            [
                                SNew(SHorizontalBox)
                                + SHorizontalBox::Slot()
                                .AutoWidth()
                                .Padding(2.0f)
                                [
                                    SNew(SButton)
                                    .ButtonStyle(&ButtonStyle)
                                    .Text(LOCTEXT(
                                        "PreviousForwardTracePage",
                                        "Previous page"))
                                    .OnClicked(
                                        this,
                                        &SMiaIAEditorPanel::
                                            HandlePreviousForwardTracePage)
                                ]
                                + SHorizontalBox::Slot()
                                .AutoWidth()
                                .Padding(2.0f)
                                [
                                    SNew(SButton)
                                    .ButtonStyle(&ButtonStyle)
                                    .Text(LOCTEXT(
                                        "NextForwardTracePage",
                                        "Next page"))
                                    .OnClicked(
                                        this,
                                        &SMiaIAEditorPanel::
                                            HandleNextForwardTracePage)
                                ]
                            ]
                        ]
                    ]
                ]
            ]
        ]
        ]
        ]
        + SOverlay::Slot()
        [
            SNew(SBorder)
            .Visibility(this, &SMiaIAEditorPanel::DialogVisibility)
            .BorderImage(panelBorder)
            .BorderBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.68f))
            .OnMouseButtonDown_Lambda(
                [](const FGeometry&, const FPointerEvent&)
                {
                    return FReply::Handled();
                })
        ]
        + SOverlay::Slot()
        .HAlign(HAlign_Center)
        .VAlign(VAlign_Center)
        .Padding(40.0f)
        [
            SNew(SBox)
            .Visibility(this, &SMiaIAEditorPanel::DialogVisibility)
            .WidthOverride(640.0f)
            .MaxDesiredHeight(620.0f)
            [
                SNew(SBorder)
                .BorderImage(panelBorder)
                .BorderBackgroundColor(
                    this,
                    &SMiaIAEditorPanel::PanelColor)
                .ForegroundColor(this, &SMiaIAEditorPanel::TextColor)
                .Padding(18.0f)
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 0.0f, 0.0f, 14.0f)
                    [
                        SNew(STextBlock)
                        .Text_Lambda([this]() { return DialogTitle; })
                        .Font(FAppStyle::GetFontStyle(TEXT("HeadingExtraSmall")))
                    ]
                    + SVerticalBox::Slot()
                    .FillHeight(1.0f)
                    [
                        SNew(SScrollBox)
                        .Visibility(
                            this,
                            &SMiaIAEditorPanel::DialogContentVisibility)
                        + SScrollBox::Slot()
                        [
                            SNew(STextBlock)
                            .Text_Lambda([this]() { return DialogContent; })
                            .AutoWrapText(true)
                        ]
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 12.0f, 0.0f, 0.0f)
                    [
                        SAssignNew(ProjectPathInput, SEditableTextBox)
                        .Visibility(
                            this,
                            &SMiaIAEditorPanel::ProjectPathVisibility)
                        .Style(&InputStyle)
                        .HintText_Lambda([this]()
                        {
                            return ProjectPathAction ==
                                    EMiaIAProjectPathAction::ImportOnnx ||
                                ProjectPathAction ==
                                    EMiaIAProjectPathAction::ExportOnnx
                                ? LOCTEXT(
                                    "OnnxPathHint",
                                    "Full path to an .onnx model")
                                : LOCTEXT(
                                    "ProjectPathHint",
                                    "Full path to a .mai project");
                        })
                        .OnTextCommitted_Lambda(
                            [this](const FText&, ETextCommit::Type CommitType)
                            {
                                if (CommitType == ETextCommit::OnEnter)
                                {
                                    HandleConfirmProjectPath();
                                }
                            })
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .HAlign(HAlign_Right)
                    .Padding(0.0f, 16.0f, 0.0f, 0.0f)
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .Padding(2.0f)
                        [
                            SNew(SButton)
                            .Visibility(
                                this,
                                &SMiaIAEditorPanel::ProjectPathVisibility)
                            .ButtonStyle(&ButtonStyle)
                            .Text(LOCTEXT("CancelProjectPath", "Cancel"))
                            .OnClicked(
                                this,
                                &SMiaIAEditorPanel::HandleCancelProjectPath)
                        ]
                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .Padding(2.0f)
                        [
                            SNew(SButton)
                            .Visibility(
                                this,
                                &SMiaIAEditorPanel::ProjectPathVisibility)
                            .ButtonStyle(&ButtonStyle)
                            .Text_Lambda([this]()
                            {
                                switch (ProjectPathAction)
                                {
                                case EMiaIAProjectPathAction::Open:
                                    return LOCTEXT(
                                        "OpenProjectConfirm",
                                        "Open");
                                case EMiaIAProjectPathAction::ImportOnnx:
                                    return LOCTEXT(
                                        "ImportOnnxConfirm",
                                        "Import");
                                case EMiaIAProjectPathAction::ExportOnnx:
                                    return LOCTEXT(
                                        "ExportOnnxConfirm",
                                        "Export");
                                default:
                                    return LOCTEXT(
                                        "SaveProjectConfirm",
                                        "Save");
                                }
                            })
                            .OnClicked(
                                this,
                                &SMiaIAEditorPanel::HandleConfirmProjectPath)
                        ]
                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .Padding(2.0f)
                        [
                            SNew(SButton)
                            .Visibility(
                                this,
                                &SMiaIAEditorPanel::DialogContentVisibility)
                            .ButtonStyle(&ButtonStyle)
                            .Text(LOCTEXT("CloseStudioDialog", "Close"))
                            .OnClicked(
                                this,
                                &SMiaIAEditorPanel::HandleCloseDialog)
                        ]
                    ]
                ]
            ]
        ]
    ];

    BottomSwitcher->SetActiveWidgetIndex(1);
    TopologySwitcher->SetActiveWidgetIndex(0);
    NetworkView->SetTheme(Theme);
    Network3DView->SetTheme(Theme);
    NetworkView->SetVisualizationSettings(VisualizationSettings);
    Network3DView->SetVisualizationSettings(VisualizationSettings);
    RebuildConsoleSuggestions(FString());
    RefreshData();
    RebuildForwardTrace();
    RegisterActiveTimer(
        0.1f,
        FWidgetActiveTimerDelegate::CreateSP(
            this,
            &SMiaIAEditorPanel::HandleRefreshTimer));
}

void SMiaIAEditorPanel::RefreshData()
{
    PeriodicRefreshElapsedSeconds = 0.0;
    const bool previouslyHadNetwork = !NetworkOverview.Layers.IsEmpty();
    const FString previousOverviewKey = BuildOverviewKey(NetworkOverview);
    NetworkOverview = UMiaIABlueprintLibrary::GetNetworkOverview();
    const bool hasNetwork = !NetworkOverview.Layers.IsEmpty();
    const bool networkDefinitionChanged = previouslyHadNetwork &&
        hasNetwork &&
        previousOverviewKey != BuildOverviewKey(NetworkOverview);
    bNetworkRequiresCompactTopology =
        MiaIA::Studio::StudioTopologyBuilder::RequiresCompactMode(
            static_cast<std::size_t>(NetworkOverview.NeuronCount),
            static_cast<std::size_t>(NetworkOverview.ConnectionCount),
            static_cast<std::size_t>(DetailedNeuronLimit),
            static_cast<std::size_t>(DetailedConnectionLimit));

    if (!hasNetwork)
    {
        FocusedLayerId = -1;
        bNetworkPreview = false;
        RelationshipOffset = 0;
    }
    else if (!previouslyHadNetwork || networkDefinitionChanged)
    {
        FocusedLayerId = -1;
        bNetworkPreview = true;
        RelationshipOffset = 0;
    }
    else if (!bNetworkRequiresCompactTopology && FocusedLayerId >= 0)
    {
        FocusedLayerId = -1;
    }

    bCompactTopology = bNetworkPreview ||
        (bNetworkRequiresCompactTopology && FocusedLayerId < 0);
    Session = UMiaIABlueprintLibrary::GetTrainingSession();
    Breakpoints = Session.Breakpoints;
    const FString newBreakpointKey =
        BuildBreakpointKey(Breakpoints, Session);
    const bool breakpointsChanged =
        newBreakpointKey != BreakpointKey;
    BreakpointKey = newBreakpointKey;

    Network = FMiaIANetworkSnapshot{};
    Debug = FMiaIATrainingDebugSnapshot{};

    if (FocusedLayerId >= 0)
    {
        FMiaIALayerSnapshot layer;

        if (UMiaIABlueprintLibrary::GetLayerSnapshot(
            FocusedLayerId,
            layer))
        {
            Network.Layers.Add(MoveTemp(layer));
        }
        else
        {
            FocusedLayerId = -1;
            bNetworkPreview = false;
            bCompactTopology = bNetworkRequiresCompactTopology;
        }
    }

    if (bCompactTopology)
    {
        Network = FMiaIANetworkSnapshot{};
    }
    else if (FocusedLayerId < 0)
    {
        Network = UMiaIABlueprintLibrary::GetNetworkSnapshot();
        Debug = UMiaIABlueprintLibrary::GetDebugStatus();

        if (Debug.Phase != EMiaIATrainingDebugPhase::Idle &&
            !Debug.CandidateNetwork.Layers.IsEmpty())
        {
            Network = Debug.CandidateNetwork;
        }
    }

    const FString newTopologyKey = bCompactTopology
        ? FString::Printf(
            TEXT("%s:%s"),
            bNetworkPreview ? TEXT("Network") : TEXT("Layers"),
            *BuildOverviewKey(NetworkOverview))
        : FocusedLayerId >= 0
            ? FString::Printf(
                TEXT("Focus%lld:%s"),
                FocusedLayerId,
                *BuildTopologyKey(Network))
            : BuildTopologyKey(Network);
    const bool topologyChanged = newTopologyKey != TopologyKey;
    TopologyKey = newTopologyKey;

    if (!bCompactTopology || bNetworkPreview ||
        !FindOverviewLayer(SelectedLayerId))
    {
        SelectedLayerId = -1;
    }

    FMiaIAConnectionInspection selectedConnectionProbe;
    if (SelectedConnectionId >= 0 &&
        !FindConnection(SelectedConnectionId) &&
        !UMiaIABlueprintLibrary::InspectConnection(
            SelectedConnectionId,
            selectedConnectionProbe))
    {
        SelectedConnectionId = -1;
    }

    for (auto iterator = SelectedNeuronIds.CreateIterator(); iterator; ++iterator)
    {
        if (!FindNeuron(*iterator))
        {
            iterator.RemoveCurrent();
        }
    }

    if (SelectedConnectionId >= 0)
    {
        SelectedNeuronId = -1;
        SelectedNeuronIds.Reset();
    }
    else
    {
        if (!SelectedNeuronIds.Contains(SelectedNeuronId))
        {
            SelectedNeuronId = -1;
        }

        if (SelectedNeuronIds.Num() == 0 && topologyChanged &&
            !Network.Layers.IsEmpty() &&
            !Network.Layers[0].Neurons.IsEmpty())
        {
            SelectedNeuronId = Network.Layers[0].Neurons[0].Id;
            SelectedNeuronIds.Add(SelectedNeuronId);
        }
        else if (SelectedNeuronId < 0)
        {
            for (const int64 selectedId : SelectedNeuronIds)
            {
                SelectedNeuronId = selectedId;
                break;
            }
        }
    }

    bHasDebugNeuron = SelectedNeuronIds.Num() == 1 &&
        SelectedNeuronId >= 0 &&
        UMiaIABlueprintLibrary::GetDebugNeuron(
            SelectedNeuronId,
            DebugNeuron);
    bHasDebugConnection = SelectedConnectionId >= 0 &&
        UMiaIABlueprintLibrary::GetDebugConnection(
            SelectedConnectionId,
            DebugConnection);

    NeuronInspection = {};
    ConnectionInspection = {};
    bHasNeuronInspection = SelectedNeuronIds.Num() == 1 &&
        SelectedNeuronId >= 0 &&
        UMiaIABlueprintLibrary::InspectNeuron(
            SelectedNeuronId,
            InspectorConnectionLimit,
            NeuronInspection);
    bHasConnectionInspection = SelectedConnectionId >= 0 &&
        UMiaIABlueprintLibrary::InspectConnection(
            SelectedConnectionId,
            ConnectionInspection);

    bool forwardTraceFocusChanged = false;
    const MiaIA::Studio::StudioForwardTraceState forwardTrace =
        FMiaIAInstanceService::ForwardTraceState(MiaIAInstance);
    if (forwardTrace.Active && SelectedNeuronIds.Num() == 1 &&
        SelectedNeuronId >= 0 &&
        forwardTrace.FocusedNeuronId !=
            static_cast<uint64>(SelectedNeuronId))
    {
        forwardTraceFocusChanged =
            FMiaIAInstanceService::FocusForwardTraceNeuron(
                MiaIAInstance,
                static_cast<uint64>(SelectedNeuronId));
    }

    RelationshipPage = {};
    bHasRelationshipPage = bHasNeuronInspection &&
        UMiaIABlueprintLibrary::GetNeuronRelationshipPage(
            SelectedNeuronId,
            RelationshipDirection,
            RelationshipOffset,
            FMath::Max(1, InspectorConnectionLimit),
            RelationshipSort,
            bRelationshipSortDescending,
            RelationshipMinimumAbsoluteWeight,
            RelationshipPage);

    if (bHasRelationshipPage &&
        RelationshipPage.FilteredConnectionCount > 0 &&
        RelationshipOffset >= RelationshipPage.FilteredConnectionCount)
    {
        const int64 pageSize = FMath::Max(1, InspectorConnectionLimit);
        RelationshipOffset =
            ((RelationshipPage.FilteredConnectionCount - 1) / pageSize) *
            pageSize;
        bHasRelationshipPage =
            UMiaIABlueprintLibrary::GetNeuronRelationshipPage(
                SelectedNeuronId,
                RelationshipDirection,
                RelationshipOffset,
                static_cast<int32>(pageSize),
                RelationshipSort,
                bRelationshipSortDescending,
                RelationshipMinimumAbsoluteWeight,
                RelationshipPage);
    }

    FString newRelationshipKey = bHasRelationshipPage
        ? FString::Printf(
            TEXT("%lld:%d:%lld:%d:%d:%.17g:%lld:%lld"),
            SelectedNeuronId,
            static_cast<int32>(RelationshipDirection),
            RelationshipPage.Offset,
            RelationshipPage.Limit,
            static_cast<int32>(RelationshipSort) * 2 +
                (bRelationshipSortDescending ? 1 : 0),
            RelationshipMinimumAbsoluteWeight,
            RelationshipPage.TotalConnectionCount,
            RelationshipPage.FilteredConnectionCount)
        : TEXT("None");
    for (const FMiaIAConnectionSnapshot& connection :
        RelationshipPage.Connections)
    {
        newRelationshipKey += FString::Printf(
            TEXT(":%lld:%.17g"),
            connection.Id,
            connection.Weight);
    }
    const bool relationshipsChanged =
        newRelationshipKey != RelationshipKey;
    RelationshipKey = MoveTemp(newRelationshipKey);

    if (bHasNeuronInspection)
    {
        const int64 inspectedNeuronId =
            NeuronInspection.Context.Neuron.Id;
        if (PendingNeuronBiasId != inspectedNeuronId ||
            !bPendingNeuronBiasDirty)
        {
            PendingNeuronBiasId = inspectedNeuronId;
            PendingNeuronBias = NeuronInspection.Context.Neuron.Bias;
            bPendingNeuronBiasDirty = false;
        }
    }
    else
    {
        PendingNeuronBiasId = -1;
        bPendingNeuronBiasDirty = false;
    }

    if (bHasConnectionInspection)
    {
        const int64 inspectedConnectionId =
            ConnectionInspection.Connection.Id;
        if (PendingConnectionWeightId != inspectedConnectionId ||
            !bPendingConnectionWeightDirty)
        {
            PendingConnectionWeightId = inspectedConnectionId;
            PendingConnectionWeight =
                ConnectionInspection.Connection.Weight;
            bPendingConnectionWeightDirty = false;
        }
    }
    else
    {
        PendingConnectionWeightId = -1;
        bPendingConnectionWeightDirty = false;
    }

    SelectedLayerName.Reset();

    for (const FMiaIALayerSnapshot& layer : Network.Layers)
    {
        if (SelectedNeuronIds.Num() == 1 &&
            layer.Neurons.ContainsByPredicate(
            [this](const FMiaIANeuronSnapshot& neuron)
            {
                return neuron.Id == SelectedNeuronId;
            }))
        {
            SelectedLayerName = FString::Printf(
                TEXT("%s · %s"),
                *layer.Name,
                *ActivationName(layer.Activation));
            break;
        }
    }

    if (NetworkView.IsValid())
    {
        NetworkView->SetSnapshot(Network);
        NetworkView->SetOverview(
            NetworkOverview,
            bCompactTopology,
            bNetworkPreview);
        NetworkView->SetDebugSnapshot(Debug);
        NetworkView->SetSelectedNeurons(
            SelectedNeuronIds,
            SelectedNeuronId);
        NetworkView->SetSelectedConnection(SelectedConnectionId);
        NetworkView->SetSelectedLayer(SelectedLayerId);
    }

    if (Network3DView.IsValid())
    {
        Network3DView->SetSnapshot(Network);
        Network3DView->SetOverview(
            NetworkOverview,
            bCompactTopology,
            bNetworkPreview);
        Network3DView->SetDebugSnapshot(Debug);
        Network3DView->SetSelectedNeurons(
            SelectedNeuronIds,
            SelectedNeuronId);
        Network3DView->SetSelectedConnection(SelectedConnectionId);
        Network3DView->SetSelectedLayer(SelectedLayerId);
    }

    ApplyForwardTraceOverlay();

    if (topologyChanged)
    {
        RebuildExplorer();
    }

    if (breakpointsChanged)
    {
        RebuildBreakpoints();
    }

    if (relationshipsChanged)
    {
        RebuildRelationshipExplorer();
    }

    if (forwardTraceFocusChanged)
    {
        RebuildForwardTrace();
    }
}

void SMiaIAEditorPanel::ApplyForwardTraceOverlay()
{
    TMap<int64, double> activations;
    TMap<int64, double> contributions;
    TSet<int64> playbackConnections;
    bool playbackActive = false;
    const MiaIA::Studio::StudioForwardTraceState state =
        FMiaIAInstanceService::ForwardTraceState(MiaIAInstance);

    if (state.Active)
    {
        const bool playbackComplete = state.PlaybackStatus ==
            MiaIA::Studio::StudioForwardTracePlaybackStatus::Completed;
        const bool hasPlaybackFrame =
            !state.PlaybackFrames.empty() &&
            state.PlaybackFrameIndex < state.PlaybackFrames.size();
        std::size_t activatedThroughLayer = state.Trace.Layers.size();

        if (!playbackComplete && hasPlaybackFrame)
        {
            playbackActive = true;
            const MiaIA::Studio::StudioForwardTraceFrame& frame =
                state.PlaybackFrames[state.PlaybackFrameIndex];
            activatedThroughLayer = frame.LayerIndex + 1;

            if (frame.Kind ==
                MiaIA::Studio::StudioForwardTraceFrameKind::IncomingSignal)
            {
                activatedThroughLayer = frame.LayerIndex;
            }

            if (frame.Kind != MiaIA::Studio::
                StudioForwardTraceFrameKind::InputActivations &&
                frame.LayerIndex < state.Trace.Layers.size())
            {
                TSet<int64> targetNeurons;

                for (const MiaIA::Core::ForwardTraceNeuronSnapshot& neuron :
                    state.Trace.Layers[frame.LayerIndex].Neurons)
                {
                    targetNeurons.Add(static_cast<int64>(neuron.Id));
                }

                for (const FMiaIAConnectionSnapshot& connection :
                    Network.Connections)
                {
                    if (targetNeurons.Contains(connection.ToNeuron))
                    {
                        playbackConnections.Add(connection.Id);
                    }
                }
            }
        }

        for (std::size_t layerIndex = 0;
            layerIndex < state.Trace.Layers.size() &&
                layerIndex < activatedThroughLayer;
            ++layerIndex)
        {
            const MiaIA::Core::ForwardTraceLayerSnapshot& layer =
                state.Trace.Layers[layerIndex];

            for (const MiaIA::Core::ForwardTraceNeuronSnapshot& neuron :
                layer.Neurons)
            {
                activations.Add(
                    static_cast<int64>(neuron.Id),
                    neuron.Activation);
            }
        }

        if (playbackComplete && state.HasContributionPage)
        {
            for (const auto& contribution :
                state.ContributionPage.Contributions)
            {
                contributions.Add(
                    static_cast<int64>(contribution.ConnectionId),
                    contribution.Contribution);
            }
        }
    }

    if (NetworkView.IsValid())
    {
        NetworkView->SetForwardTraceOverlay(
            activations,
            contributions,
            playbackConnections,
            playbackActive);
    }

    if (Network3DView.IsValid())
    {
        Network3DView->SetForwardTraceOverlay(
            activations,
            contributions,
            playbackConnections,
            playbackActive);
    }
}

void SMiaIAEditorPanel::RebuildForwardTrace()
{
    if (!ForwardTraceContent.IsValid())
    {
        return;
    }

    ForwardTraceContent->ClearChildren();
    const MiaIA::Studio::StudioForwardTraceState state =
        FMiaIAInstanceService::ForwardTraceState(MiaIAInstance);

    if (!state.Active)
    {
        ForwardTraceContent->AddSlot()
        .AutoHeight()
        .Padding(3.0f)
        [
            SNew(STextBlock)
            .Text(LOCTEXT(
                "ForwardTraceInactive",
                "Enter one value per input neuron, then run an immutable forward trace."))
            .AutoWrapText(true)
        ];
        return;
    }

    if (!state.HasContributionPage)
    {
        ForwardTraceContent->AddSlot()
        .AutoHeight()
        .Padding(3.0f)
        [
            SNew(STextBlock)
            .Text(LOCTEXT(
                "ForwardTraceSelectNeuron",
                "Select one neuron to inspect its exact incoming contributions."))
            .AutoWrapText(true)
        ];
        return;
    }

    ForwardTraceContent->AddSlot()
    .AutoHeight()
    .Padding(3.0f, 2.0f, 3.0f, 6.0f)
    [
        SNew(STextBlock)
        .Text(FText::FromString(FString::Printf(
            TEXT("Neuron #%lld | contributions %lld-%lld of %lld"),
            static_cast<int64>(state.FocusedNeuronId),
            static_cast<int64>(state.ContributionPage.Offset +
                (state.ContributionPage.Contributions.empty() ? 0 : 1)),
            static_cast<int64>(state.ContributionPage.Offset +
                state.ContributionPage.Contributions.size()),
            static_cast<int64>(
                state.ContributionPage.FilteredContributionCount))))
        .Font(FAppStyle::GetFontStyle(TEXT("SmallFontBold")))
    ];

    for (const auto& contribution : state.ContributionPage.Contributions)
    {
        const int64 connectionId =
            static_cast<int64>(contribution.ConnectionId);
        ForwardTraceContent->AddSlot()
        .AutoHeight()
        .Padding(3.0f, 1.0f)
        [
            SNew(SButton)
            .ButtonStyle(&ExplorerButtonStyle)
            .OnClicked_Lambda([this, connectionId]()
            {
                SelectConnection(connectionId);
                return FReply::Handled();
            })
            [
                SNew(STextBlock)
                .Text(FText::FromString(FString::Printf(
                    TEXT("#%lld  |  #%lld -> #%lld  |  %g x %g = %g"),
                    connectionId,
                    static_cast<int64>(contribution.FromNeuron),
                    static_cast<int64>(contribution.ToNeuron),
                    contribution.SourceActivation,
                    contribution.Weight,
                    contribution.Contribution)))
            ]
        ];
    }
}

void SMiaIAEditorPanel::RebuildBreakpoints()
{
    if (!BreakpointContent.IsValid())
    {
        return;
    }

    BreakpointContent->ClearChildren();

    if (Breakpoints.IsEmpty())
    {
        BreakpointContent->AddSlot()
        .AutoHeight()
        .Padding(4.0f)
        [
            SNew(STextBlock)
            .Text(LOCTEXT(
                "NoBreakpoints",
                "No breakpoints configured. Add one above or use the shared CLI."))
            .AutoWrapText(true)
        ];
        return;
    }

    for (const FMiaIATrainingBreakpoint& breakpoint : Breakpoints)
    {
        FText description;

        if (breakpoint.Kind == EMiaIATrainingBreakpointKind::Phase)
        {
            description = FText::Format(
                LOCTEXT(
                    "PhaseBreakpointDescription",
                    "#{0}  {1}  |  {2}  |  hits {3}"),
                FText::AsNumber(breakpoint.Id),
                BreakpointKindName(breakpoint.Kind),
                DebugPhaseName(breakpoint.Phase),
                FText::AsNumber(breakpoint.HitCount));
        }
        else
        {
            description = FText::Format(
                LOCTEXT(
                    "ValueBreakpointDescription",
                    "#{0}  {1}  |  target {2}  |  threshold {3}  |  hits {4}"),
                FText::AsNumber(breakpoint.Id),
                BreakpointKindName(breakpoint.Kind),
                FText::AsNumber(breakpoint.TargetId),
                FText::AsNumber(breakpoint.Threshold),
                FText::AsNumber(breakpoint.HitCount));
        }

        BreakpointContent->AddSlot()
        .AutoHeight()
        .Padding(2.0f)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(description)
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(4.0f, 0.0f)
            [
                SNew(SButton)
                .ButtonStyle(&ButtonStyle)
                .Text(breakpoint.bEnabled
                    ? LOCTEXT("DisableBreakpoint", "Disable")
                    : LOCTEXT("EnableBreakpoint", "Enable"))
                .OnClicked(
                    this,
                    &SMiaIAEditorPanel::HandleToggleBreakpoint,
                    breakpoint.Id,
                    !breakpoint.bEnabled)
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            [
                SNew(SButton)
                .ButtonStyle(&ButtonStyle)
                .Text(LOCTEXT("RemoveBreakpoint", "Remove"))
                .OnClicked(
                    this,
                    &SMiaIAEditorPanel::HandleRemoveBreakpoint,
                    breakpoint.Id)
            ]
        ];
    }
}

void SMiaIAEditorPanel::RebuildRelationshipExplorer()
{
    if (!RelationshipContent.IsValid())
    {
        return;
    }

    RelationshipContent->ClearChildren();

    if (!bHasNeuronInspection)
    {
        return;
    }

    if (!bHasRelationshipPage)
    {
        RelationshipContent->AddSlot()
        .AutoHeight()
        .Padding(2.0f)
        [
            SNew(STextBlock)
            .Text(LOCTEXT(
                "RelationshipPageUnavailable",
                "The relationship page is unavailable."))
            .AutoWrapText(true)
        ];
        return;
    }

    const int64 pageNumber = RelationshipPage.Limit > 0
        ? RelationshipPage.Offset / RelationshipPage.Limit + 1
        : 1;
    const int64 pageCount = RelationshipPage.Limit > 0 &&
        RelationshipPage.FilteredConnectionCount > 0
        ? (RelationshipPage.FilteredConnectionCount +
            RelationshipPage.Limit - 1) / RelationshipPage.Limit
        : 1;

    RelationshipContent->AddSlot()
    .AutoHeight()
    .Padding(0.0f, 3.0f, 0.0f, 5.0f)
    [
        SNew(STextBlock)
        .Text(FText::Format(
            LOCTEXT(
                "RelationshipPageSummary",
                "{0} | Page {1} of {2} | {3} filtered of {4}"),
            RelationshipDirection ==
                EMiaIANeuronRelationshipDirection::Incoming
                ? LOCTEXT("IncomingRelationshipSummary", "Incoming")
                : LOCTEXT("OutgoingRelationshipSummary", "Outgoing"),
            FText::AsNumber(pageNumber),
            FText::AsNumber(pageCount),
            FText::AsNumber(RelationshipPage.FilteredConnectionCount),
            FText::AsNumber(RelationshipPage.TotalConnectionCount)))
    ];

    for (const FMiaIAConnectionSnapshot& connection :
        RelationshipPage.Connections)
    {
        const int64 connectionId = connection.Id;
        const int64 relatedNeuronId = RelationshipDirection ==
            EMiaIANeuronRelationshipDirection::Incoming
            ? connection.FromNeuron
            : connection.ToNeuron;

        RelationshipContent->AddSlot()
        .AutoHeight()
        .Padding(0.0f, 1.0f)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            [
                SNew(SButton)
                .ButtonStyle(&ButtonStyle)
                .Text(FText::Format(
                    LOCTEXT(
                        "RelationshipConnectionEntry",
                        "#{0}: #{1} -> #{2} | {3}"),
                    FText::AsNumber(connection.Id),
                    FText::AsNumber(connection.FromNeuron),
                    FText::AsNumber(connection.ToNeuron),
                    FText::AsNumber(connection.Weight)))
                .ToolTipText(LOCTEXT(
                    "SelectRelationshipConnectionTooltip",
                    "Select this connection for Inspector details. Its geometry is highlighted when the connection is loaded in the current topology."))
                .OnClicked(
                    this,
                    &SMiaIAEditorPanel::
                        HandleSelectRelationshipConnection,
                    connectionId)
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(3.0f, 0.0f, 0.0f, 0.0f)
            [
                SNew(SButton)
                .ButtonStyle(&ButtonStyle)
                .Text(FText::Format(
                    LOCTEXT("OpenRelatedNeuron", "Neuron #{0}"),
                    FText::AsNumber(relatedNeuronId)))
                .ToolTipText(LOCTEXT(
                    "OpenRelatedNeuronTooltip",
                    "Navigate to the neuron at the other end of this connection."))
                .OnClicked(
                    this,
                    &SMiaIAEditorPanel::HandleNavigateRelationshipNeuron,
                    relatedNeuronId)
            ]
        ];
    }

    if (RelationshipPage.Connections.IsEmpty())
    {
        RelationshipContent->AddSlot()
        .AutoHeight()
        .Padding(2.0f, 4.0f)
        [
            SNew(STextBlock)
            .Text(LOCTEXT(
                "NoFilteredRelationships",
                "No connections match the current direction and filter."))
            .AutoWrapText(true)
        ];
    }

    RelationshipContent->AddSlot()
    .AutoHeight()
    .Padding(0.0f, 5.0f, 0.0f, 0.0f)
    [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot()
        .FillWidth(1.0f)
        .Padding(0.0f, 0.0f, 2.0f, 0.0f)
        [
            SNew(SButton)
            .ButtonStyle(&ButtonStyle)
            .IsEnabled(RelationshipPage.bHasPrevious)
            .Text(LOCTEXT("PreviousRelationshipPage", "Previous"))
            .OnClicked(
                this,
                &SMiaIAEditorPanel::HandlePreviousRelationshipPage)
        ]
        + SHorizontalBox::Slot()
        .FillWidth(1.0f)
        .Padding(2.0f, 0.0f, 0.0f, 0.0f)
        [
            SNew(SButton)
            .ButtonStyle(&ButtonStyle)
            .IsEnabled(RelationshipPage.bHasNext)
            .Text(LOCTEXT("NextRelationshipPage", "Next"))
            .OnClicked(
                this,
                &SMiaIAEditorPanel::HandleNextRelationshipPage)
        ]
    ];
}

void SMiaIAEditorPanel::RebuildExplorer()
{
    if (!ExplorerContent.IsValid())
    {
        return;
    }

    ExplorerContent->ClearChildren();

    if (!bCompactTopology)
    {
        for (auto iterator = ExpandedExplorerLayerIds.CreateIterator();
            iterator;
            ++iterator)
        {
            const int64 layerId = *iterator;
            if (!Network.Layers.ContainsByPredicate(
                [layerId](const FMiaIALayerSnapshot& layer)
                {
                    return layer.Id == layerId;
                }))
            {
                iterator.RemoveCurrent();
            }
        }

        if (!bExplorerExpansionInitialized)
        {
            bExplorerExpansionInitialized = true;

            if (!ExpandExplorerForNeuron(SelectedNeuronId))
            {
                const FMiaIALayerSnapshot* firstLayer =
                    Network.Layers.FindByPredicate(
                    [](const FMiaIALayerSnapshot& layer)
                    {
                        return !layer.Neurons.IsEmpty();
                    });

                if (firstLayer != nullptr)
                {
                    ExpandedExplorerLayerIds.Add(firstLayer->Id);
                }
            }
        }
    }

    ExplorerContent->AddSlot()
    .AutoHeight()
    .Padding(2.0f, 4.0f, 2.0f, 8.0f)
    [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot()
        .FillWidth(1.0f)
        .VAlign(VAlign_Center)
        [
            SNew(STextBlock)
            .Text(LOCTEXT("Explorer", "Model explorer"))
            .Font(FAppStyle::GetFontStyle(TEXT("NormalFontBold")))
        ]
        + SHorizontalBox::Slot()
        .AutoWidth()
        .Padding(3.0f, 0.0f)
        [
            SNew(SButton)
            .Visibility(bCompactTopology
                ? EVisibility::Collapsed
                : EVisibility::Visible)
            .ButtonStyle(&ExplorerButtonStyle)
            .ContentPadding(FMargin(4.0f, 2.0f))
            .Text(LOCTEXT("ExpandExplorerAll", "Expand"))
            .ToolTipText(LOCTEXT(
                "ExpandExplorerAllTooltip",
                "Expand every layer and the connection list."))
            .OnClicked(
                this,
                &SMiaIAEditorPanel::HandleExpandAllExplorer)
        ]
        + SHorizontalBox::Slot()
        .AutoWidth()
        [
            SNew(SButton)
            .Visibility(bCompactTopology
                ? EVisibility::Collapsed
                : EVisibility::Visible)
            .ButtonStyle(&ExplorerButtonStyle)
            .ContentPadding(FMargin(4.0f, 2.0f))
            .Text(LOCTEXT("CollapseExplorerAll", "Collapse"))
            .ToolTipText(LOCTEXT(
                "CollapseExplorerAllTooltip",
                "Collapse every layer and the connection list."))
            .OnClicked(
                this,
                &SMiaIAEditorPanel::HandleCollapseAllExplorer)
        ]
    ];

    if (bCompactTopology)
    {
        ExplorerContent->AddSlot()
        .AutoHeight()
        .Padding(2.0f, 0.0f, 2.0f, 8.0f)
        [
            SNew(STextBlock)
            .Text(LOCTEXT(
                "CompactExplorerNotice",
                "Compact mode keeps large models responsive. Element-level entries are hidden."))
            .AutoWrapText(true)
            .ColorAndOpacity_Lambda([this]()
            {
                return FSlateColor(
                    FMiaIAEditorTheme::Palette(Theme).SubduedText);
            })
        ];

        if (bNetworkPreview)
        {
            ExplorerContent->AddSlot()
            .AutoHeight()
            .Padding(2.0f, 3.0f)
            [
                SNew(STextBlock)
                .Text(FText::Format(
                    LOCTEXT(
                        "CompactNetworkEntry",
                        "Network  |  {0} inputs  |  {1} layers  |  {2} outputs"),
                    FText::AsNumber(NetworkInputCount()),
                    FText::AsNumber(NetworkOverview.Layers.Num()),
                    FText::AsNumber(NetworkOutputCount())))
                .AutoWrapText(true)
            ];

            ExplorerContent->AddSlot()
            .AutoHeight()
            .Padding(2.0f, 5.0f)
            [
                SNew(STextBlock)
                .Text(LOCTEXT(
                    "CompactNetworkOpenHint",
                    "Click the central network node or press Enter to inspect its layers."))
                .AutoWrapText(true)
                .ColorAndOpacity_Lambda([this]()
                {
                    return FSlateColor(
                        FMiaIAEditorTheme::Palette(Theme).SubduedText);
                })
            ];
            return;
        }

        for (const FMiaIALayerOverview& layer : NetworkOverview.Layers)
        {
            ExplorerContent->AddSlot()
            .AutoHeight()
            .Padding(2.0f, 3.0f)
            [
                SNew(STextBlock)
                .Text(FText::Format(
                    LOCTEXT(
                        "CompactLayerEntry",
                        "L{0}  |  {1}  |  {2} neurons"),
                    FText::AsNumber(layer.Order),
                    FText::FromString(layer.Name),
                    FText::AsNumber(layer.NeuronCount)))
            ];
        }

        return;
    }

    for (const FMiaIALayerSnapshot& layer : Network.Layers)
    {
        const bool layerExpanded =
            ExpandedExplorerLayerIds.Contains(layer.Id);
        const int64 layerId = layer.Id;

        ExplorerContent->AddSlot()
        .AutoHeight()
        .Padding(2.0f, 3.0f)
        [
            SNew(SButton)
            .ButtonStyle(&ExplorerButtonStyle)
            .ContentPadding(FMargin(4.0f, 3.0f))
            .OnClicked(
                this,
                &SMiaIAEditorPanel::HandleToggleExplorerLayer,
                layerId)
            [
            SNew(STextBlock)
            .Text(FText::Format(
                LOCTEXT("LayerTreeEntry", "{0}  {1}  |  {2} neurons"),
                layerExpanded
                    ? LOCTEXT("ExpandedTreeMarker", "[-]")
                    : LOCTEXT("CollapsedTreeMarker", "[+]"),
                FText::FromString(layer.Name),
                FText::AsNumber(layer.Neurons.Num())))
            ]
        ];

        if (layerExpanded)
        {
            for (const FMiaIANeuronSnapshot& neuron : layer.Neurons)
            {
                const int64 neuronId = neuron.Id;
                ExplorerContent->AddSlot()
                .AutoHeight()
                .Padding(16.0f, 1.0f, 2.0f, 1.0f)
                [
                    SNew(SButton)
                    .ButtonStyle(&ExplorerButtonStyle)
                    .ContentPadding(FMargin(4.0f, 2.0f))
                    .OnClicked_Lambda([this, neuronId]()
                    {
                        SelectNeuron(neuronId);
                        return FReply::Handled();
                    })
                    [
                        SNew(STextBlock)
                        .Text(FText::Format(
                            LOCTEXT("NeuronTreeEntry", "- Neuron #{0}"),
                            FText::AsNumber(neuronId)))
                    ]
                ];
            }
        }
    }

    if (!Network.Connections.IsEmpty())
    {
        ExplorerContent->AddSlot()
        .AutoHeight()
        .Padding(2.0f, 10.0f, 2.0f, 5.0f)
        [
            SNew(SButton)
            .ButtonStyle(&ExplorerButtonStyle)
            .ContentPadding(FMargin(4.0f, 3.0f))
            .OnClicked(
                this,
                &SMiaIAEditorPanel::HandleToggleExplorerConnections)
            [
                SNew(STextBlock)
                .Text(FText::Format(
                    LOCTEXT(
                        "ConnectionsTreeEntry",
                        "{0}  Connections  |  {1}"),
                    bExplorerConnectionsExpanded
                        ? LOCTEXT("ExpandedConnectionsMarker", "[-]")
                        : LOCTEXT("CollapsedConnectionsMarker", "[+]"),
                    FText::AsNumber(Network.Connections.Num())))
            ]
        ];

        if (bExplorerConnectionsExpanded)
        {
            for (const FMiaIAConnectionSnapshot& connection :
                Network.Connections)
            {
                const int64 connectionId = connection.Id;
                const int64 fromNeuron = connection.FromNeuron;
                const int64 toNeuron = connection.ToNeuron;
                ExplorerContent->AddSlot()
                .AutoHeight()
                .Padding(16.0f, 1.0f, 2.0f, 1.0f)
                [
                    SNew(SButton)
                    .ButtonStyle(&ExplorerButtonStyle)
                    .ContentPadding(FMargin(4.0f, 2.0f))
                    .OnClicked_Lambda([this, connectionId]()
                    {
                        SelectConnection(connectionId);
                        return FReply::Handled();
                    })
                    [
                        SNew(STextBlock)
                        .Text(FText::Format(
                            LOCTEXT(
                                "ConnectionTreeChildEntry",
                                "- #{0}: {1} -> {2}"),
                            FText::AsNumber(connectionId),
                            FText::AsNumber(fromNeuron),
                            FText::AsNumber(toNeuron)))
                    ]
                ];
            }
        }
    }
}

bool SMiaIAEditorPanel::ExpandExplorerForNeuron(int64 NeuronId)
{
    if (NeuronId < 0)
    {
        return false;
    }

    for (const FMiaIALayerSnapshot& layer : Network.Layers)
    {
        if (layer.Neurons.ContainsByPredicate(
            [NeuronId](const FMiaIANeuronSnapshot& neuron)
            {
                return neuron.Id == NeuronId;
            }))
        {
            if (ExpandedExplorerLayerIds.Contains(layer.Id))
            {
                return false;
            }

            ExpandedExplorerLayerIds.Add(layer.Id);
            return true;
        }
    }

    return false;
}

bool SMiaIAEditorPanel::ExpandExplorerForNeurons(
    const TSet<int64>& NeuronIds)
{
    bool changed = false;

    for (const FMiaIALayerSnapshot& layer : Network.Layers)
    {
        if (!ExpandedExplorerLayerIds.Contains(layer.Id) &&
            layer.Neurons.ContainsByPredicate(
            [&NeuronIds](const FMiaIANeuronSnapshot& neuron)
            {
                return NeuronIds.Contains(neuron.Id);
            }))
        {
            ExpandedExplorerLayerIds.Add(layer.Id);
            changed = true;
        }
    }

    return changed;
}

FReply SMiaIAEditorPanel::HandleToggleExplorerLayer(int64 LayerId)
{
    if (ExpandedExplorerLayerIds.Contains(LayerId))
    {
        ExpandedExplorerLayerIds.Remove(LayerId);
    }
    else
    {
        ExpandedExplorerLayerIds.Add(LayerId);
    }

    RebuildExplorer();
    return FReply::Handled();
}

FReply SMiaIAEditorPanel::HandleToggleExplorerConnections()
{
    bExplorerConnectionsExpanded = !bExplorerConnectionsExpanded;
    RebuildExplorer();
    return FReply::Handled();
}

FReply SMiaIAEditorPanel::HandleExpandAllExplorer()
{
    for (const FMiaIALayerSnapshot& layer : Network.Layers)
    {
        ExpandedExplorerLayerIds.Add(layer.Id);
    }

    bExplorerConnectionsExpanded = !Network.Connections.IsEmpty();
    bExplorerExpansionInitialized = true;
    RebuildExplorer();
    return FReply::Handled();
}

FReply SMiaIAEditorPanel::HandleCollapseAllExplorer()
{
    ExpandedExplorerLayerIds.Reset();
    bExplorerConnectionsExpanded = false;
    bExplorerExpansionInitialized = true;
    RebuildExplorer();
    return FReply::Handled();
}

void SMiaIAEditorPanel::SelectNeuron(int64 NeuronId)
{
    if (SelectedNeuronId != NeuronId)
    {
        RelationshipOffset = 0;
    }

    SelectedLayerId = -1;
    SelectedNeuronId = NeuronId;
    SelectedNeuronIds.Reset();

    if (NeuronId >= 0)
    {
        SelectedNeuronIds.Add(NeuronId);
    }

    SelectedConnectionId = -1;
    const bool explorerChanged = ExpandExplorerForNeuron(NeuronId);
    bExplorerExpansionInitialized = true;
    RefreshData();

    if (explorerChanged)
    {
        RebuildExplorer();
    }
}

void SMiaIAEditorPanel::SelectNeurons(
    const TSet<int64>& NeuronIds,
    int64 PrimaryNeuronId)
{
    if (SelectedNeuronIds.Num() != 1 ||
        !SelectedNeuronIds.Contains(PrimaryNeuronId) ||
        SelectedNeuronId != PrimaryNeuronId)
    {
        RelationshipOffset = 0;
    }

    SelectedLayerId = -1;
    SelectedNeuronIds = NeuronIds;
    SelectedNeuronId = SelectedNeuronIds.Contains(PrimaryNeuronId)
        ? PrimaryNeuronId
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
    const bool explorerChanged =
        ExpandExplorerForNeurons(SelectedNeuronIds);
    bExplorerExpansionInitialized = true;
    RefreshData();

    if (explorerChanged)
    {
        RebuildExplorer();
    }
}

void SMiaIAEditorPanel::NavigateNeuron(
    EMiaIANeuronNavigationDirection Direction)
{
    if (bCompactTopology || Network.Layers.IsEmpty())
    {
        return;
    }

    int32 currentLayerIndex = INDEX_NONE;
    int32 currentNeuronIndex = INDEX_NONE;

    for (int32 layerIndex = 0;
        layerIndex < Network.Layers.Num() && currentLayerIndex == INDEX_NONE;
        ++layerIndex)
    {
        const FMiaIALayerSnapshot& layer = Network.Layers[layerIndex];

        for (int32 neuronIndex = 0;
            neuronIndex < layer.Neurons.Num();
            ++neuronIndex)
        {
            if (layer.Neurons[neuronIndex].Id == SelectedNeuronId)
            {
                currentLayerIndex = layerIndex;
                currentNeuronIndex = neuronIndex;
                break;
            }
        }
    }

    if (currentLayerIndex == INDEX_NONE)
    {
        for (int32 layerIndex = 0;
            layerIndex < Network.Layers.Num();
            ++layerIndex)
        {
            if (!Network.Layers[layerIndex].Neurons.IsEmpty())
            {
                currentLayerIndex = layerIndex;
                currentNeuronIndex = 0;
                break;
            }
        }
    }

    if (currentLayerIndex == INDEX_NONE)
    {
        return;
    }

    int32 targetLayerIndex = currentLayerIndex;
    int32 targetNeuronIndex = currentNeuronIndex;
    const FMiaIALayerSnapshot& currentLayer =
        Network.Layers[currentLayerIndex];

    if (Direction == EMiaIANeuronNavigationDirection::PreviousNeuron)
    {
        targetNeuronIndex = FMath::Max(0, currentNeuronIndex - 1);
    }
    else if (Direction == EMiaIANeuronNavigationDirection::NextNeuron)
    {
        targetNeuronIndex = FMath::Min(
            currentLayer.Neurons.Num() - 1,
            currentNeuronIndex + 1);
    }
    else
    {
        const int32 layerStep =
            Direction == EMiaIANeuronNavigationDirection::PreviousLayer
            ? -1
            : 1;
        targetLayerIndex += layerStep;

        while (Network.Layers.IsValidIndex(targetLayerIndex) &&
            Network.Layers[targetLayerIndex].Neurons.IsEmpty())
        {
            targetLayerIndex += layerStep;
        }

        if (!Network.Layers.IsValidIndex(targetLayerIndex))
        {
            return;
        }

        const FMiaIALayerSnapshot& targetLayer =
            Network.Layers[targetLayerIndex];
        const double normalizedPosition = currentLayer.Neurons.Num() > 1
            ? static_cast<double>(currentNeuronIndex) /
                static_cast<double>(currentLayer.Neurons.Num() - 1)
            : 0.5;
        targetNeuronIndex = FMath::Clamp(
            FMath::RoundToInt(
                normalizedPosition *
                static_cast<double>(targetLayer.Neurons.Num() - 1)),
            0,
            targetLayer.Neurons.Num() - 1);
    }

    const int64 targetNeuronId = Network.Layers[targetLayerIndex]
        .Neurons[targetNeuronIndex]
        .Id;
    TSet<int64> selection;
    selection.Add(targetNeuronId);
    SelectNeurons(selection, targetNeuronId);

    if (NetworkView.IsValid())
    {
        NetworkView->RevealNeuron(targetNeuronId);
    }

    if (Network3DView.IsValid())
    {
        Network3DView->RevealNeuron(targetNeuronId);
    }
}

void SMiaIAEditorPanel::SelectConnection(int64 ConnectionId)
{
    SelectedLayerId = -1;
    SelectedConnectionId = ConnectionId;
    SelectedNeuronId = -1;
    SelectedNeuronIds.Reset();
    bool explorerChanged = false;

    if (ConnectionId >= 0)
    {
        explorerChanged = !bExplorerConnectionsExpanded;
        bExplorerConnectionsExpanded = true;
        bExplorerExpansionInitialized = true;
    }

    RefreshData();

    if (explorerChanged)
    {
        RebuildExplorer();
    }
}

void SMiaIAEditorPanel::SelectLayer(int64 LayerId)
{
    SelectedLayerId = FindOverviewLayer(LayerId) ? LayerId : -1;
    SelectedConnectionId = -1;
    SelectedNeuronId = -1;
    SelectedNeuronIds.Reset();
    RefreshData();
}

FReply SMiaIAEditorPanel::SelectRelationshipDirection(
    EMiaIANeuronRelationshipDirection InDirection)
{
    RelationshipDirection = InDirection;
    RelationshipOffset = 0;
    RefreshData();
    return FReply::Handled();
}

TSharedRef<SWidget> SMiaIAEditorPanel::BuildRelationshipSortMenu()
{
    return SNew(SVerticalBox)
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SButton)
            .ButtonStyle(&ButtonStyle)
            .Text(LOCTEXT("RelationshipSortId", "Connection ID"))
            .OnClicked(
                this,
                &SMiaIAEditorPanel::SelectRelationshipSort,
                EMiaIANeuronRelationshipSort::ConnectionId)
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SButton)
            .ButtonStyle(&ButtonStyle)
            .Text(LOCTEXT("RelationshipSortWeight", "Weight"))
            .OnClicked(
                this,
                &SMiaIAEditorPanel::SelectRelationshipSort,
                EMiaIANeuronRelationshipSort::Weight)
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SButton)
            .ButtonStyle(&ButtonStyle)
            .Text(LOCTEXT(
                "RelationshipSortAbsoluteWeight",
                "Absolute weight"))
            .OnClicked(
                this,
                &SMiaIAEditorPanel::SelectRelationshipSort,
                EMiaIANeuronRelationshipSort::AbsoluteWeight)
        ];
}

FReply SMiaIAEditorPanel::SelectRelationshipSort(
    EMiaIANeuronRelationshipSort InSort)
{
    RelationshipSort = InSort;
    RelationshipOffset = 0;
    FSlateApplication::Get().DismissAllMenus();
    RefreshData();
    return FReply::Handled();
}

FReply SMiaIAEditorPanel::ToggleRelationshipSortDirection()
{
    bRelationshipSortDescending = !bRelationshipSortDescending;
    RelationshipOffset = 0;
    RefreshData();
    return FReply::Handled();
}

void SMiaIAEditorPanel::HandleRelationshipMinimumWeightCommitted(
    double InValue,
    ETextCommit::Type CommitType)
{
    if (CommitType == ETextCommit::OnCleared ||
        !FMath::IsFinite(InValue))
    {
        return;
    }

    RelationshipMinimumAbsoluteWeight = FMath::Max(0.0, InValue);
    RelationshipOffset = 0;
    RefreshData();
}

FReply SMiaIAEditorPanel::HandlePreviousRelationshipPage()
{
    RelationshipOffset = FMath::Max<int64>(
        0,
        RelationshipOffset - FMath::Max(1, InspectorConnectionLimit));
    RefreshData();
    return FReply::Handled();
}

FReply SMiaIAEditorPanel::HandleNextRelationshipPage()
{
    if (bHasRelationshipPage && RelationshipPage.bHasNext)
    {
        RelationshipOffset += FMath::Max(1, InspectorConnectionLimit);
        RefreshData();
    }

    return FReply::Handled();
}

FReply SMiaIAEditorPanel::HandleSelectRelationshipConnection(
    int64 ConnectionId)
{
    SelectConnection(ConnectionId);
    return FReply::Handled();
}

FReply SMiaIAEditorPanel::HandleNavigateRelationshipNeuron(int64 NeuronId)
{
    if (!FindNeuron(NeuronId) && bNetworkRequiresCompactTopology)
    {
        FMiaIANeuronInspection target;
        if (!UMiaIABlueprintLibrary::InspectNeuron(NeuronId, 0, target))
        {
            return FReply::Handled();
        }

        FocusedLayerId = target.Context.LayerId;
        bNetworkPreview = false;
        RefreshData();
    }

    if (FindNeuron(NeuronId))
    {
        SelectNeuron(NeuronId);

        if (NetworkView.IsValid())
        {
            NetworkView->RevealNeuron(NeuronId);
        }

        if (Network3DView.IsValid())
        {
            Network3DView->RevealNeuron(NeuronId);
        }
    }

    return FReply::Handled();
}

FReply SMiaIAEditorPanel::HandleNavigateSelectedConnectionEndpoint(
    bool bToNeuron)
{
    const FMiaIAConnectionSnapshot* connection =
        FindConnection(SelectedConnectionId);
    if (connection == nullptr)
    {
        return FReply::Handled();
    }

    return HandleNavigateRelationshipNeuron(
        bToNeuron ? connection->ToNeuron : connection->FromNeuron);
}

void SMiaIAEditorPanel::OpenNetworkFromPreview()
{
    if (!bNetworkPreview || NetworkOverview.Layers.IsEmpty())
    {
        return;
    }

    bNetworkPreview = false;
    SelectedLayerId = -1;
    SelectedConnectionId = -1;
    SelectedNeuronId = -1;
    SelectedNeuronIds.Reset();
    ExpandedExplorerLayerIds.Reset();
    bExplorerConnectionsExpanded = false;
    bExplorerExpansionInitialized = false;
    RefreshData();
    RegisterActiveTimer(
        0.05f,
        FWidgetActiveTimerDelegate::CreateSP(
            this,
            &SMiaIAEditorPanel::HandleDeferredWorkspaceFit));
}

void SMiaIAEditorPanel::OpenLayerDetail(int64 LayerId)
{
    if (!bCompactTopology || bNetworkPreview || LayerId < 0 ||
        !NetworkOverview.Layers.ContainsByPredicate(
            [LayerId](const FMiaIALayerOverview& layer)
            {
                return layer.Id == LayerId;
            }))
    {
        return;
    }

    FocusedLayerId = LayerId;
    SelectedLayerId = -1;
    SelectedConnectionId = -1;
    SelectedNeuronId = -1;
    SelectedNeuronIds.Reset();
    ExpandedExplorerLayerIds.Reset();
    bExplorerConnectionsExpanded = false;
    bExplorerExpansionInitialized = false;
    RefreshData();
    RegisterActiveTimer(
        0.05f,
        FWidgetActiveTimerDelegate::CreateSP(
            this,
            &SMiaIAEditorPanel::HandleDeferredWorkspaceFit));
}

bool SMiaIAEditorPanel::NavigateBackFromTopology()
{
    if (FocusedLayerId >= 0)
    {
        FocusedLayerId = -1;
        bNetworkPreview = false;
    }
    else if (!bNetworkPreview && !NetworkOverview.Layers.IsEmpty())
    {
        bNetworkPreview = true;
    }
    else
    {
        return false;
    }

    SelectedLayerId = -1;
    SelectedConnectionId = -1;
    SelectedNeuronId = -1;
    SelectedNeuronIds.Reset();
    ExpandedExplorerLayerIds.Reset();
    bExplorerConnectionsExpanded = false;
    bExplorerExpansionInitialized = false;
    RefreshData();
    RegisterActiveTimer(
        0.05f,
        FWidgetActiveTimerDelegate::CreateSP(
            this,
            &SMiaIAEditorPanel::HandleDeferredWorkspaceFit));
    return true;
}

FReply SMiaIAEditorPanel::HandleNavigateBackFromTopology()
{
    NavigateBackFromTopology();
    return FReply::Handled();
}

const FMiaIANeuronSnapshot* SMiaIAEditorPanel::FindNeuron(
    int64 NeuronId) const
{
    for (const FMiaIALayerSnapshot& layer : Network.Layers)
    {
        if (const FMiaIANeuronSnapshot* neuron =
            layer.Neurons.FindByPredicate(
                [NeuronId](const FMiaIANeuronSnapshot& candidate)
                {
                    return candidate.Id == NeuronId;
                }))
        {
            return neuron;
        }
    }

    return nullptr;
}

const FMiaIAConnectionSnapshot* SMiaIAEditorPanel::FindConnection(
    int64 ConnectionId) const
{
    const FMiaIAConnectionSnapshot* connection =
        Network.Connections.FindByPredicate(
        [ConnectionId](const FMiaIAConnectionSnapshot& connection)
        {
            return connection.Id == ConnectionId;
        });

    if (connection != nullptr)
    {
        return connection;
    }

    return bHasConnectionInspection &&
        ConnectionInspection.Connection.Id == ConnectionId
        ? &ConnectionInspection.Connection
        : nullptr;
}

const FMiaIALayerOverview* SMiaIAEditorPanel::FindOverviewLayer(
    int64 LayerId) const
{
    return NetworkOverview.Layers.FindByPredicate(
        [LayerId](const FMiaIALayerOverview& layer)
        {
            return layer.Id == LayerId;
        });
}

EActiveTimerReturnType SMiaIAEditorPanel::HandleRefreshTimer(
    double,
    float DeltaTime)
{
    if (FMiaIAInstanceService::AdvanceForwardTracePlayback(
        MiaIAInstance,
        DeltaTime))
    {
        RebuildForwardTrace();
        ApplyForwardTraceOverlay();
    }

    PeriodicRefreshElapsedSeconds += DeltaTime;

    if (PeriodicRefreshElapsedSeconds >= DataRefreshInterval())
    {
        RefreshData();
    }

    return EActiveTimerReturnType::Continue;
}

EActiveTimerReturnType SMiaIAEditorPanel::HandleDeferredWorkspaceFit(
    double,
    float)
{
    HandleFitView();
    return EActiveTimerReturnType::Stop;
}

FReply SMiaIAEditorPanel::HandleRefresh()
{
    RefreshData();
    return FReply::Handled();
}

TSharedRef<SWidget> SMiaIAEditorPanel::BuildLayoutMenu()
{
    return SNew(SBox)
        .WidthOverride(300.0f)
        .Padding(10.0f)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, 4.0f)
            [
                SNew(STextBlock)
                .Font(FAppStyle::GetFontStyle(TEXT("NormalFontBold")))
                .Text(LOCTEXT("LayoutPlacementHeading", "Placement"))
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .Padding(0.0f, 0.0f, 2.0f, 0.0f)
                [
                    SNew(SButton)
                    .ButtonStyle(&ButtonStyle)
                    .Text(LOCTEXT("ExpandedLayout", "Expanded"))
                    .ToolTipText(LOCTEXT(
                        "ExpandedLayoutTooltip",
                        "Keep layers open and readable while preserving a non-overlapping minimum distance."))
                    .OnClicked(
                        this,
                        &SMiaIAEditorPanel::SelectLayoutMode,
                        MiaIA::Studio::StudioLayoutMode::Expanded)
                ]
                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .Padding(2.0f, 0.0f, 0.0f, 0.0f)
                [
                    SNew(SButton)
                    .ButtonStyle(&ButtonStyle)
                    .Text(LOCTEXT("PackedLayout", "Packed"))
                    .ToolTipText(LOCTEXT(
                        "PackedLayoutTooltip",
                        "Pack symmetric layers and neurons as closely as the configured gaps allow."))
                    .OnClicked(
                        this,
                        &SMiaIAEditorPanel::SelectLayoutMode,
                        MiaIA::Studio::StudioLayoutMode::Packed)
                ]
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 10.0f, 0.0f, 4.0f)
            [
                SNew(STextBlock)
                .Font(FAppStyle::GetFontStyle(TEXT("NormalFontBold")))
                .Text(LOCTEXT("LayoutOrientationHeading", "Flow direction"))
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .Padding(0.0f, 0.0f, 2.0f, 0.0f)
                [
                    SNew(SButton)
                    .ButtonStyle(&ButtonStyle)
                    .Text_Lambda([this]()
                    {
                        if (VisualizationSettings.Layout.Orientation !=
                            MiaIA::Studio::StudioLayoutOrientation::Horizontal)
                        {
                            return LOCTEXT(
                                "HorizontalLayout",
                                "Horizontal");
                        }

                        return VisualizationSettings.Layout.Direction ==
                            MiaIA::Studio::StudioLayoutDirection::Reverse
                            ? LOCTEXT(
                                "HorizontalReverseLayout",
                                "< Horizontal")
                            : LOCTEXT(
                                "HorizontalForwardLayout",
                                "Horizontal >");
                    })
                    .ToolTipText(LOCTEXT(
                        "HorizontalLayoutTooltip",
                        "Use horizontal flow. Click the active orientation again to reverse its layer direction."))
                    .OnClicked(
                        this,
                        &SMiaIAEditorPanel::SelectLayoutOrientation,
                        MiaIA::Studio::StudioLayoutOrientation::Horizontal)
                ]
                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .Padding(2.0f, 0.0f, 0.0f, 0.0f)
                [
                    SNew(SButton)
                    .ButtonStyle(&ButtonStyle)
                    .Text_Lambda([this]()
                    {
                        if (VisualizationSettings.Layout.Orientation !=
                            MiaIA::Studio::StudioLayoutOrientation::Vertical)
                        {
                            return LOCTEXT(
                                "VerticalLayout",
                                "Vertical");
                        }

                        return VisualizationSettings.Layout.Direction ==
                            MiaIA::Studio::StudioLayoutDirection::Reverse
                            ? LOCTEXT(
                                "VerticalReverseLayout",
                                "Vertical ^")
                            : LOCTEXT(
                                "VerticalForwardLayout",
                                "Vertical v");
                    })
                    .ToolTipText(LOCTEXT(
                        "VerticalLayoutTooltip",
                        "Use vertical flow. Click the active orientation again to reverse its layer direction."))
                    .OnClicked(
                        this,
                        &SMiaIAEditorPanel::SelectLayoutOrientation,
                        MiaIA::Studio::StudioLayoutOrientation::Vertical)
                ]
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 10.0f, 0.0f, 3.0f)
            [
                SNew(STextBlock)
                .Text(LOCTEXT("NeuronSizeLabel", "Neuron size (%)"))
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(SSpinBox<float>)
                .MinValue(25.0f)
                .MaxValue(300.0f)
                .MinSliderValue(25.0f)
                .MaxSliderValue(200.0f)
                .Delta(5.0f)
                .Value_Lambda([this]()
                {
                    return VisualizationSettings.NeuronScale * 100.0f;
                })
                .OnValueChanged(
                    this,
                    &SMiaIAEditorPanel::HandleNeuronScaleChanged)
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 8.0f, 0.0f, 3.0f)
            [
                SNew(STextBlock)
                .Text(LOCTEXT(
                    "NeuronGapLabel",
                    "Minimum neuron gap (% of diameter)"))
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(SSpinBox<float>)
                .MinValue(0.0f)
                .MaxValue(500.0f)
                .MinSliderValue(0.0f)
                .MaxSliderValue(200.0f)
                .Delta(5.0f)
                .Value_Lambda([this]()
                {
                    return static_cast<float>(
                        VisualizationSettings.Layout.NeuronGap * 100.0);
                })
                .OnValueChanged(
                    this,
                    &SMiaIAEditorPanel::HandleNeuronGapChanged)
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 8.0f, 0.0f, 3.0f)
            [
                SNew(STextBlock)
                .Text(LOCTEXT(
                    "LayerGapLabel",
                    "Minimum layer gap (% of diameter)"))
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(SSpinBox<float>)
                .MinValue(0.0f)
                .MaxValue(1000.0f)
                .MinSliderValue(0.0f)
                .MaxSliderValue(500.0f)
                .Delta(10.0f)
                .Value_Lambda([this]()
                {
                    return static_cast<float>(
                        VisualizationSettings.Layout.LayerGap * 100.0);
                })
                .OnValueChanged(
                    this,
                    &SMiaIAEditorPanel::HandleLayerGapChanged)
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 12.0f, 0.0f, 4.0f)
            [
                SNew(STextBlock)
                .Font(FAppStyle::GetFontStyle(TEXT("NormalFontBold")))
                .Text(LOCTEXT(
                    "NeuronDisplayHeading",
                    "Neuron display"))
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(SCheckBox)
                .IsChecked_Lambda([this]()
                {
                    return VisualizationSettings.bShowNeuronLabels
                        ? ECheckBoxState::Checked
                        : ECheckBoxState::Unchecked;
                })
                .OnCheckStateChanged_Lambda(
                    [this](ECheckBoxState newState)
                    {
                        VisualizationSettings.bShowNeuronLabels =
                            newState == ECheckBoxState::Checked;
                        SaveVisualizationSettings(VisualizationSettings);
                        NetworkView->SetVisualizationSettings(
                            VisualizationSettings);
                        Network3DView->SetVisualizationSettings(
                            VisualizationSettings);
                    })
                .ToolTipText(LOCTEXT(
                    "ShowNeuronLabelsTooltip",
                    "Show neuron ID capsules when detailed labels are available."))
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT(
                        "ShowNeuronLabels",
                        "Show neuron labels"))
                ]
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 12.0f, 0.0f, 4.0f)
            [
                SNew(STextBlock)
                .Font(FAppStyle::GetFontStyle(TEXT("NormalFontBold")))
                .Text(LOCTEXT(
                    "ConnectionDisplayHeading",
                    "Connections"))
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, 3.0f)
            [
                SNew(STextBlock)
                .Text(LOCTEXT(
                    "ConnectionScaleLabel",
                    "Connection visibility (%)"))
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, 8.0f)
            [
                SNew(SSpinBox<float>)
                .MinValue(0.0f)
                .MaxValue(200.0f)
                .MinSliderValue(0.0f)
                .MaxSliderValue(200.0f)
                .Delta(5.0f)
                .Value_Lambda([this]()
                {
                    return VisualizationSettings.ConnectionScale * 100.0f;
                })
                .OnValueChanged(
                    this,
                    &SMiaIAEditorPanel::HandleConnectionScaleChanged)
                .ToolTipText(LOCTEXT(
                    "ConnectionScaleTooltip",
                    "Fade and thin every visible connection independently from neuron size. Set zero to hide them."))
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                [
                    SNew(SButton)
                    .ButtonStyle(&ButtonStyle)
                    .Text(LOCTEXT("AllConnections", "All"))
                    .OnClicked(
                        this,
                        &SMiaIAEditorPanel::SelectConnectionDisplayMode,
                        EMiaIAConnectionDisplayMode::All)
                ]
                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .Padding(4.0f, 0.0f, 0.0f, 0.0f)
                [
                    SNew(SButton)
                    .ButtonStyle(&ButtonStyle)
                    .Text(LOCTEXT("SelectedConnections", "Selected"))
                    .OnClicked(
                        this,
                        &SMiaIAEditorPanel::SelectConnectionDisplayMode,
                        EMiaIAConnectionDisplayMode::Selected)
                ]
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 12.0f, 0.0f, 0.0f)
            [
                SNew(SButton)
                .ButtonStyle(&ButtonStyle)
                .Text(LOCTEXT(
                    "ResetVisualizationSettings",
                    "Reset visualization defaults"))
                .OnClicked(
                    this,
                    &SMiaIAEditorPanel::HandleResetVisualizationSettings)
            ]
        ];
}

FReply SMiaIAEditorPanel::SelectLayoutMode(
    MiaIA::Studio::StudioLayoutMode InMode)
{
    VisualizationSettings.Layout.Mode = InMode;
    SaveVisualizationSettings(VisualizationSettings);
    NetworkView->SetVisualizationSettings(VisualizationSettings);
    Network3DView->SetVisualizationSettings(VisualizationSettings);
    FSlateApplication::Get().DismissAllMenus();
    return FReply::Handled();
}

FReply SMiaIAEditorPanel::SelectLayoutOrientation(
    MiaIA::Studio::StudioLayoutOrientation InOrientation)
{
    if (VisualizationSettings.Layout.Orientation == InOrientation)
    {
        VisualizationSettings.Layout.Direction =
            VisualizationSettings.Layout.Direction ==
                MiaIA::Studio::StudioLayoutDirection::Forward
            ? MiaIA::Studio::StudioLayoutDirection::Reverse
            : MiaIA::Studio::StudioLayoutDirection::Forward;
    }
    else
    {
        VisualizationSettings.Layout.Orientation = InOrientation;
        VisualizationSettings.Layout.Direction =
            MiaIA::Studio::StudioLayoutDirection::Forward;
    }

    SaveVisualizationSettings(VisualizationSettings);
    NetworkView->SetVisualizationSettings(VisualizationSettings);
    Network3DView->SetVisualizationSettings(VisualizationSettings);
    FSlateApplication::Get().DismissAllMenus();
    return FReply::Handled();
}

FReply SMiaIAEditorPanel::SelectConnectionDisplayMode(
    EMiaIAConnectionDisplayMode InMode)
{
    VisualizationSettings.ConnectionDisplay = InMode;
    SaveVisualizationSettings(VisualizationSettings);
    NetworkView->SetVisualizationSettings(VisualizationSettings);
    Network3DView->SetVisualizationSettings(VisualizationSettings);
    FSlateApplication::Get().DismissAllMenus();
    return FReply::Handled();
}

void SMiaIAEditorPanel::HandleNeuronScaleChanged(float InValue)
{
    VisualizationSettings.NeuronScale = FMath::Clamp(
        InValue / 100.0f,
        0.25f,
        3.0f);
    SaveVisualizationSettings(VisualizationSettings);
    NetworkView->SetVisualizationSettings(VisualizationSettings);
    Network3DView->SetVisualizationSettings(VisualizationSettings);
}

void SMiaIAEditorPanel::HandleConnectionScaleChanged(float InValue)
{
    VisualizationSettings.ConnectionScale = FMath::Clamp(
        InValue / 100.0f,
        0.0f,
        2.0f);
    SaveVisualizationSettings(VisualizationSettings);
    NetworkView->SetVisualizationSettings(VisualizationSettings);
    Network3DView->SetVisualizationSettings(VisualizationSettings);
}

void SMiaIAEditorPanel::HandleNeuronGapChanged(float InValue)
{
    VisualizationSettings.Layout.NeuronGap = FMath::Clamp(
        static_cast<double>(InValue / 100.0f),
        0.0,
        5.0);
    SaveVisualizationSettings(VisualizationSettings);
    NetworkView->SetVisualizationSettings(VisualizationSettings);
    Network3DView->SetVisualizationSettings(VisualizationSettings);
}

void SMiaIAEditorPanel::HandleLayerGapChanged(float InValue)
{
    VisualizationSettings.Layout.LayerGap = FMath::Clamp(
        static_cast<double>(InValue / 100.0f),
        0.0,
        10.0);
    SaveVisualizationSettings(VisualizationSettings);
    NetworkView->SetVisualizationSettings(VisualizationSettings);
    Network3DView->SetVisualizationSettings(VisualizationSettings);
}

FReply SMiaIAEditorPanel::HandleResetVisualizationSettings()
{
    VisualizationSettings = {};
    SaveVisualizationSettings(VisualizationSettings);
    NetworkView->SetVisualizationSettings(VisualizationSettings);
    Network3DView->SetVisualizationSettings(VisualizationSettings);
    FSlateApplication::Get().DismissAllMenus();
    return FReply::Handled();
}

FReply SMiaIAEditorPanel::HandleFitView()
{
    if (ViewMode == EMiaIAStudioViewMode::ThreeDimensional &&
        Network3DView.IsValid())
    {
        Network3DView->FitView();
    }
    else if (NetworkView.IsValid())
    {
        NetworkView->FitView();
    }

    return FReply::Handled();
}

FReply SMiaIAEditorPanel::HandleResetLayout()
{
    if (ViewMode == EMiaIAStudioViewMode::ThreeDimensional &&
        Network3DView.IsValid())
    {
        Network3DView->ResetView();
    }
    else if (NetworkView.IsValid())
    {
        NetworkView->ResetLayout();
    }

    return FReply::Handled();
}

FReply SMiaIAEditorPanel::HandleToggleTopologyWorkspace()
{
    bTopologyWorkspaceExpanded = !bTopologyWorkspaceExpanded;
    Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
    RegisterActiveTimer(
        0.05f,
        FWidgetActiveTimerDelegate::CreateSP(
            this,
            &SMiaIAEditorPanel::HandleDeferredWorkspaceFit));
    return FReply::Handled();
}

FReply SMiaIAEditorPanel::HandleResume()
{
    UMiaIABlueprintLibrary::ResumeTrainingSession();
    RefreshData();
    return FReply::Handled();
}

FReply SMiaIAEditorPanel::HandlePause()
{
    UMiaIABlueprintLibrary::PauseTrainingSession();
    RefreshData();
    return FReply::Handled();
}

FReply SMiaIAEditorPanel::HandleStartDebug()
{
    FMiaIATrainingDebugSnapshot result;
    UMiaIABlueprintLibrary::StartSessionDebug(result);
    RefreshData();
    return FReply::Handled();
}

FReply SMiaIAEditorPanel::HandleAdvanceDebug()
{
    FMiaIATrainingDebugSnapshot result;
    UMiaIABlueprintLibrary::AdvanceDebugPhase(result);
    RefreshData();
    return FReply::Handled();
}

FReply SMiaIAEditorPanel::HandleCancelDebug()
{
    UMiaIABlueprintLibrary::CancelDebug();
    RefreshData();
    return FReply::Handled();
}

FReply SMiaIAEditorPanel::HandleApplySelectedNeuronBias()
{
    if (PendingNeuronBiasId < 0 ||
        !FMath::IsFinite(PendingNeuronBias) ||
        !UMiaIABlueprintLibrary::SetNeuronBias(
            PendingNeuronBiasId,
            PendingNeuronBias))
    {
        ShowDialog(
            LOCTEXT("NeuronBiasUpdateFailed", "Bias Update Failed"),
            LOCTEXT(
                "NeuronBiasUpdateFailedBody",
                "The bias could not be updated. Input neurons are not editable, and parameter changes are unavailable while training is running or phase debug is active."));
        return FReply::Handled();
    }

    bPendingNeuronBiasDirty = false;
    RefreshData();
    return FReply::Handled();
}

FReply SMiaIAEditorPanel::HandleApplySelectedConnectionWeight()
{
    if (PendingConnectionWeightId < 0 ||
        !FMath::IsFinite(PendingConnectionWeight) ||
        !UMiaIABlueprintLibrary::SetConnectionWeight(
            PendingConnectionWeightId,
            PendingConnectionWeight))
    {
        ShowDialog(
            LOCTEXT("ConnectionWeightUpdateFailed", "Weight Update Failed"),
            LOCTEXT(
                "ConnectionWeightUpdateFailedBody",
                "The weight could not be updated. Check the selected connection and value; parameter changes are unavailable while training is running or phase debug is active."));
        return FReply::Handled();
    }

    bPendingConnectionWeightDirty = false;
    RefreshData();
    return FReply::Handled();
}

FReply SMiaIAEditorPanel::HandleExit()
{
    if (bStandaloneMode)
    {
        FPlatformMisc::RequestExit(false);
    }

    return FReply::Handled();
}

FReply SMiaIAEditorPanel::SelectBottomTab(int32 TabIndex)
{
    if (BottomSwitcher.IsValid())
    {
        BottomSwitcher->SetActiveWidgetIndex(TabIndex);
    }

    return FReply::Handled();
}

FReply SMiaIAEditorPanel::HandleRunForwardTrace()
{
    if (!ForwardTraceInput.IsValid())
    {
        return FReply::Handled();
    }

    TArray<FString> tokens;
    ForwardTraceInput->GetText().ToString().ParseIntoArrayWS(tokens);
    TArray<double> inputs;
    inputs.Reserve(tokens.Num());

    for (const FString& token : tokens)
    {
        if (!token.IsNumeric())
        {
            ShowDialog(
                LOCTEXT("ForwardTraceInvalidTitle", "Trace not started"),
                LOCTEXT(
                    "ForwardTraceInvalidValue",
                    "Enter a whitespace-separated list of finite numeric input values."));
            return FReply::Handled();
        }

        inputs.Add(FCString::Atod(*token));
    }

    FMiaIAInstanceService::Refresh(MiaIAInstance);
    if (!FMiaIAInstanceService::RunForwardTrace(MiaIAInstance, inputs))
    {
        ShowDialog(
            LOCTEXT("ForwardTraceFailedTitle", "Trace not started"),
            LOCTEXT(
                "ForwardTraceFailed",
                "The input count or values do not match the current valid network."));
        return FReply::Handled();
    }

    if (SelectedNeuronIds.Num() == 1 && SelectedNeuronId >= 0)
    {
        FMiaIAInstanceService::FocusForwardTraceNeuron(
            MiaIAInstance,
            static_cast<uint64>(SelectedNeuronId));
    }

    RebuildForwardTrace();
    ApplyForwardTraceOverlay();
    return FReply::Handled();
}

FReply SMiaIAEditorPanel::HandleClearForwardTrace()
{
    FMiaIAInstanceService::ClearForwardTrace(MiaIAInstance);
    RebuildForwardTrace();
    ApplyForwardTraceOverlay();
    return FReply::Handled();
}

FReply SMiaIAEditorPanel::HandleRestartForwardTrace()
{
    if (FMiaIAInstanceService::RestartForwardTrace(MiaIAInstance))
    {
        RebuildForwardTrace();
        ApplyForwardTraceOverlay();
    }

    return FReply::Handled();
}

FReply SMiaIAEditorPanel::HandlePreviousForwardTraceFrame()
{
    if (FMiaIAInstanceService::StepForwardTraceBackward(MiaIAInstance))
    {
        RebuildForwardTrace();
        ApplyForwardTraceOverlay();
    }

    return FReply::Handled();
}

FReply SMiaIAEditorPanel::HandleToggleForwardTracePlayback()
{
    const MiaIA::Studio::StudioForwardTraceState state =
        FMiaIAInstanceService::ForwardTraceState(MiaIAInstance);
    const bool changed = state.PlaybackStatus ==
        MiaIA::Studio::StudioForwardTracePlaybackStatus::Playing
        ? FMiaIAInstanceService::PauseForwardTrace(MiaIAInstance)
        : FMiaIAInstanceService::PlayForwardTrace(MiaIAInstance);

    if (changed)
    {
        RebuildForwardTrace();
        ApplyForwardTraceOverlay();
    }

    return FReply::Handled();
}

FReply SMiaIAEditorPanel::HandleNextForwardTraceFrame()
{
    if (FMiaIAInstanceService::StepForwardTraceForward(MiaIAInstance))
    {
        RebuildForwardTrace();
        ApplyForwardTraceOverlay();
    }

    return FReply::Handled();
}

TSharedRef<SWidget> SMiaIAEditorPanel::BuildForwardTraceSpeedMenu()
{
    TSharedRef<SVerticalBox> menu = SNew(SVerticalBox);
    constexpr double speeds[] = { 0.25, 0.5, 1.0, 2.0, 4.0 };

    for (const double speed : speeds)
    {
        menu->AddSlot()
        .AutoHeight()
        [
            SNew(SButton)
            .ButtonStyle(&ButtonStyle)
            .Text(FText::FromString(FString::Printf(
                TEXT("%gx"),
                speed)))
            .OnClicked(
                this,
                &SMiaIAEditorPanel::SelectForwardTraceSpeed,
                speed)
        ];
    }

    return menu;
}

FReply SMiaIAEditorPanel::SelectForwardTraceSpeed(
    double SpeedMultiplier)
{
    if (SpeedMultiplier > 0.0)
    {
        FMiaIAInstanceService::SetForwardTraceFrameDuration(
            MiaIAInstance,
            DefaultForwardTraceFrameDurationSeconds / SpeedMultiplier);
        RebuildForwardTrace();
    }

    FSlateApplication::Get().DismissAllMenus();
    return FReply::Handled();
}

FReply SMiaIAEditorPanel::HandlePreviousForwardTracePage()
{
    const MiaIA::Studio::StudioForwardTraceState state =
        FMiaIAInstanceService::ForwardTraceState(MiaIAInstance);
    if (!state.HasContributionPage ||
        !state.ContributionPage.HasPrevious)
    {
        return FReply::Handled();
    }

    MiaIA::Core::ForwardTraceContributionPageRequest request =
        state.ContributionRequest;
    request.Offset = request.Offset > request.Limit
        ? request.Offset - request.Limit
        : 0;
    FMiaIAInstanceService::SetForwardTraceContributionRequest(
        MiaIAInstance,
        request);
    RebuildForwardTrace();
    ApplyForwardTraceOverlay();
    return FReply::Handled();
}

FReply SMiaIAEditorPanel::HandleNextForwardTracePage()
{
    const MiaIA::Studio::StudioForwardTraceState state =
        FMiaIAInstanceService::ForwardTraceState(MiaIAInstance);
    if (!state.HasContributionPage || !state.ContributionPage.HasNext)
    {
        return FReply::Handled();
    }

    MiaIA::Core::ForwardTraceContributionPageRequest request =
        state.ContributionRequest;
    request.Offset += request.Limit;
    FMiaIAInstanceService::SetForwardTraceContributionRequest(
        MiaIAInstance,
        request);
    RebuildForwardTrace();
    ApplyForwardTraceOverlay();
    return FReply::Handled();
}

TSharedRef<SWidget> SMiaIAEditorPanel::BuildBreakpointKindMenu()
{
    TSharedRef<SVerticalBox> menu = SNew(SVerticalBox);
    const EMiaIATrainingBreakpointKind kinds[] =
    {
        EMiaIATrainingBreakpointKind::Phase,
        EMiaIATrainingBreakpointKind::NeuronActivationAbove,
        EMiaIATrainingBreakpointKind::NeuronActivationBelow,
        EMiaIATrainingBreakpointKind::NeuronGradientMagnitudeAbove,
        EMiaIATrainingBreakpointKind::ConnectionUpdateMagnitudeAbove
    };

    for (const EMiaIATrainingBreakpointKind kind : kinds)
    {
        menu->AddSlot()
        .AutoHeight()
        [
            SNew(SButton)
            .ButtonStyle(&ButtonStyle)
            .Text(BreakpointKindName(kind))
            .OnClicked(
                this,
                &SMiaIAEditorPanel::SelectBreakpointKind,
                kind)
        ];
    }

    return menu;
}

TSharedRef<SWidget> SMiaIAEditorPanel::BuildBreakpointPhaseMenu()
{
    TSharedRef<SVerticalBox> menu = SNew(SVerticalBox);
    const EMiaIATrainingDebugPhase phases[] =
    {
        EMiaIATrainingDebugPhase::BeforeForward,
        EMiaIATrainingDebugPhase::ForwardComplete,
        EMiaIATrainingDebugPhase::BackwardComplete,
        EMiaIATrainingDebugPhase::UpdateComplete,
        EMiaIATrainingDebugPhase::Verified,
        EMiaIATrainingDebugPhase::Committed
    };

    for (const EMiaIATrainingDebugPhase phase : phases)
    {
        menu->AddSlot()
        .AutoHeight()
        [
            SNew(SButton)
            .ButtonStyle(&ButtonStyle)
            .Text(DebugPhaseName(phase))
            .OnClicked(
                this,
                &SMiaIAEditorPanel::SelectBreakpointPhase,
                phase)
        ];
    }

    return menu;
}

FReply SMiaIAEditorPanel::SelectBreakpointKind(
    EMiaIATrainingBreakpointKind InKind)
{
    BreakpointKind = InKind;
    return FReply::Handled();
}

FReply SMiaIAEditorPanel::SelectBreakpointPhase(
    EMiaIATrainingDebugPhase InPhase)
{
    BreakpointPhase = InPhase;
    return FReply::Handled();
}

FReply SMiaIAEditorPanel::HandleAddBreakpoint()
{
    int64 targetId{};
    double threshold{};

    if (BreakpointKind != EMiaIATrainingBreakpointKind::Phase)
    {
        if (!BreakpointTargetInput.IsValid() ||
            !BreakpointThresholdInput.IsValid() ||
            !LexTryParseString(
                targetId,
                *BreakpointTargetInput->GetText().ToString()) ||
            !LexTryParseString(
                threshold,
                *BreakpointThresholdInput->GetText().ToString()))
        {
            ShowDialog(
                LOCTEXT("InvalidBreakpointTitle", "Invalid breakpoint"),
                LOCTEXT(
                    "InvalidBreakpointValues",
                    "Enter a numeric neuron/connection ID and threshold."));
            return FReply::Handled();
        }
    }

    FMiaIATrainingBreakpoint breakpoint;

    if (!UMiaIABlueprintLibrary::AddTrainingBreakpoint(
        BreakpointKind,
        BreakpointPhase,
        targetId,
        threshold,
        breakpoint))
    {
        ShowDialog(
            LOCTEXT("BreakpointAddFailedTitle", "Breakpoint not added"),
            LOCTEXT(
                "BreakpointAddFailed",
                "Check the values and pause any running training or debug session."));
        return FReply::Handled();
    }

    if (BreakpointTargetInput.IsValid())
    {
        BreakpointTargetInput->SetText(FText::GetEmpty());
    }

    if (BreakpointThresholdInput.IsValid())
    {
        BreakpointThresholdInput->SetText(FText::GetEmpty());
    }

    RefreshData();
    return FReply::Handled();
}

FReply SMiaIAEditorPanel::HandleToggleBreakpoint(
    int64 BreakpointId,
    bool bEnabled)
{
    if (!UMiaIABlueprintLibrary::SetTrainingBreakpointEnabled(
        BreakpointId,
        bEnabled))
    {
        ShowDialog(
            LOCTEXT("BreakpointUpdateFailedTitle", "Breakpoint not updated"),
            LOCTEXT(
                "BreakpointUpdateFailed",
                "Pause the running training or debug operation and try again."));
    }

    RefreshData();
    return FReply::Handled();
}

FReply SMiaIAEditorPanel::HandleRemoveBreakpoint(int64 BreakpointId)
{
    if (!UMiaIABlueprintLibrary::RemoveTrainingBreakpoint(BreakpointId))
    {
        ShowDialog(
            LOCTEXT("BreakpointRemoveFailedTitle", "Breakpoint not removed"),
            LOCTEXT(
                "BreakpointRemoveFailed",
                "Pause the running training or debug operation and try again."));
    }

    RefreshData();
    return FReply::Handled();
}

FReply SMiaIAEditorPanel::HandleClearBreakpoints()
{
    if (!UMiaIABlueprintLibrary::ClearTrainingBreakpoints())
    {
        ShowDialog(
            LOCTEXT("BreakpointClearFailedTitle", "Breakpoints not cleared"),
            LOCTEXT(
                "BreakpointClearFailed",
                "Pause the running training or debug operation and try again."));
    }

    RefreshData();
    return FReply::Handled();
}

TSharedRef<SWidget> SMiaIAEditorPanel::BuildViewModeMenu()
{
    return SNew(SVerticalBox)
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SButton)
            .ButtonStyle(&ButtonStyle)
            .Text(LOCTEXT("TwoDimensionalView", "2D"))
            .OnClicked(
                this,
                &SMiaIAEditorPanel::SelectViewMode,
                EMiaIAStudioViewMode::TwoDimensional)
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SButton)
            .ButtonStyle(&ButtonStyle)
            .Text(LOCTEXT("ThreeDimensionalView", "3D"))
            .OnClicked(
                this,
                &SMiaIAEditorPanel::SelectViewMode,
                EMiaIAStudioViewMode::ThreeDimensional)
        ];
}

FReply SMiaIAEditorPanel::SelectViewMode(
    EMiaIAStudioViewMode InViewMode)
{
    ViewMode = InViewMode;

    if (TopologySwitcher.IsValid())
    {
        TopologySwitcher->SetActiveWidgetIndex(
            ViewMode == EMiaIAStudioViewMode::TwoDimensional
                ? 0
                : 1);
    }

    if (ViewMode == EMiaIAStudioViewMode::ThreeDimensional &&
        Network3DView.IsValid())
    {
        Network3DView->FitView();
    }

    FSlateApplication::Get().DismissAllMenus();
    return FReply::Handled();
}

TSharedRef<SWidget> SMiaIAEditorPanel::BuildThemeMenu()
{
    return SNew(SVerticalBox)
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SButton)
            .ButtonStyle(&ButtonStyle)
            .Text(FMiaIAEditorTheme::DisplayName(
                EMiaIAEditorTheme::FollowUnreal))
            .OnClicked(
                this,
                &SMiaIAEditorPanel::SelectTheme,
                EMiaIAEditorTheme::FollowUnreal)
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SButton)
            .ButtonStyle(&ButtonStyle)
            .Text(FMiaIAEditorTheme::DisplayName(
                EMiaIAEditorTheme::Dark))
            .OnClicked(
                this,
                &SMiaIAEditorPanel::SelectTheme,
                EMiaIAEditorTheme::Dark)
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SButton)
            .ButtonStyle(&ButtonStyle)
            .Text(FMiaIAEditorTheme::DisplayName(
                EMiaIAEditorTheme::Light))
            .OnClicked(
                this,
                &SMiaIAEditorPanel::SelectTheme,
                EMiaIAEditorTheme::Light)
        ];
}

FReply SMiaIAEditorPanel::SelectTheme(EMiaIAEditorTheme InTheme)
{
    Theme = InTheme;
    FMiaIAEditorTheme::Save(Theme);
    RefreshWidgetStyles();

    if (NetworkView.IsValid())
    {
        NetworkView->SetTheme(Theme);
    }

    if (Network3DView.IsValid())
    {
        Network3DView->SetTheme(Theme);
    }

    FSlateApplication::Get().DismissAllMenus();
    Invalidate(EInvalidateWidgetReason::Paint);
    return FReply::Handled();
}

TSharedRef<SWidget> SMiaIAEditorPanel::BuildDataRefreshMenu()
{
    return SNew(SVerticalBox)
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SButton)
            .ButtonStyle(&ButtonStyle)
            .Text(DataRefreshModeDisplayName(
                EMiaIADataRefreshMode::Adaptive))
            .ToolTipText(LOCTEXT(
                "AdaptiveDataRefreshTooltip",
                "Refresh at 4 Hz while training runs and at 1 Hz while idle or paused."))
            .OnClicked(
                this,
                &SMiaIAEditorPanel::SelectDataRefreshMode,
                EMiaIADataRefreshMode::Adaptive)
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SButton)
            .ButtonStyle(&ButtonStyle)
            .Text(DataRefreshModeDisplayName(
                EMiaIADataRefreshMode::OneHz))
            .OnClicked(
                this,
                &SMiaIAEditorPanel::SelectDataRefreshMode,
                EMiaIADataRefreshMode::OneHz)
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SButton)
            .ButtonStyle(&ButtonStyle)
            .Text(DataRefreshModeDisplayName(
                EMiaIADataRefreshMode::TwoHz))
            .OnClicked(
                this,
                &SMiaIAEditorPanel::SelectDataRefreshMode,
                EMiaIADataRefreshMode::TwoHz)
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SButton)
            .ButtonStyle(&ButtonStyle)
            .Text(DataRefreshModeDisplayName(
                EMiaIADataRefreshMode::FourHz))
            .OnClicked(
                this,
                &SMiaIAEditorPanel::SelectDataRefreshMode,
                EMiaIADataRefreshMode::FourHz)
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SButton)
            .ButtonStyle(&ButtonStyle)
            .Text(DataRefreshModeDisplayName(
                EMiaIADataRefreshMode::TenHz))
            .ToolTipText(LOCTEXT(
                "TenHzDataRefreshTooltip",
                "Use for highly responsive monitoring at a higher CPU cost."))
            .OnClicked(
                this,
                &SMiaIAEditorPanel::SelectDataRefreshMode,
                EMiaIADataRefreshMode::TenHz)
        ];
}

FReply SMiaIAEditorPanel::SelectDataRefreshMode(
    EMiaIADataRefreshMode InMode)
{
    DataRefreshMode = InMode;
    PeriodicRefreshElapsedSeconds = 0.0;
    SaveDataRefreshMode(DataRefreshMode);
    FSlateApplication::Get().DismissAllMenus();
    return FReply::Handled();
}

TSharedRef<SWidget> SMiaIAEditorPanel::BuildTopologyLimitsMenu()
{
    PendingDetailedNeuronLimit = DetailedNeuronLimit;
    PendingDetailedConnectionLimit = DetailedConnectionLimit;
    PendingInspectorConnectionLimit = InspectorConnectionLimit;

    return SNew(SBox)
        .WidthOverride(340.0f)
        .Padding(10.0f)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, 4.0f)
            [
                SNew(STextBlock)
                .Text(LOCTEXT(
                    "DetailedNeuronLimitLabel",
                    "Detailed neurons"))
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, 10.0f)
            [
                SNew(SSpinBox<int32>)
                .MinValue(MinimumTopologyLimit)
                .MaxValue(MaximumDetailedNeuronLimit)
                .MinSliderValue(MinimumTopologyLimit)
                .MaxSliderValue(MaximumNeuronSliderLimit)
                .Delta(100)
                .Value(PendingDetailedNeuronLimit)
                .OnValueChanged(
                    this,
                    &SMiaIAEditorPanel::
                        HandlePendingNeuronLimitChanged)
                .ToolTipText(LOCTEXT(
                    "DetailedNeuronLimitTooltip",
                    "Networks above this neuron count use compact mode. Values above the slider range can be typed directly."))
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, 4.0f)
            [
                SNew(STextBlock)
                .Text(LOCTEXT(
                    "DetailedConnectionLimitLabel",
                    "Detailed connections"))
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, 10.0f)
            [
                SNew(SSpinBox<int32>)
                .MinValue(MinimumTopologyLimit)
                .MaxValue(MaximumDetailedConnectionLimit)
                .MinSliderValue(MinimumTopologyLimit)
                .MaxSliderValue(MaximumConnectionSliderLimit)
                .Delta(100)
                .Value(PendingDetailedConnectionLimit)
                .OnValueChanged(
                    this,
                    &SMiaIAEditorPanel::
                        HandlePendingConnectionLimitChanged)
                .ToolTipText(LOCTEXT(
                    "DetailedConnectionLimitTooltip",
                    "Networks above this connection count use compact mode. Values above the slider range can be typed directly."))
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, 4.0f)
            [
                SNew(STextBlock)
                .Text(LOCTEXT(
                    "InspectorConnectionLimitLabel",
                    "Relationship page size"))
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, 10.0f)
            [
                SNew(SSpinBox<int32>)
                .MinValue(MinimumTopologyLimit)
                .MaxValue(MaximumInspectorConnectionLimit)
                .MinSliderValue(MinimumTopologyLimit)
                .MaxSliderValue(MaximumInspectorConnectionSliderLimit)
                .Delta(1)
                .Value(PendingInspectorConnectionLimit)
                .OnValueChanged(
                    this,
                    &SMiaIAEditorPanel::
                        HandlePendingInspectorConnectionLimitChanged)
                .ToolTipText(LOCTEXT(
                    "InspectorConnectionLimitTooltip",
                    "Relationship Explorer page size for one selected neuron."))
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(STextBlock)
                .AutoWrapText(true)
                .Text(LOCTEXT(
                    "TopologyLimitsWarning",
                    "Higher limits can significantly increase snapshot, layout, and rendering cost."))
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 10.0f, 0.0f, 0.0f)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .Padding(0.0f, 0.0f, 4.0f, 0.0f)
                [
                    SNew(SButton)
                    .ButtonStyle(&ButtonStyle)
                    .Text(LOCTEXT("ApplyTopologyLimits", "Apply"))
                    .OnClicked(
                        this,
                        &SMiaIAEditorPanel::HandleApplyTopologyLimits)
                ]
                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .Padding(4.0f, 0.0f, 0.0f, 0.0f)
                [
                    SNew(SButton)
                    .ButtonStyle(&ButtonStyle)
                    .Text(LOCTEXT(
                        "ResetTopologyLimits",
                        "Reset defaults"))
                    .OnClicked(
                        this,
                        &SMiaIAEditorPanel::HandleResetTopologyLimits)
                ]
            ]
        ];
}

void SMiaIAEditorPanel::HandlePendingNeuronLimitChanged(int32 InValue)
{
    PendingDetailedNeuronLimit = FMath::Clamp(
        InValue,
        MinimumTopologyLimit,
        MaximumDetailedNeuronLimit);
}

void SMiaIAEditorPanel::HandlePendingConnectionLimitChanged(int32 InValue)
{
    PendingDetailedConnectionLimit = FMath::Clamp(
        InValue,
        MinimumTopologyLimit,
        MaximumDetailedConnectionLimit);
}

void SMiaIAEditorPanel::HandlePendingInspectorConnectionLimitChanged(
    int32 InValue)
{
    PendingInspectorConnectionLimit = FMath::Clamp(
        InValue,
        MinimumTopologyLimit,
        MaximumInspectorConnectionLimit);
}

FReply SMiaIAEditorPanel::HandleApplyTopologyLimits()
{
    DetailedNeuronLimit = PendingDetailedNeuronLimit;
    DetailedConnectionLimit = PendingDetailedConnectionLimit;
    InspectorConnectionLimit = PendingInspectorConnectionLimit;
    RelationshipOffset = 0;
    SaveTopologyLimits(
        DetailedNeuronLimit,
        DetailedConnectionLimit,
        InspectorConnectionLimit);
    FSlateApplication::Get().DismissAllMenus();
    RefreshData();
    return FReply::Handled();
}

FReply SMiaIAEditorPanel::HandleResetTopologyLimits()
{
    DetailedNeuronLimit = static_cast<int32>(
        MiaIA::Studio::StudioTopologyBuilder::DetailedNeuronLimit);
    DetailedConnectionLimit = static_cast<int32>(
        MiaIA::Studio::StudioTopologyBuilder::DetailedConnectionLimit);
    PendingDetailedNeuronLimit = DetailedNeuronLimit;
    PendingDetailedConnectionLimit = DetailedConnectionLimit;
    InspectorConnectionLimit = DefaultInspectorConnectionLimit;
    PendingInspectorConnectionLimit = InspectorConnectionLimit;
    RelationshipOffset = 0;
    SaveTopologyLimits(
        DetailedNeuronLimit,
        DetailedConnectionLimit,
        InspectorConnectionLimit);
    FSlateApplication::Get().DismissAllMenus();
    RefreshData();
    return FReply::Handled();
}

TSharedRef<SWidget> SMiaIAEditorPanel::BuildProjectMenu()
{
    return SNew(SVerticalBox)
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SButton)
            .ButtonStyle(&ButtonStyle)
            .Text(LOCTEXT("NewProjectMenuItem", "New"))
            .OnClicked(this, &SMiaIAEditorPanel::HandleNewProject)
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SButton)
            .ButtonStyle(&ButtonStyle)
            .Text(LOCTEXT("OpenProjectMenuItem", "Open..."))
            .OnClicked(this, &SMiaIAEditorPanel::HandleOpenProject)
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SButton)
            .ButtonStyle(&ButtonStyle)
            .Text(LOCTEXT("SaveProjectMenuItem", "Save"))
            .OnClicked(this, &SMiaIAEditorPanel::HandleSaveProject)
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SButton)
            .ButtonStyle(&ButtonStyle)
            .Text(LOCTEXT("SaveProjectAsMenuItem", "Save as..."))
            .OnClicked(this, &SMiaIAEditorPanel::HandleSaveProjectAs)
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SButton)
            .ButtonStyle(&ButtonStyle)
            .Text(LOCTEXT("ImportOnnxMenuItem", "Import ONNX..."))
            .OnClicked(this, &SMiaIAEditorPanel::HandleImportOnnx)
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SButton)
            .ButtonStyle(&ButtonStyle)
            .Text(LOCTEXT("ExportOnnxMenuItem", "Export ONNX..."))
            .OnClicked(this, &SMiaIAEditorPanel::HandleExportOnnx)
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SButton)
            .ButtonStyle(&ButtonStyle)
            .Text(LOCTEXT("ProjectInfoMenuItem", "Info"))
            .OnClicked(this, &SMiaIAEditorPanel::HandleProjectInfo)
        ];
}

FReply SMiaIAEditorPanel::HandleNewProject()
{
    FSlateApplication::Get().DismissAllMenus();

    if (!UMiaIABlueprintLibrary::NewProject())
    {
        ShowDialog(
            LOCTEXT("NewProjectFailedTitle", "New Project Failed"),
            LOCTEXT(
                "NewProjectFailedContent",
                "Pause training or cancel phase debugging before replacing the current project."));
        return FReply::Handled();
    }

    ConsoleHistory += TEXT("\n> project new\nNew MiaIA project created.\n");
    UpdateConsoleOutput();
    RefreshData();
    return FReply::Handled();
}

FReply SMiaIAEditorPanel::HandleOpenProject()
{
    ShowProjectPathDialog(EMiaIAProjectPathAction::Open);
    return FReply::Handled();
}

FReply SMiaIAEditorPanel::HandleSaveProject()
{
    FSlateApplication::Get().DismissAllMenus();

    if (UMiaIABlueprintLibrary::GetNetworkOverview().Layers.Num() == 0)
    {
        ShowDialog(
            LOCTEXT(
                "SaveProjectWithoutModelTitle",
                "No Model to Save"),
            LOCTEXT(
                "SaveProjectWithoutModelContent",
                "Create or import a valid network before saving a .mai project. Version 1 always contains an embedded model."));
        return FReply::Handled();
    }

    const FMiaIAProjectInfo info =
        UMiaIABlueprintLibrary::GetProjectInfo();

    if (info.Path.IsEmpty())
    {
        ShowProjectPathDialog(EMiaIAProjectPathAction::SaveAs);
        return FReply::Handled();
    }

    if (!UMiaIABlueprintLibrary::SaveProject(info.Path))
    {
        ShowDialog(
            LOCTEXT("SaveProjectFailedTitle", "Save Project Failed"),
            LOCTEXT(
                "SaveProjectFailedContent",
                "The .mai project could not be saved. Check the path and pause active training or phase debugging."));
        return FReply::Handled();
    }

    ConsoleHistory += FString::Printf(
        TEXT("\n> project save\nMiaIA project saved to %s.\n"),
        *info.Path);
    UpdateConsoleOutput();
    return FReply::Handled();
}

FReply SMiaIAEditorPanel::HandleSaveProjectAs()
{
    if (UMiaIABlueprintLibrary::GetNetworkOverview().Layers.Num() == 0)
    {
        FSlateApplication::Get().DismissAllMenus();
        ShowDialog(
            LOCTEXT(
                "SaveProjectAsWithoutModelTitle",
                "No Model to Save"),
            LOCTEXT(
                "SaveProjectAsWithoutModelContent",
                "Create or import a valid network before saving a .mai project. Version 1 always contains an embedded model."));
        return FReply::Handled();
    }

    ShowProjectPathDialog(EMiaIAProjectPathAction::SaveAs);
    return FReply::Handled();
}

FReply SMiaIAEditorPanel::HandleImportOnnx()
{
    ShowProjectPathDialog(EMiaIAProjectPathAction::ImportOnnx);
    return FReply::Handled();
}

FReply SMiaIAEditorPanel::HandleExportOnnx()
{
    if (UMiaIABlueprintLibrary::GetNetworkOverview().Layers.Num() == 0)
    {
        FSlateApplication::Get().DismissAllMenus();
        ShowDialog(
            LOCTEXT(
                "ExportOnnxWithoutModelTitle",
                "No Model to Export"),
            LOCTEXT(
                "ExportOnnxWithoutModelContent",
                "Create, import, or open a valid network before exporting an ONNX model."));
        return FReply::Handled();
    }

    ShowProjectPathDialog(EMiaIAProjectPathAction::ExportOnnx);
    return FReply::Handled();
}

FReply SMiaIAEditorPanel::HandleProjectInfo()
{
    FSlateApplication::Get().DismissAllMenus();
    const FMiaIAProjectInfo info =
        UMiaIABlueprintLibrary::GetProjectInfo();
    FString content = FString::Printf(
        TEXT("Path: %s\nFormat: .mai v%d\nModel: %s\nBreakpoints: %lld"),
        info.Path.IsEmpty() ? TEXT("Unsaved") : *info.Path,
        info.FormatVersion > 0 ? info.FormatVersion : 1,
        info.bHasModel ? TEXT("Available") : TEXT("None"),
        static_cast<long long>(info.BreakpointCount));

    if (!info.bHasDatasetReference)
    {
        content += TEXT("\nDataset: None");
    }
    else
    {
        content += FString::Printf(
            TEXT("\nDataset: %s\nDataset source: %s\nDataset inputs: %lld\nDataset targets: %lld\nDataset header: %s"),
            info.bDatasetLoaded ? TEXT("Loaded") : TEXT("Unavailable"),
            *info.DatasetSource,
            static_cast<long long>(info.DatasetInputCount),
            static_cast<long long>(info.DatasetTargetCount),
            info.bDatasetHasHeader ? TEXT("Yes") : TEXT("No"));
    }

    if (info.bTrainingAvailable)
    {
        content += FString::Printf(
            TEXT("\nTraining epochs: %lld\nLearning rate: %.12g\nLoss: MSE\nOptimizer: SGD"),
            static_cast<long long>(info.TrainingEpochCount),
            info.TrainingLearningRate);
    }
    else
    {
        content += TEXT("\nTraining configuration: None");
    }

    ShowDialog(
        LOCTEXT("ProjectInfoTitle", "MiaIA Project"),
        FText::FromString(content));
    return FReply::Handled();
}

void SMiaIAEditorPanel::ShowProjectPathDialog(
    EMiaIAProjectPathAction Action)
{
    ProjectPathAction = Action;

    switch (Action)
    {
    case EMiaIAProjectPathAction::Open:
        DialogTitle = LOCTEXT("OpenProjectTitle", "Open MiaIA Project");
        break;
    case EMiaIAProjectPathAction::SaveAs:
        DialogTitle = LOCTEXT(
            "SaveProjectAsTitle",
            "Save MiaIA Project As");
        break;
    case EMiaIAProjectPathAction::ImportOnnx:
        DialogTitle = LOCTEXT("ImportOnnxTitle", "Import ONNX Model");
        break;
    case EMiaIAProjectPathAction::ExportOnnx:
        DialogTitle = LOCTEXT("ExportOnnxTitle", "Export ONNX Model");
        break;
    default:
        DialogTitle = FText::GetEmpty();
        break;
    }

    DialogContent = FText::GetEmpty();
    bDialogVisible = true;
    FSlateApplication::Get().DismissAllMenus();

    const bool onnxAction =
        Action == EMiaIAProjectPathAction::ImportOnnx ||
        Action == EMiaIAProjectPathAction::ExportOnnx;
    FString path = onnxAction
        ? FString()
        : UMiaIABlueprintLibrary::GetProjectInfo().Path;

    if (path.IsEmpty())
    {
        path = FPaths::Combine(
            FPaths::ProjectSavedDir(),
            onnxAction
                ? TEXT("MiaIAModel.onnx")
                : TEXT("MiaIAProject.mai"));
    }

    if (ProjectPathInput.IsValid())
    {
        ProjectPathInput->SetText(FText::FromString(path));
        FSlateApplication::Get().SetKeyboardFocus(
            ProjectPathInput,
            EFocusCause::SetDirectly);
        ProjectPathInput->SelectAllText();
    }

    Invalidate(EInvalidateWidgetReason::PaintAndVolatility);
}

FReply SMiaIAEditorPanel::HandleConfirmProjectPath()
{
    if (!ProjectPathInput.IsValid() ||
        ProjectPathAction == EMiaIAProjectPathAction::None)
    {
        return FReply::Handled();
    }

    FString path = ProjectPathInput->GetText().ToString();
    path.TrimStartAndEndInline();

    if (FPaths::GetExtension(path).IsEmpty())
    {
        if (ProjectPathAction == EMiaIAProjectPathAction::SaveAs)
        {
            path += TEXT(".mai");
        }
        else if (ProjectPathAction ==
            EMiaIAProjectPathAction::ExportOnnx)
        {
            path += TEXT(".onnx");
        }
    }

    if (path.IsEmpty())
    {
        return FReply::Handled();
    }

    path = FPaths::ConvertRelativePathToFull(path);
    const EMiaIAProjectPathAction action = ProjectPathAction;
    bool succeeded{};

    switch (action)
    {
    case EMiaIAProjectPathAction::Open:
        succeeded = UMiaIABlueprintLibrary::OpenProject(path);
        break;
    case EMiaIAProjectPathAction::SaveAs:
        succeeded = UMiaIABlueprintLibrary::SaveProject(path);
        break;
    case EMiaIAProjectPathAction::ImportOnnx:
        succeeded = UMiaIABlueprintLibrary::ImportOnnx(path);
        break;
    case EMiaIAProjectPathAction::ExportOnnx:
        succeeded = UMiaIABlueprintLibrary::ExportOnnx(path);
        break;
    default:
        break;
    }

    if (!succeeded)
    {
        ProjectPathAction = EMiaIAProjectPathAction::None;
        const bool projectAction =
            action == EMiaIAProjectPathAction::Open ||
            action == EMiaIAProjectPathAction::SaveAs;
        ShowDialog(
            action == EMiaIAProjectPathAction::Open
                ? LOCTEXT("OpenProjectFailedTitle", "Open Project Failed")
                : action == EMiaIAProjectPathAction::SaveAs
                    ? LOCTEXT(
                        "SaveProjectAsFailedTitle",
                        "Save Project Failed")
                    : action == EMiaIAProjectPathAction::ImportOnnx
                        ? LOCTEXT(
                            "ImportOnnxFailedTitle",
                            "Import ONNX Failed")
                        : LOCTEXT(
                            "ExportOnnxFailedTitle",
                            "Export ONNX Failed"),
            projectAction
                ? LOCTEXT(
                    "ProjectPathFailedContent",
                    "The .mai project could not be processed. Check the path, file format, and current training/debug state.")
                : LOCTEXT(
                    "OnnxPathFailedContent",
                    "The ONNX model could not be processed. Check the path, supported graph format, and current training/debug state."));
        return FReply::Handled();
    }

    bDialogVisible = false;
    ProjectPathAction = EMiaIAProjectPathAction::None;
    switch (action)
    {
    case EMiaIAProjectPathAction::Open:
        ConsoleHistory += FString::Printf(
            TEXT("\n> project open \"%s\"\nMiaIA project opened.\n"),
            *path);
        break;
    case EMiaIAProjectPathAction::SaveAs:
        ConsoleHistory += FString::Printf(
            TEXT("\n> project save \"%s\"\nMiaIA project saved.\n"),
            *path);
        break;
    case EMiaIAProjectPathAction::ImportOnnx:
        ConsoleHistory += FString::Printf(
            TEXT("\n> import onnx \"%s\"\nONNX model imported.\n"),
            *path);
        break;
    case EMiaIAProjectPathAction::ExportOnnx:
        ConsoleHistory += FString::Printf(
            TEXT("\n> export onnx \"%s\"\nONNX model exported.\n"),
            *path);
        break;
    default:
        break;
    }
    UpdateConsoleOutput();

    if (action == EMiaIAProjectPathAction::Open ||
        action == EMiaIAProjectPathAction::ImportOnnx)
    {
        FocusedLayerId = -1;
        bNetworkPreview = true;
    }

    RefreshData();
    return FReply::Handled();
}

FReply SMiaIAEditorPanel::HandleCancelProjectPath()
{
    ProjectPathAction = EMiaIAProjectPathAction::None;
    bDialogVisible = false;
    Invalidate(EInvalidateWidgetReason::PaintAndVolatility);
    return FReply::Handled();
}

TSharedRef<SWidget> SMiaIAEditorPanel::BuildHelpMenu()
{
    return SNew(SVerticalBox)
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SButton)
            .ButtonStyle(&ButtonStyle)
            .Text(LOCTEXT("QuickHelpMenuItem", "Quick help"))
            .OnClicked(this, &SMiaIAEditorPanel::HandleQuickHelp)
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SButton)
            .ButtonStyle(&ButtonStyle)
            .Text(LOCTEXT(
                "AboutMiaIAStudioMenuItem",
                "About MiaIA Studio"))
            .OnClicked(this, &SMiaIAEditorPanel::HandleAbout)
        ];
}

FReply SMiaIAEditorPanel::HandleQuickHelp()
{
    ShowDialog(
        LOCTEXT("QuickHelpTitle", "MiaIA Studio Quick Help"),
        LOCTEXT(
            "QuickHelpContent",
            "GETTING STARTED\n"
            "Use the Console to create or import a network, then choose 2D or 3D from View.\n\n"
            "PROJECTS AND INTERCHANGE\n"
            "Use Project to create, open, save, or inspect a .mai project. Import ONNX and Export ONNX exchange only the supported model portion.\n\n"
            "SELECTION AND LAYOUT\n"
            "Left click: select a neuron or connection.\n"
            "Ctrl + left click: add or remove a neuron from the selection.\n"
            "Drag empty space: select neurons with a rectangle.\n"
            "Ctrl + rectangle: add neurons to the current selection.\n"
            "Drag a selected neuron: move the complete selected group.\n"
            "Every network starts on its whole-network preview. Click its central node or press Enter to continue with the normal detailed or compact topology.\n"
            "Compact mode: click a layer aggregate to inspect its summary, then double-click it or press Enter to open that layer. Use Back or Esc to retrace the available levels to the preview.\n"
            "Click the topology, then use the arrow keys in the visible direction; their layer/neuron meaning follows Horizontal or Vertical flow and its Forward or Reverse direction.\n"
            "Mouse wheel: zoom. Middle drag: pan. Right drag pans in 2D and orbits in 3D.\n"
            "Use Layout for Expanded or Packed placement, Horizontal or Vertical flow, uniform neuron size, minimum gaps, and All or Selected connections. Click the active orientation again to mirror the layer direction. Packed with zero gaps makes symmetric nodes adjacent without overlap.\n\n"
            "INSPECTION AND DEBUG\n"
            "The Inspector follows the current element or group. For one neuron, choose Incoming or Outgoing relationships, page them, order by ID or weight, filter by minimum absolute weight, and open either the connection or its opposite neuron. Use Start debug and Step phase to inspect forward, backward, update, verify, and commit states.\n\n"

            "EXECUTION TRACE\n"
            "Open Execution trace, enter one whitespace-separated value per input neuron, and choose Run trace. Use Reset, Previous frame, Play/Pause, Next frame, and Speed to inspect the signal layer by layer in 2D or 3D. Select a neuron to inspect its weighted input, bias, pre-activation, activation, and paged incoming contributions. Clear removes the captured overlay without changing the model.\n\n"

            "BREAKPOINTS\n"
            "Open the Breakpoints tab to stop controlled training on a phase, neuron activation, neuron gradient, or connection update. Automatic training stops after the triggering sample commits, before the next sample begins.\n\n"

            "LICENSE AND SOURCE\n"
            "Official website and downloads: https://www.nonop.biz\n"
            "MiaIA source code is available under the Mozilla Public License 2.0 at https://github.com/Agosillo/MiaIA. Unreal Engine and third-party components remain under their respective terms.\n\n"

            "Type 'help' in the Console to list every shared CLI command."));
    return FReply::Handled();
}

FReply SMiaIAEditorPanel::HandleAbout()
{
    FString version = TEXT("0.1.0");
    FString configuredVersion;

    if (GConfig && GConfig->GetString(
        TEXT("/Script/EngineSettings.GeneralProjectSettings"),
        TEXT("ProjectVersion"),
        configuredVersion,
        GGameIni) &&
        !configuredVersion.IsEmpty())
    {
        version = MoveTemp(configuredVersion);
    }

    ShowDialog(
        LOCTEXT("AboutMiaIAStudioTitle", "About MiaIA Studio"),
        FText::Format(
            LOCTEXT(
                "AboutMiaIAStudioContent",
                "MiaIA Studio {0}\n\n"
                "VISUALIZE | EXPERIMENT | INSPECT | DEBUG\n\n"
                "An interactive development environment for understanding neural networks through visualization, experimentation, and step-by-step debugging.\n\n"
                "This Unreal frontend uses the shared MiaIA Engine, SDK, and CLI application services.\n\n"
                "Copyright 2026 Agostino Mosillo\n"
                "Official website and downloads: https://www.nonop.biz\n"
                "MiaIA source code: https://github.com/Agosillo/MiaIA\n"
                "Licensed under the Mozilla Public License 2.0.\n\n"
                "MiaIA Studio uses Unreal\u00AE Engine. Unreal\u00AE is a trademark or registered trademark of Epic Games, Inc. in the United States of America and elsewhere.\n"
                "Unreal\u00AE Engine, Copyright 1998-2026, Epic Games, Inc. All rights reserved.\n\n"
                "Early development build."),
            FText::FromString(version)));
    return FReply::Handled();
}

FReply SMiaIAEditorPanel::HandleCloseDialog()
{
    ProjectPathAction = EMiaIAProjectPathAction::None;
    bDialogVisible = false;
    Invalidate(EInvalidateWidgetReason::PaintAndVolatility);
    return FReply::Handled();
}

void SMiaIAEditorPanel::ShowDialog(
    const FText& Title,
    const FText& Content)
{
    ProjectPathAction = EMiaIAProjectPathAction::None;
    DialogTitle = Title;
    DialogContent = Content;
    bDialogVisible = true;
    FSlateApplication::Get().DismissAllMenus();
    FSlateApplication::Get().ClearKeyboardFocus(EFocusCause::Cleared);
    Invalidate(EInvalidateWidgetReason::PaintAndVolatility);
}

EVisibility SMiaIAEditorPanel::DialogVisibility() const
{
    return bDialogVisible
        ? EVisibility::Visible
        : EVisibility::Collapsed;
}

EVisibility SMiaIAEditorPanel::DialogContentVisibility() const
{
    return ProjectPathAction == EMiaIAProjectPathAction::None
        ? EVisibility::Visible
        : EVisibility::Collapsed;
}

EVisibility SMiaIAEditorPanel::ProjectPathVisibility() const
{
    return ProjectPathAction != EMiaIAProjectPathAction::None
        ? EVisibility::Visible
        : EVisibility::Collapsed;
}

void SMiaIAEditorPanel::RefreshWidgetStyles()
{
    ButtonStyle = FMiaIAEditorTheme::ButtonStyle(Theme);
    ExplorerButtonStyle =
        FMiaIAEditorTheme::ExplorerButtonStyle(Theme);
    ComboButtonStyle = FMiaIAEditorTheme::ComboButtonStyle(Theme);
    InputStyle = FMiaIAEditorTheme::InputStyle(Theme);
    ScrollBarStyle = FMiaIAEditorTheme::ScrollBarStyle(Theme);
    SplitterStyle = FMiaIAEditorTheme::SplitterStyle(Theme);
}

void SMiaIAEditorPanel::HandleConsoleCommandCommitted(
    const FText& Text,
    ETextCommit::Type CommitType)
{
    if (CommitType != ETextCommit::OnEnter)
    {
        return;
    }

    const FString command = Text.ToString().TrimStartAndEnd();

    if (command.IsEmpty())
    {
        return;
    }

    if (ConsoleCommandHistory.IsEmpty() ||
        ConsoleCommandHistory.Last() != command)
    {
        ConsoleCommandHistory.Add(command);
    }

    ConsoleHistoryIndex = ConsoleCommandHistory.Num();
    ConsoleHistoryDraft.Empty();

    bool exitRequested{};
    const FString output = UMiaIABlueprintLibrary::ExecuteCommand(
        command,
        exitRequested);
    ConsoleHistory += FString::Printf(TEXT("\n> %s\n"), *command);
    ConsoleHistory += output;

    if (exitRequested)
    {
        ConsoleHistory += TEXT(
            "Exit is available only in the standalone Console.\n");
    }

    UpdateConsoleOutput();
    SetConsoleInputText(FString());

    const bool replacedNetwork =
        output.Contains(TEXT("Dense network created.")) ||
        output.Contains(TEXT("ONNX model imported.")) ||
        output.Contains(TEXT("New MiaIA project created.")) ||
        output.Contains(TEXT("Project opened."));

    if (replacedNetwork)
    {
        FocusedLayerId = -1;
        bNetworkPreview = true;
    }

    RefreshData();

    if (ConsoleInput.IsValid())
    {
        FSlateApplication::Get().SetKeyboardFocus(
            ConsoleInput,
            EFocusCause::SetDirectly);
    }
}

void SMiaIAEditorPanel::HandleConsoleTextChanged(const FText& Text)
{
    const FString input = Text.ToString();

    if (!bUpdatingConsoleInput)
    {
        ConsoleHistoryIndex = ConsoleCommandHistory.Num();
        ConsoleHistoryDraft = input;
    }

    RebuildConsoleSuggestions(input);
}

FReply SMiaIAEditorPanel::HandleConsoleInputKeyDown(
    const FGeometry& Geometry,
    const FKeyEvent& KeyEvent)
{
    const FKey key = KeyEvent.GetKey();

    if (key == EKeys::Tab && !FirstConsoleSuggestion.IsEmpty())
    {
        return ApplyConsoleSuggestion(FirstConsoleSuggestion);
    }

    if (key == EKeys::Up)
    {
        if (ConsoleCommandHistory.IsEmpty())
        {
            return FReply::Unhandled();
        }

        if (ConsoleHistoryIndex >= ConsoleCommandHistory.Num())
        {
            ConsoleHistoryDraft = ConsoleInput.IsValid()
                ? ConsoleInput->GetText().ToString()
                : FString();
            ConsoleHistoryIndex = ConsoleCommandHistory.Num() - 1;
        }
        else if (ConsoleHistoryIndex > 0)
        {
            --ConsoleHistoryIndex;
        }

        SetConsoleInputText(
            ConsoleCommandHistory[ConsoleHistoryIndex]);
        return FReply::Handled();
    }

    if (key == EKeys::Down)
    {
        if (ConsoleCommandHistory.IsEmpty() ||
            ConsoleHistoryIndex >= ConsoleCommandHistory.Num())
        {
            return FReply::Unhandled();
        }

        ++ConsoleHistoryIndex;

        if (ConsoleHistoryIndex == ConsoleCommandHistory.Num())
        {
            SetConsoleInputText(ConsoleHistoryDraft);
        }
        else
        {
            SetConsoleInputText(
                ConsoleCommandHistory[ConsoleHistoryIndex]);
        }

        return FReply::Handled();
    }

    return FReply::Unhandled();
}

FReply SMiaIAEditorPanel::HandleConsoleSend()
{
    if (ConsoleInput.IsValid())
    {
        HandleConsoleCommandCommitted(
            ConsoleInput->GetText(),
            ETextCommit::OnEnter);
    }

    return FReply::Handled();
}

FReply SMiaIAEditorPanel::ApplyConsoleSuggestion(FString Completion)
{
    if (!Completion.EndsWith(TEXT(" ")))
    {
        Completion += TEXT(" ");
    }

    SetConsoleInputText(Completion);

    if (ConsoleInput.IsValid())
    {
        FSlateApplication::Get().SetKeyboardFocus(
            ConsoleInput,
            EFocusCause::SetDirectly);
    }

    return FReply::Handled();
}

void SMiaIAEditorPanel::RebuildConsoleSuggestions(
    const FString& Input)
{
    FirstConsoleSuggestion.Empty();

    if (!ConsoleSuggestionsContent.IsValid())
    {
        return;
    }

    ConsoleSuggestionsContent->ClearChildren();
    const auto suggestions =
        MiaIA::CLI::MiaIACommandProcessor::GetSuggestions(
            std::string(TCHAR_TO_UTF8(*Input)),
            8);

    for (const auto& suggestion : suggestions)
    {
        const FString completion =
            UTF8_TO_TCHAR(suggestion.Completion.c_str());
        const FText syntax = FText::FromString(
            UTF8_TO_TCHAR(suggestion.Syntax.c_str()));
        const FText description = FText::FromString(
            UTF8_TO_TCHAR(suggestion.Description.c_str()));

        if (FirstConsoleSuggestion.IsEmpty())
        {
            FirstConsoleSuggestion = completion;
        }

        ConsoleSuggestionsContent->AddSlot()
        .AutoHeight()
        .Padding(0.0f, 1.0f)
        [
            SNew(SButton)
            .ButtonStyle(&ButtonStyle)
            .ContentPadding(FMargin(6.0f, 2.0f))
            .ToolTipText(description)
            .OnClicked(
                this,
                &SMiaIAEditorPanel::ApplyConsoleSuggestion,
                completion)
            [
                SNew(STextBlock)
                .Text(syntax)
                .Font(FAppStyle::GetFontStyle(
                    TEXT("SmallFontBold")))
                .AutoWrapText(true)
            ]
        ];
    }
}

void SMiaIAEditorPanel::SetConsoleInputText(const FString& Text)
{
    if (!ConsoleInput.IsValid())
    {
        return;
    }

    bUpdatingConsoleInput = true;
    ConsoleInput->SetText(FText::FromString(Text));
    ConsoleInput->GoTo(ETextLocation::EndOfDocument);
    bUpdatingConsoleInput = false;
    RebuildConsoleSuggestions(Text);
}

void SMiaIAEditorPanel::UpdateConsoleOutput()
{
    if (!ConsoleOutput.IsValid())
    {
        return;
    }

    ConsoleOutput->SetText(FText::FromString(ConsoleHistory));
    ConsoleOutput->ScrollTo(ETextLocation::EndOfDocument);
}

bool SMiaIAEditorPanel::CanResume() const
{
    return Session.Status == EMiaIATrainingSessionStatus::Active &&
        Debug.Phase == EMiaIATrainingDebugPhase::Idle;
}

bool SMiaIAEditorPanel::CanPause() const
{
    return Session.Status == EMiaIATrainingSessionStatus::Running;
}

bool SMiaIAEditorPanel::CanStartDebug() const
{
    return !bNetworkPreview &&
        !bNetworkRequiresCompactTopology &&
        Session.Status == EMiaIATrainingSessionStatus::Active &&
        Session.CompletedSteps < Session.TotalSteps &&
        (Debug.Phase == EMiaIATrainingDebugPhase::Idle ||
            Debug.Phase == EMiaIATrainingDebugPhase::Committed);
}

bool SMiaIAEditorPanel::CanAdvanceDebug() const
{
    return !bNetworkPreview &&
        !bNetworkRequiresCompactTopology &&
        Debug.Phase != EMiaIATrainingDebugPhase::Idle &&
        Debug.Phase != EMiaIATrainingDebugPhase::Committed;
}

bool SMiaIAEditorPanel::CanCancelDebug() const
{
    return !bNetworkPreview &&
        Debug.Phase != EMiaIATrainingDebugPhase::Idle &&
        Debug.Phase != EMiaIATrainingDebugPhase::Committed;
}

bool SMiaIAEditorPanel::CanEditNetworkParameters() const
{
    const bool debugOwnsCandidate =
        Debug.Phase >= EMiaIATrainingDebugPhase::BeforeForward &&
        Debug.Phase < EMiaIATrainingDebugPhase::Committed;
    return Session.Status != EMiaIATrainingSessionStatus::Running &&
        !debugOwnsCandidate;
}

FText SMiaIAEditorPanel::SessionStatusText() const
{
    return FText::Format(
        LOCTEXT("SessionStatus", "Session: {0}  ·  {1}/{2} steps"),
        SessionStatusName(Session.Status),
        FText::AsNumber(Session.CompletedSteps),
        FText::AsNumber(Session.TotalSteps));
}

FText SMiaIAEditorPanel::DebugPhaseText() const
{
    return FText::Format(
        LOCTEXT("DebugPhase", "Debug: {0}"),
        DebugPhaseName(Debug.Phase));
}

FText SMiaIAEditorPanel::NetworkSummaryText() const
{
    if (FocusedLayerId >= 0 && !Network.Layers.IsEmpty())
    {
        const FMiaIALayerSnapshot& layer = Network.Layers[0];
        return FText::Format(
            LOCTEXT(
                "LayerDetailSummary",
                "Network  >  L{0} {1}  |  Layer detail  |  {2} neurons"),
            FText::AsNumber(layer.Order),
            FText::FromString(layer.Name),
            FText::AsNumber(layer.Neurons.Num()));
    }

    if (IsNetworkAggregateOverview())
    {
        return FText::Format(
            LOCTEXT(
                "AggregateNetworkSummary",
                "Network overview  |  {0} inputs  |  {1} layers  |  {2} outputs  |  {3} neurons  |  {4} connections"),
            FText::AsNumber(NetworkInputCount()),
            FText::AsNumber(NetworkOverview.Layers.Num()),
            FText::AsNumber(NetworkOutputCount()),
            FText::AsNumber(NetworkOverview.NeuronCount),
            FText::AsNumber(NetworkOverview.ConnectionCount));
    }

    return bCompactTopology
        ? FText::Format(
            LOCTEXT(
                "CompactNetworkSummary",
                "Network  >  Layers  |  Compact  |  {0} layers  |  {1} neurons  |  {2} connections"),
            FText::AsNumber(NetworkOverview.Layers.Num()),
            FText::AsNumber(NetworkOverview.NeuronCount),
            FText::AsNumber(NetworkOverview.ConnectionCount))
        : FText::Format(
            LOCTEXT(
                "NetworkSummary",
                "Network topology  |  {0} layers  |  {1} neurons  |  {2} connections"),
            FText::AsNumber(NetworkOverview.Layers.Num()),
            FText::AsNumber(NetworkOverview.NeuronCount),
            FText::AsNumber(NetworkOverview.ConnectionCount));
}

bool SMiaIAEditorPanel::IsNetworkAggregateOverview() const
{
    return bNetworkPreview && !NetworkOverview.Layers.IsEmpty();
}

int64 SMiaIAEditorPanel::NetworkInputCount() const
{
    return NetworkOverview.Layers.IsEmpty()
        ? 0
        : NetworkOverview.Layers[0].NeuronCount;
}

int64 SMiaIAEditorPanel::NetworkOutputCount() const
{
    return NetworkOverview.Layers.IsEmpty()
        ? 0
        : NetworkOverview.Layers.Last().NeuronCount;
}

EVisibility SMiaIAEditorPanel::LayerDetailVisibility() const
{
    return !bNetworkPreview && !NetworkOverview.Layers.IsEmpty()
        ? EVisibility::Visible
        : EVisibility::Collapsed;
}

FText SMiaIAEditorPanel::PositiveMetricLegendText() const
{
    if (FMiaIAInstanceService::ForwardTraceState(MiaIAInstance).Active)
    {
        return LOCTEXT(
            "PositiveContributionLegend",
            "Positive contribution");
    }

    switch (Debug.Phase)
    {
    case EMiaIATrainingDebugPhase::BackwardComplete:
        return LOCTEXT("PositiveGradientLegend", "Positive gradient");
    case EMiaIATrainingDebugPhase::UpdateComplete:
        return LOCTEXT("PositiveDeltaLegend", "Positive delta");
    default:
        return LOCTEXT("PositiveWeightLegend", "Positive weight");
    }
}

FText SMiaIAEditorPanel::NegativeMetricLegendText() const
{
    if (FMiaIAInstanceService::ForwardTraceState(MiaIAInstance).Active)
    {
        return LOCTEXT(
            "NegativeContributionLegend",
            "Negative contribution");
    }

    switch (Debug.Phase)
    {
    case EMiaIATrainingDebugPhase::BackwardComplete:
        return LOCTEXT("NegativeGradientLegend", "Negative gradient");
    case EMiaIATrainingDebugPhase::UpdateComplete:
        return LOCTEXT("NegativeDeltaLegend", "Negative delta");
    default:
        return LOCTEXT("NegativeWeightLegend", "Negative weight");
    }
}

FText SMiaIAEditorPanel::ConsoleText() const
{
    return FText::FromString(ConsoleHistory);
}

FText SMiaIAEditorPanel::ForwardTraceSummaryText() const
{
    const MiaIA::Studio::StudioForwardTraceState state =
        FMiaIAInstanceService::ForwardTraceState(MiaIAInstance);
    if (!state.Active)
    {
        return LOCTEXT(
            "ForwardTraceSummaryInactive",
            "No captured execution trace.");
    }

    FString inputText;
    for (const double value : state.Trace.Inputs)
    {
        if (!inputText.IsEmpty())
        {
            inputText += TEXT(", ");
        }
        inputText += FString::SanitizeFloat(value);
    }

    FString outputText;
    for (const double value : state.Trace.Outputs)
    {
        if (!outputText.IsEmpty())
        {
            outputText += TEXT(", ");
        }
        outputText += FString::SanitizeFloat(value);
    }

    FString playbackText = TEXT("Complete");

    if (state.PlaybackStatus !=
        MiaIA::Studio::StudioForwardTracePlaybackStatus::Completed &&
        state.PlaybackFrameIndex < state.PlaybackFrames.size())
    {
        const MiaIA::Studio::StudioForwardTraceFrame& frame =
            state.PlaybackFrames[state.PlaybackFrameIndex];
        const TCHAR* phase = TEXT("Input activations");

        if (frame.Kind ==
            MiaIA::Studio::StudioForwardTraceFrameKind::IncomingSignal)
        {
            phase = TEXT("Incoming signal");
        }
        else if (frame.Kind == MiaIA::Studio::
            StudioForwardTraceFrameKind::LayerActivations)
        {
            phase = TEXT("Layer activations");
        }

        FString layerName = FString::Printf(
            TEXT("Layer %llu"),
            static_cast<uint64>(frame.LayerIndex));

        if (frame.LayerIndex < state.Trace.Layers.size())
        {
            layerName = UTF8_TO_TCHAR(
                state.Trace.Layers[frame.LayerIndex].Name.c_str());
        }

        const TCHAR* status = state.PlaybackStatus ==
            MiaIA::Studio::StudioForwardTracePlaybackStatus::Playing
            ? TEXT("Playing")
            : TEXT("Paused");
        playbackText = FString::Printf(
            TEXT("Frame %llu/%llu | %s | %s | %s"),
            static_cast<uint64>(state.PlaybackFrameIndex + 1),
            static_cast<uint64>(state.PlaybackFrames.size()),
            *layerName,
            phase,
            status);
    }

    return FText::FromString(FString::Printf(
        TEXT("Captured snapshot | Inputs: [%s] | Outputs: [%s]\n%s"),
        *inputText,
        *outputText,
        *playbackText));
}

FText SMiaIAEditorPanel::ForwardTracePlayPauseText() const
{
    const MiaIA::Studio::StudioForwardTraceState state =
        FMiaIAInstanceService::ForwardTraceState(MiaIAInstance);
    return state.PlaybackStatus ==
        MiaIA::Studio::StudioForwardTracePlaybackStatus::Playing
        ? LOCTEXT("PauseForwardTracePlayback", "Pause")
        : LOCTEXT("PlayForwardTracePlayback", "Play");
}

FText SMiaIAEditorPanel::ForwardTraceSpeedText() const
{
    const MiaIA::Studio::StudioForwardTraceState state =
        FMiaIAInstanceService::ForwardTraceState(MiaIAInstance);
    const double duration = state.PlaybackFrameDurationSeconds > 0.0
        ? state.PlaybackFrameDurationSeconds
        : DefaultForwardTraceFrameDurationSeconds;
    return FText::FromString(FString::Printf(
        TEXT("Speed %gx"),
        DefaultForwardTraceFrameDurationSeconds / duration));
}

FText SMiaIAEditorPanel::ForwardTraceSelectionText() const
{
    const MiaIA::Studio::StudioForwardTraceState state =
        FMiaIAInstanceService::ForwardTraceState(MiaIAInstance);
    if (!state.Active || SelectedNeuronIds.Num() != 1 ||
        SelectedNeuronId < 0)
    {
        return FText::GetEmpty();
    }

    for (const MiaIA::Core::ForwardTraceLayerSnapshot& layer :
        state.Trace.Layers)
    {
        for (const MiaIA::Core::ForwardTraceNeuronSnapshot& neuron :
            layer.Neurons)
        {
            if (neuron.Id != static_cast<uint64>(SelectedNeuronId))
            {
                continue;
            }

            return FText::FromString(FString::Printf(
                TEXT(
                    "Execution trace\n"
                    "Weighted input: %g\n"
                    "Bias: %g\n"
                    "Pre-activation: %g\n"
                    "Activation: %g"),
                neuron.WeightedInputSum,
                neuron.Bias,
                neuron.PreActivation,
                neuron.Activation));
        }
    }

    return FText::GetEmpty();
}

FText SMiaIAEditorPanel::SelectionTitle() const
{
    if (IsNetworkAggregateOverview())
    {
        return LOCTEXT("SelectedNetworkAggregate", "Complete network");
    }

    if (const FMiaIALayerOverview* layer =
        FindOverviewLayer(SelectedLayerId))
    {
        return FText::Format(
            LOCTEXT("SelectedAggregateLayer", "Layer L{0}: {1}"),
            FText::AsNumber(layer->Order),
            FText::FromString(layer->Name));
    }

    if (SelectedConnectionId >= 0)
    {
        return FText::Format(
            LOCTEXT("SelectedConnection", "Connection #{0}"),
            FText::AsNumber(SelectedConnectionId));
    }

    if (SelectedNeuronIds.Num() > 1)
    {
        return FText::Format(
            LOCTEXT(
                "SelectedNeuronGroup",
                "{0} neurons selected"),
            FText::AsNumber(SelectedNeuronIds.Num()));
    }

    return SelectedNeuronId < 0
        ? LOCTEXT("NoSelection", "No item selected")
        : FText::Format(
            LOCTEXT("SelectedNeuron", "Neuron #{0}"),
            FText::AsNumber(SelectedNeuronId));
}

FText SMiaIAEditorPanel::SelectionContextText() const
{
    if (IsNetworkAggregateOverview())
    {
        return LOCTEXT(
            "SelectedNetworkContext",
            "Compact whole-model overview");
    }

    if (const FMiaIALayerOverview* layer =
        FindOverviewLayer(SelectedLayerId))
    {
        return FText::Format(
            LOCTEXT("SelectedLayerActivation", "Activation: {0}"),
            FText::FromString(ActivationName(layer->Activation)));
    }

    if (const FMiaIAConnectionSnapshot* connection =
        FindConnection(SelectedConnectionId))
    {
        return FText::Format(
            LOCTEXT("SelectedEndpoints", "From #{0} to #{1}"),
            FText::AsNumber(connection->FromNeuron),
            FText::AsNumber(connection->ToNeuron));
    }

    if (SelectedNeuronIds.Num() > 1)
    {
        int32 selectedLayerCount = 0;

        for (const FMiaIALayerSnapshot& layer : Network.Layers)
        {
            if (layer.Neurons.ContainsByPredicate(
                [this](const FMiaIANeuronSnapshot& neuron)
                {
                    return SelectedNeuronIds.Contains(neuron.Id);
                }))
            {
                ++selectedLayerCount;
            }
        }

        return FText::Format(
            LOCTEXT("SelectedLayerCount", "Layers: {0}"),
            FText::AsNumber(selectedLayerCount));
    }

    return FText::Format(
        LOCTEXT("SelectedLayer", "Layer: {0}"),
        FText::FromString(SelectedLayerName));
}

FText SMiaIAEditorPanel::SelectionPrimaryText() const
{
    if (IsNetworkAggregateOverview())
    {
        return FText::Format(
            LOCTEXT(
                "SelectedNetworkDimensions",
                "Inputs: {0}  |  Layers: {1}  |  Outputs: {2}"),
            FText::AsNumber(NetworkInputCount()),
            FText::AsNumber(NetworkOverview.Layers.Num()),
            FText::AsNumber(NetworkOutputCount()));
    }

    if (const FMiaIALayerOverview* layer =
        FindOverviewLayer(SelectedLayerId))
    {
        return FText::Format(
            LOCTEXT("SelectedLayerNeuronCount", "Neurons: {0}"),
            FText::AsNumber(layer->NeuronCount));
    }

    if (const FMiaIAConnectionSnapshot* connection =
        FindConnection(SelectedConnectionId))
    {
        const double weight = bHasDebugConnection
            ? DebugConnection.PublicWeight
            : connection->Weight;
        return FText::Format(
            LOCTEXT("SelectedPublicWeight", "Public weight: {0}"),
            FText::AsNumber(weight));
    }

    if (SelectedNeuronIds.Num() > 1)
    {
        double activationTotal = 0.0;
        int32 neuronCount = 0;

        for (const int64 neuronId : SelectedNeuronIds)
        {
            if (const FMiaIANeuronSnapshot* neuron = FindNeuron(neuronId))
            {
                activationTotal += neuron->Activation;
                ++neuronCount;
            }
        }

        return FText::Format(
            LOCTEXT(
                "SelectedAverageActivation",
                "Average activation: {0}"),
            FText::AsNumber(
                neuronCount > 0 ? activationTotal / neuronCount : 0.0));
    }

    const FMiaIANeuronSnapshot* neuron = FindNeuron(SelectedNeuronId);
    const double activation = bHasDebugNeuron
        ? DebugNeuron.CandidateActivation
        : neuron ? neuron->Activation : 0.0;
    return FText::Format(
        LOCTEXT("SelectedActivation", "Activation: {0}"),
        FText::AsNumber(activation));
}

FText SMiaIAEditorPanel::SelectionSecondaryText() const
{
    if (IsNetworkAggregateOverview())
    {
        return FText::Format(
            LOCTEXT(
                "SelectedNetworkSize",
                "Neurons: {0}  |  Connections: {1}"),
            FText::AsNumber(NetworkOverview.NeuronCount),
            FText::AsNumber(NetworkOverview.ConnectionCount));
    }

    if (FindOverviewLayer(SelectedLayerId))
    {
        return LOCTEXT(
            "SelectedLayerOpenHint",
            "Double-click the aggregate or press Enter to open it.");
    }

    if (SelectedConnectionId >= 0)
    {
        return bHasDebugConnection
            ? FText::Format(
                LOCTEXT(
                    "SelectedCandidateWeight",
                    "Candidate weight: {0}"),
                FText::AsNumber(DebugConnection.CandidateWeight))
            : LOCTEXT(
                "CandidateWeightUnavailable",
                "Candidate weight: unavailable");
    }

    if (SelectedNeuronIds.Num() > 1)
    {
        double biasTotal = 0.0;
        int32 neuronCount = 0;

        for (const int64 neuronId : SelectedNeuronIds)
        {
            if (const FMiaIANeuronSnapshot* neuron = FindNeuron(neuronId))
            {
                biasTotal += neuron->Bias;
                ++neuronCount;
            }
        }

        return FText::Format(
            LOCTEXT("SelectedAverageBias", "Average bias: {0}"),
            FText::AsNumber(
                neuronCount > 0 ? biasTotal / neuronCount : 0.0));
    }

    const FMiaIANeuronSnapshot* neuron = FindNeuron(SelectedNeuronId);
    const double bias = bHasDebugNeuron
        ? DebugNeuron.CandidateBias
        : neuron ? neuron->Bias : 0.0;
    return FText::Format(
        LOCTEXT("SelectedBias", "Bias: {0}"),
        FText::AsNumber(bias));
}

FText SMiaIAEditorPanel::SelectionGradientText() const
{
    if (IsNetworkAggregateOverview())
    {
        return LOCTEXT(
            "SelectedNetworkIndicatorMeaning",
            "Left: inputs  |  Above: layers  |  Right: outputs");
    }

    if (FindOverviewLayer(SelectedLayerId))
    {
        return LOCTEXT(
            "SelectedLayerGradientHint",
            "Aggregate gradients are not displayed in the overview.");
    }

    if (SelectedNeuronIds.Num() > 1)
    {
        return LOCTEXT(
            "GroupGradientHint",
            "Select one neuron to inspect its gradients.");
    }

    if (SelectedConnectionId >= 0)
    {
        return bHasDebugConnection && DebugConnection.bHasGradient
            ? FText::Format(
                LOCTEXT("SelectedWeightGradient", "Weight gradient: {0}"),
                FText::AsNumber(DebugConnection.WeightGradient))
            : LOCTEXT(
                "WeightGradientUnavailable",
                "Weight gradient: unavailable");
    }

    return bHasDebugNeuron && DebugNeuron.bHasGradients
        ? FText::Format(
            LOCTEXT("SelectedBiasGradient", "Bias gradient: {0}"),
            FText::AsNumber(DebugNeuron.BiasGradient))
        : LOCTEXT("BiasGradientUnavailable", "Bias gradient: unavailable");
}

FText SMiaIAEditorPanel::SelectionUpdateText() const
{
    if (IsNetworkAggregateOverview())
    {
        return LOCTEXT(
            "SelectedNetworkOpenHint",
            "Click the central node or press Enter to inspect the layers.");
    }

    if (FindOverviewLayer(SelectedLayerId))
    {
        return LOCTEXT(
            "SelectedLayerDetailHint",
            "Open the layer to inspect individual neuron values.");
    }

    if (SelectedNeuronIds.Num() > 1)
    {
        return LOCTEXT(
            "GroupDragHint",
            "Drag any selected neuron to move the group.");
    }

    if (SelectedConnectionId >= 0)
    {
        return bHasDebugConnection && DebugConnection.bHasUpdate
            ? FText::Format(
                LOCTEXT(
                    "SelectedWeightUpdate",
                    "Delta: {0}\nUpdated weight: {1}"),
                FText::AsNumber(DebugConnection.Delta),
                FText::AsNumber(DebugConnection.UpdatedWeight))
            : LOCTEXT("WeightUpdateUnavailable", "Weight update: unavailable");
    }

    return bHasDebugNeuron && DebugNeuron.bHasUpdate
        ? FText::Format(
            LOCTEXT(
                "SelectedBiasUpdate",
                "Delta: {0}\nUpdated bias: {1}"),
            FText::AsNumber(DebugNeuron.Delta),
            FText::AsNumber(DebugNeuron.UpdatedBias))
        : LOCTEXT("BiasUpdateUnavailable", "Bias update: unavailable");
}

EVisibility SMiaIAEditorPanel::NeuronBiasEditorVisibility() const
{
    return bHasNeuronInspection &&
        SelectedConnectionId < 0 &&
        NeuronInspection.Context.LayerOrder > 0
        ? EVisibility::Visible
        : EVisibility::Collapsed;
}

EVisibility SMiaIAEditorPanel::InputNeuronBiasNoticeVisibility() const
{
    return bHasNeuronInspection &&
        SelectedConnectionId < 0 &&
        NeuronInspection.Context.LayerOrder == 0
        ? EVisibility::Visible
        : EVisibility::Collapsed;
}

EVisibility SMiaIAEditorPanel::ConnectionWeightEditorVisibility() const
{
    return bHasConnectionInspection && SelectedConnectionId >= 0
        ? EVisibility::Visible
        : EVisibility::Collapsed;
}

FText SMiaIAEditorPanel::SelectionRelationshipsText() const
{
    if (IsNetworkAggregateOverview())
    {
        return LOCTEXT(
            "SelectedNetworkRelationshipsHint",
            "The central node summarizes the complete model. Its surrounding parallel indicators represent exact input, layer, and output counts; very large counts may be visually capped while the numeric values remain exact.");
    }

    if (FindOverviewLayer(SelectedLayerId))
    {
        return LOCTEXT(
            "SelectedLayerRelationshipsHint",
            "The compact node summarizes one complete layer. Opening it loads only that layer through the SDK, without requesting the complete network snapshot.");
    }

    if (SelectedNeuronIds.Num() > 1)
    {
        return LOCTEXT(
            "GroupRelationshipsHint",
            "Select one neuron to inspect its connections.");
    }

    if (SelectedConnectionId >= 0)
    {
        if (!bHasConnectionInspection)
        {
            return LOCTEXT(
                "ConnectionRelationshipsUnavailable",
                "Endpoint details are unavailable.");
        }

        const FMiaIANeuronContext& from =
            ConnectionInspection.FromNeuron;
        const FMiaIANeuronContext& to = ConnectionInspection.ToNeuron;
        return FText::FromString(FString::Printf(
            TEXT(
                "Endpoints\n"
                "From #%lld | Layer %lld: %s | %s\n"
                "Activation: %g | Bias: %g\n\n"
                "To #%lld | Layer %lld: %s | %s\n"
                "Activation: %g | Bias: %g"),
            from.Neuron.Id,
            from.LayerOrder,
            *from.LayerName,
            *ActivationName(from.LayerActivation),
            from.Neuron.Activation,
            from.Neuron.Bias,
            to.Neuron.Id,
            to.LayerOrder,
            *to.LayerName,
            *ActivationName(to.LayerActivation),
            to.Neuron.Activation,
            to.Neuron.Bias));
    }

    if (SelectedNeuronId < 0)
    {
        return FText::GetEmpty();
    }

    if (!bHasNeuronInspection)
    {
        return LOCTEXT(
            "NeuronRelationshipsUnavailable",
            "Connection details are unavailable.");
    }

    return LOCTEXT(
        "NeuronRelationshipExplorerHint",
        "Relationship explorer");
}

FText SMiaIAEditorPanel::SelectedConnectionEndpointText(
    bool bToNeuron) const
{
    const FMiaIAConnectionSnapshot* connection =
        FindConnection(SelectedConnectionId);
    if (connection == nullptr)
    {
        return bToNeuron
            ? LOCTEXT("NavigateToEndpointUnavailable", "Go to To neuron")
            : LOCTEXT(
                "NavigateFromEndpointUnavailable",
                "Go to From neuron");
    }

    return FText::Format(
        bToNeuron
            ? LOCTEXT("NavigateToEndpoint", "Go to To neuron #{0}")
            : LOCTEXT("NavigateFromEndpoint", "Go to From neuron #{0}"),
        FText::AsNumber(
            bToNeuron ? connection->ToNeuron : connection->FromNeuron));
}

FText SMiaIAEditorPanel::RelationshipSortText() const
{
    switch (RelationshipSort)
    {
    case EMiaIANeuronRelationshipSort::Weight:
        return LOCTEXT("RelationshipSortWeightValue", "Weight");
    case EMiaIANeuronRelationshipSort::AbsoluteWeight:
        return LOCTEXT(
            "RelationshipSortAbsoluteWeightValue",
            "Absolute weight");
    case EMiaIANeuronRelationshipSort::ConnectionId:
    default:
        return LOCTEXT("RelationshipSortIdValue", "Connection ID");
    }
}

EVisibility SMiaIAEditorPanel::RelationshipExplorerVisibility() const
{
    return bHasNeuronInspection &&
        SelectedNeuronIds.Num() == 1 &&
        SelectedConnectionId < 0
        ? EVisibility::Visible
        : EVisibility::Collapsed;
}

FSlateColor SMiaIAEditorPanel::PhaseColor(
    EMiaIATrainingDebugPhase Phase) const
{
    const FMiaIAEditorPalette palette =
        FMiaIAEditorTheme::Palette(Theme);
    return static_cast<uint8>(Debug.Phase) >= static_cast<uint8>(Phase)
        ? FSlateColor(palette.Debug)
        : FSlateColor(palette.SubduedText);
}

FSlateColor SMiaIAEditorPanel::BackgroundColor() const
{
    return FSlateColor(FMiaIAEditorTheme::Palette(Theme).Background);
}

FSlateColor SMiaIAEditorPanel::PanelColor() const
{
    return FSlateColor(FMiaIAEditorTheme::Palette(Theme).Panel);
}

FSlateColor SMiaIAEditorPanel::TextColor() const
{
    return FSlateColor(FMiaIAEditorTheme::Palette(Theme).Text);
}

FText SMiaIAEditorPanel::ViewModeText() const
{
    return ViewMode == EMiaIAStudioViewMode::TwoDimensional
        ? LOCTEXT("CurrentTwoDimensionalView", "2D")
        : LOCTEXT("CurrentThreeDimensionalView", "3D");
}

FText SMiaIAEditorPanel::LayoutModeText() const
{
    return VisualizationSettings.Layout.Mode ==
        MiaIA::Studio::StudioLayoutMode::Packed
        ? LOCTEXT("PackedLayoutModeText", "Layout: Packed")
        : LOCTEXT("ExpandedLayoutModeText", "Layout: Expanded");
}

FText SMiaIAEditorPanel::ThemeText() const
{
    return FMiaIAEditorTheme::DisplayName(Theme);
}

FText SMiaIAEditorPanel::DataRefreshText() const
{
    return DataRefreshModeDisplayName(DataRefreshMode);
}

FText SMiaIAEditorPanel::TopologyLimitsText() const
{
    return FText::Format(
        LOCTEXT("CurrentTopologyLimits", "{0} / {1}"),
        FText::AsNumber(DetailedNeuronLimit),
        FText::AsNumber(DetailedConnectionLimit));
}

double SMiaIAEditorPanel::DataRefreshInterval() const
{
    switch (DataRefreshMode)
    {
    case EMiaIADataRefreshMode::OneHz:
        return 1.0;
    case EMiaIADataRefreshMode::TwoHz:
        return 0.5;
    case EMiaIADataRefreshMode::FourHz:
        return 0.25;
    case EMiaIADataRefreshMode::TenHz:
        return 0.1;
    case EMiaIADataRefreshMode::Adaptive:
    default:
        return Session.Status == EMiaIATrainingSessionStatus::Running
            ? 0.25
            : 1.0;
    }
}

FText SMiaIAEditorPanel::TopologyWorkspaceText() const
{
    return bTopologyWorkspaceExpanded
        ? LOCTEXT("RestoreTopologyPanels", "Restore panels")
        : LOCTEXT("ExpandTopologyView", "Expand view");
}

FText SMiaIAEditorPanel::BreakpointKindText() const
{
    return BreakpointKindName(BreakpointKind);
}

FText SMiaIAEditorPanel::BreakpointPhaseText() const
{
    return DebugPhaseName(BreakpointPhase);
}

FText SMiaIAEditorPanel::LastBreakpointHitText() const
{
    if (!Session.bHasBreakpointHit)
    {
        return LOCTEXT(
            "NoBreakpointHit",
            "Last hit: none. Automatic training pauses at a safe sample boundary.");
    }

    const FMiaIATrainingBreakpointHit& hit =
        Session.LastBreakpointHit;

    if (hit.TargetId == 0)
    {
        return FText::Format(
            LOCTEXT(
                "LastPhaseBreakpointHit",
                "Last hit: #{0} at {1}, sample {2}, step {3}."),
            FText::AsNumber(hit.BreakpointId),
            DebugPhaseName(hit.Phase),
            FText::AsNumber(hit.SampleIndex),
            FText::AsNumber(hit.StepIndex));
    }

    return FText::Format(
        LOCTEXT(
            "LastValueBreakpointHit",
            "Last hit: #{0} at {1}, target {2}, observed {3}, threshold {4}."),
        FText::AsNumber(hit.BreakpointId),
        DebugPhaseName(hit.Phase),
        FText::AsNumber(hit.TargetId),
        FText::AsNumber(hit.ObservedValue),
        FText::AsNumber(hit.Threshold));
}

EVisibility SMiaIAEditorPanel::BreakpointPhaseVisibility() const
{
    return BreakpointKind == EMiaIATrainingBreakpointKind::Phase
        ? EVisibility::Visible
        : EVisibility::Collapsed;
}

EVisibility SMiaIAEditorPanel::BreakpointTargetVisibility() const
{
    return BreakpointKind == EMiaIATrainingBreakpointKind::Phase
        ? EVisibility::Collapsed
        : EVisibility::Visible;
}

#undef LOCTEXT_NAMESPACE
