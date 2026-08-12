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
    constexpr TCHAR ConnectionDisplaySettingsKey[] =
        TEXT("ConnectionDisplay");
    constexpr TCHAR NeuronScaleSettingsKey[] = TEXT("NeuronScale");
    constexpr TCHAR ConnectionScaleSettingsKey[] = TEXT("ConnectionScale");
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
                            SNew(STextBlock)
                            .Text(this, &SMiaIAEditorPanel::NetworkSummaryText)
                            .Font(FAppStyle::GetFontStyle(TEXT("NormalFontBold")))
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
                                .OnNeuronNavigationRequested(
                                    this,
                                    &SMiaIAEditorPanel::NavigateNeuron)
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
                                .OnNeuronNavigationRequested(
                                    this,
                                    &SMiaIAEditorPanel::NavigateNeuron)
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
                        .FillHeight(1.0f)
                        .Padding(0.0f, 8.0f, 0.0f, 0.0f)
                        [
                            SNew(SScrollBox)
                            .ScrollBarStyle(&ScrollBarStyle)
                            + SScrollBox::Slot()
                            [
                                SNew(STextBlock)
                                .Text(
                                    this,
                                    &SMiaIAEditorPanel::
                                        SelectionRelationshipsText)
                                .AutoWrapText(true)
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
    RegisterActiveTimer(
        0.1f,
        FWidgetActiveTimerDelegate::CreateSP(
            this,
            &SMiaIAEditorPanel::HandleRefreshTimer));
}

void SMiaIAEditorPanel::RefreshData()
{
    PeriodicRefreshElapsedSeconds = 0.0;
    NetworkOverview = UMiaIABlueprintLibrary::GetNetworkOverview();
    bCompactTopology =
        MiaIA::Studio::StudioTopologyBuilder::RequiresCompactMode(
            static_cast<std::size_t>(NetworkOverview.NeuronCount),
            static_cast<std::size_t>(NetworkOverview.ConnectionCount),
            static_cast<std::size_t>(DetailedNeuronLimit),
            static_cast<std::size_t>(DetailedConnectionLimit));
    Session = UMiaIABlueprintLibrary::GetTrainingSession();
    Breakpoints = Session.Breakpoints;
    const FString newBreakpointKey =
        BuildBreakpointKey(Breakpoints, Session);
    const bool breakpointsChanged =
        newBreakpointKey != BreakpointKey;
    BreakpointKey = newBreakpointKey;

    if (bCompactTopology)
    {
        Network = FMiaIANetworkSnapshot{};
        Debug = FMiaIATrainingDebugSnapshot{};
    }
    else
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
        ? BuildOverviewKey(NetworkOverview)
        : BuildTopologyKey(Network);
    const bool topologyChanged = newTopologyKey != TopologyKey;
    TopologyKey = newTopologyKey;

    if (!FindConnection(SelectedConnectionId))
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
        NetworkView->SetOverview(NetworkOverview, bCompactTopology);
        NetworkView->SetDebugSnapshot(Debug);
        NetworkView->SetSelectedNeurons(
            SelectedNeuronIds,
            SelectedNeuronId);
        NetworkView->SetSelectedConnection(SelectedConnectionId);
    }

    if (Network3DView.IsValid())
    {
        Network3DView->SetSnapshot(Network);
        Network3DView->SetOverview(
            NetworkOverview,
            bCompactTopology);
        Network3DView->SetDebugSnapshot(Debug);
        Network3DView->SetSelectedNeurons(
            SelectedNeuronIds,
            SelectedNeuronId);
        Network3DView->SetSelectedConnection(SelectedConnectionId);
    }

    if (topologyChanged)
    {
        RebuildExplorer();
    }

    if (breakpointsChanged)
    {
        RebuildBreakpoints();
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

void SMiaIAEditorPanel::RebuildExplorer()
{
    if (!ExplorerContent.IsValid())
    {
        return;
    }

    ExplorerContent->ClearChildren();
    ExplorerContent->AddSlot()
    .AutoHeight()
    .Padding(2.0f, 4.0f, 2.0f, 8.0f)
    [
        SNew(STextBlock)
        .Text(LOCTEXT("Explorer", "Model explorer"))
        .Font(FAppStyle::GetFontStyle(TEXT("NormalFontBold")))
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
        ExplorerContent->AddSlot()
        .AutoHeight()
        .Padding(2.0f, 5.0f)
        [
            SNew(STextBlock)
            .Text(FText::Format(
                LOCTEXT("LayerEntry", "{0}  ·  {1} neurons"),
                FText::FromString(layer.Name),
                FText::AsNumber(layer.Neurons.Num())))
        ];

        for (const FMiaIANeuronSnapshot& neuron : layer.Neurons)
        {
            const int64 neuronId = neuron.Id;
            ExplorerContent->AddSlot()
            .AutoHeight()
            .Padding(12.0f, 1.0f, 2.0f, 1.0f)
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
                        LOCTEXT("NeuronEntry", "Neuron #{0}"),
                        FText::AsNumber(neuronId)))
                ]
            ];
        }
    }

    if (!Network.Connections.IsEmpty())
    {
        ExplorerContent->AddSlot()
        .AutoHeight()
        .Padding(2.0f, 10.0f, 2.0f, 5.0f)
        [
            SNew(STextBlock)
            .Text(FText::Format(
                LOCTEXT("ConnectionsEntry", "Connections  |  {0}"),
                FText::AsNumber(Network.Connections.Num())))
        ];

        for (const FMiaIAConnectionSnapshot& connection :
            Network.Connections)
        {
            const int64 connectionId = connection.Id;
            const int64 fromNeuron = connection.FromNeuron;
            const int64 toNeuron = connection.ToNeuron;
            ExplorerContent->AddSlot()
            .AutoHeight()
            .Padding(12.0f, 1.0f, 2.0f, 1.0f)
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
                            "ConnectionEntry",
                            "#{0}: {1} -> {2}"),
                        FText::AsNumber(connectionId),
                        FText::AsNumber(fromNeuron),
                        FText::AsNumber(toNeuron)))
                ]
            ];
        }
    }
}

