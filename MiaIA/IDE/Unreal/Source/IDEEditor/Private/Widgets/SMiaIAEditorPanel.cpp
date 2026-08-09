#include "Widgets/SMiaIAEditorPanel.h"

#include "MiaIACommandProcessor.h"
#include "MiaIABlueprintLibrary.h"
#include "StudioTopology.h"
#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Text/SMultiLineEditableText.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBar.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SMiaIANetworkView.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "MiaIAEditorPanel"

namespace
{
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
    Theme = FMiaIAEditorTheme::Load();
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
                SNew(SHorizontalBox)
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
                        "Restore the automatic neuron layout and fit the view."))
                    .OnClicked(this, &SMiaIAEditorPanel::HandleResetLayout)
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
                            SAssignNew(NetworkView, SMiaIANetworkView)
                            .OnNeuronSelected(
                                this,
                                &SMiaIAEditorPanel::SelectNeuron)
                            .OnConnectionSelected(
                                this,
                                &SMiaIAEditorPanel::SelectConnection)
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
                    ]
                ]
            ]
            + SSplitter::Slot()
            .Value(0.28f)
            [
                SNew(SBorder)
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
                            SNew(STextBlock)
                            .Text(LOCTEXT(
                                "BreakpointsPlanned",
                                "Breakpoint authoring will be connected in the next editor increment."))
                        ]
                    ]
                ]
            ]
        ]
        ]
    ];

    BottomSwitcher->SetActiveWidgetIndex(1);
    NetworkView->SetTheme(Theme);
    RebuildConsoleSuggestions(FString());
    RefreshData();
    RegisterActiveTimer(
        0.25f,
        FWidgetActiveTimerDelegate::CreateSP(
            this,
            &SMiaIAEditorPanel::HandleRefreshTimer));
}

void SMiaIAEditorPanel::RefreshData()
{
    NetworkOverview = UMiaIABlueprintLibrary::GetNetworkOverview();
    bCompactTopology =
        MiaIA::Studio::StudioTopologyBuilder::RequiresCompactMode(
            static_cast<std::size_t>(NetworkOverview.NeuronCount),
            static_cast<std::size_t>(NetworkOverview.ConnectionCount));
    Session = UMiaIABlueprintLibrary::GetTrainingSession();

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

    if (SelectedConnectionId >= 0)
    {
        SelectedNeuronId = -1;
    }
    else if (!FindNeuron(SelectedNeuronId))
    {
        SelectedNeuronId = Network.Layers.IsEmpty() ||
            Network.Layers[0].Neurons.IsEmpty()
            ? -1
            : Network.Layers[0].Neurons[0].Id;
    }

    bHasDebugNeuron = SelectedNeuronId >= 0 &&
        UMiaIABlueprintLibrary::GetDebugNeuron(
            SelectedNeuronId,
            DebugNeuron);
    bHasDebugConnection = SelectedConnectionId >= 0 &&
        UMiaIABlueprintLibrary::GetDebugConnection(
            SelectedConnectionId,
            DebugConnection);

    SelectedLayerName.Reset();

    for (const FMiaIALayerSnapshot& layer : Network.Layers)
    {
        if (layer.Neurons.ContainsByPredicate(
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
        NetworkView->SetSelectedNeuron(SelectedNeuronId);
        NetworkView->SetSelectedConnection(SelectedConnectionId);
    }

    if (topologyChanged)
    {
        RebuildExplorer();
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
    SelectedConnectionId = -1;
    RefreshData();
}

void SMiaIAEditorPanel::SelectConnection(int64 ConnectionId)
{
    SelectedConnectionId = ConnectionId;
    SelectedNeuronId = -1;
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
    float)
{
    RefreshData();
    return EActiveTimerReturnType::Continue;
}

FReply SMiaIAEditorPanel::HandleRefresh()
{
    RefreshData();
    return FReply::Handled();
}

FReply SMiaIAEditorPanel::HandleFitView()
{
    if (NetworkView.IsValid())
    {
        NetworkView->FitView();
    }

    return FReply::Handled();
}

FReply SMiaIAEditorPanel::HandleResetLayout()
{
    if (NetworkView.IsValid())
    {
        NetworkView->ResetLayout();
    }

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

FReply SMiaIAEditorPanel::SelectBottomTab(int32 TabIndex)
{
    if (BottomSwitcher.IsValid())
    {
        BottomSwitcher->SetActiveWidgetIndex(TabIndex);
    }

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

    FSlateApplication::Get().DismissAllMenus();
    Invalidate(EInvalidateWidgetReason::Paint);
    return FReply::Handled();
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

FText SMiaIAEditorPanel::ThemeText() const
{
    return FMiaIAEditorTheme::DisplayName(Theme);
}

#undef LOCTEXT_NAMESPACE