void SMiaIAEditorPanel::SelectNeuron(int64 NeuronId)
{
    SelectedNeuronId = NeuronId;
    SelectedNeuronIds.Reset();

    if (NeuronId >= 0)
    {
        SelectedNeuronIds.Add(NeuronId);
    }

    SelectedConnectionId = -1;
    RefreshData();
}

void SMiaIAEditorPanel::SelectNeurons(
    const TSet<int64>& NeuronIds,
    int64 PrimaryNeuronId)
{
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
    RefreshData();
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
    SelectedConnectionId = ConnectionId;
    SelectedNeuronId = -1;
    SelectedNeuronIds.Reset();
    RefreshData();
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
    return Network.Connections.FindByPredicate(
        [ConnectionId](const FMiaIAConnectionSnapshot& connection)
        {
            return connection.Id == ConnectionId;
        });
}

EActiveTimerReturnType SMiaIAEditorPanel::HandleRefreshTimer(
    double,
    float DeltaTime)
{
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
                    .Text(LOCTEXT("HorizontalLayout", "Horizontal"))
                    .ToolTipText(LOCTEXT(
                        "HorizontalLayoutTooltip",
                        "Arrange layers from left to right and neurons from top to bottom."))
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
                    .Text(LOCTEXT("VerticalLayout", "Vertical"))
                    .ToolTipText(LOCTEXT(
                        "VerticalLayoutTooltip",
                        "Arrange layers from top to bottom and neurons from left to right."))
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
    VisualizationSettings.Layout.Orientation = InOrientation;
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
                    "Inspector connections per direction"))
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
                    "Maximum incoming and outgoing connections shown for one selected neuron."))
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
            "Click the topology, then use the arrow keys in the visible direction; their layer/neuron meaning follows Horizontal or Vertical flow.\n"
            "Mouse wheel: zoom. Middle drag: pan. Right drag pans in 2D and orbits in 3D.\n"
            "Use Layout for Expanded or Packed placement, Horizontal or Vertical flow, uniform neuron size, minimum gaps, and All or Selected connections. Packed with zero gaps makes symmetric nodes adjacent without overlap.\n\n"
            "INSPECTION AND DEBUG\n"
            "The Inspector follows the current element or group. Use Start debug and Step phase to inspect forward, backward, update, verify, and commit states.\n\n"

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
    return !bCompactTopology &&
        Session.Status == EMiaIATrainingSessionStatus::Active &&
        Session.CompletedSteps < Session.TotalSteps &&
        (Debug.Phase == EMiaIATrainingDebugPhase::Idle ||
            Debug.Phase == EMiaIATrainingDebugPhase::Committed);
}

bool SMiaIAEditorPanel::CanAdvanceDebug() const
{
    return !bCompactTopology &&
        Debug.Phase != EMiaIATrainingDebugPhase::Idle &&
        Debug.Phase != EMiaIATrainingDebugPhase::Committed;
}

bool SMiaIAEditorPanel::CanCancelDebug() const
{
    return Debug.Phase != EMiaIATrainingDebugPhase::Idle &&
        Debug.Phase != EMiaIATrainingDebugPhase::Committed;
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
    return bCompactTopology
        ? FText::Format(
            LOCTEXT(
                "CompactNetworkSummary",
                "Network topology  |  Compact  |  {0} layers  |  {1} neurons  |  {2} connections"),
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

FText SMiaIAEditorPanel::PositiveMetricLegendText() const
{
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

FText SMiaIAEditorPanel::SelectionTitle() const
{
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

FText SMiaIAEditorPanel::SelectionRelationshipsText() const
{
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

    FString details;
    auto appendConnections = [&details](
        const TCHAR* heading,
        const TArray<FMiaIAConnectionSnapshot>& connections,
        int64 totalCount)
    {
        details += FString::Printf(
            TEXT("%s (%d shown of %lld)\n"),
            heading,
            connections.Num(),
            totalCount);

        for (const FMiaIAConnectionSnapshot& connection : connections)
        {
            details += FString::Printf(
                TEXT("#%lld: #%lld -> #%lld | Weight: %g\n"),
                connection.Id,
                connection.FromNeuron,
                connection.ToNeuron,
                connection.Weight);
        }

        const int64 hiddenCount = totalCount - connections.Num();
        if (hiddenCount > 0)
        {
            details += FString::Printf(
                TEXT("... %lld more\n"),
                hiddenCount);
        }
    };

    appendConnections(
        TEXT("Incoming"),
        NeuronInspection.IncomingConnections,
        NeuronInspection.IncomingConnectionCount);
    details += TEXT("\n");
    appendConnections(
        TEXT("Outgoing"),
        NeuronInspection.OutgoingConnections,
        NeuronInspection.OutgoingConnectionCount);
    details.RemoveFromEnd(TEXT("\n"));

    return FText::FromString(details);
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
