#pragma once

#include "MiaIABlueprintTypes.h"
#include "Styling/MiaIAEditorTheme.h"
#include "Widgets/SCompoundWidget.h"

class SMiaIANetworkView;
class SEditableTextBox;
class SMultiLineEditableText;
class SScrollBar;
class SVerticalBox;
class SWidgetSwitcher;

class IDESTUDIO_API SMiaIAEditorPanel final : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SMiaIAEditorPanel)
        : _StandaloneMode(false)
    {
    }
        SLATE_ARGUMENT(bool, StandaloneMode)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

private:
    void RefreshData();
    void RebuildExplorer();
    void SelectNeuron(int64 NeuronId);
    void SelectConnection(int64 ConnectionId);
    const FMiaIANeuronSnapshot* FindNeuron(int64 NeuronId) const;
    const FMiaIAConnectionSnapshot* FindConnection(
        int64 ConnectionId) const;

    EActiveTimerReturnType HandleRefreshTimer(
        double CurrentTime,
        float DeltaTime);
    FReply HandleRefresh();
    FReply HandleFitView();
    FReply HandleResetLayout();
    FReply HandleResume();
    FReply HandlePause();
    FReply HandleStartDebug();
    FReply HandleAdvanceDebug();
    FReply HandleCancelDebug();
    FReply HandleExit();
    FReply SelectBottomTab(int32 TabIndex);
    TSharedRef<SWidget> BuildThemeMenu();
    FReply SelectTheme(EMiaIAEditorTheme InTheme);
    void RefreshWidgetStyles();
    void HandleConsoleCommandCommitted(
        const FText& Text,
        ETextCommit::Type CommitType);
    void HandleConsoleTextChanged(const FText& Text);
    FReply HandleConsoleInputKeyDown(
        const FGeometry& Geometry,
        const FKeyEvent& KeyEvent);
    FReply HandleConsoleSend();
    FReply ApplyConsoleSuggestion(FString Completion);
    void RebuildConsoleSuggestions(const FString& Input);
    void SetConsoleInputText(const FString& Text);
    void UpdateConsoleOutput();

    bool CanResume() const;
    bool CanPause() const;
    bool CanStartDebug() const;
    bool CanAdvanceDebug() const;
    bool CanCancelDebug() const;

    FText SessionStatusText() const;
    FText DebugPhaseText() const;
    FText NetworkSummaryText() const;
    FText PositiveMetricLegendText() const;
    FText NegativeMetricLegendText() const;
    FText ConsoleText() const;
    FText SelectionTitle() const;
    FText SelectionContextText() const;
    FText SelectionPrimaryText() const;
    FText SelectionSecondaryText() const;
    FText SelectionGradientText() const;
    FText SelectionUpdateText() const;
    FSlateColor PhaseColor(EMiaIATrainingDebugPhase Phase) const;
    FSlateColor BackgroundColor() const;
    FSlateColor PanelColor() const;
    FSlateColor TextColor() const;
    FText ThemeText() const;

    FMiaIANetworkSnapshot Network;
    FMiaIANetworkOverview NetworkOverview;
    FMiaIATrainingSessionSnapshot Session;
    FMiaIATrainingDebugSnapshot Debug;
    FMiaIATrainingDebugNeuron DebugNeuron;
    FMiaIATrainingDebugConnection DebugConnection;
    int64 SelectedNeuronId{-1};
    int64 SelectedConnectionId{-1};
    FString SelectedLayerName;
    FString TopologyKey;
    FString ConsoleHistory;
    TArray<FString> ConsoleCommandHistory;
    FString ConsoleHistoryDraft;
    FString FirstConsoleSuggestion;
    int32 ConsoleHistoryIndex{};
    EMiaIAEditorTheme Theme{EMiaIAEditorTheme::FollowUnreal};
    FButtonStyle ButtonStyle;
    FButtonStyle ExplorerButtonStyle;
    FComboButtonStyle ComboButtonStyle;
    FEditableTextBoxStyle InputStyle;
    FScrollBarStyle ScrollBarStyle;
    FSplitterStyle SplitterStyle;
    bool bHasDebugNeuron{};
    bool bHasDebugConnection{};
    bool bUpdatingConsoleInput{};
    bool bCompactTopology{};
    bool bStandaloneMode{};

    TSharedPtr<SVerticalBox> ExplorerContent;
    TSharedPtr<SVerticalBox> ConsoleSuggestionsContent;
    TSharedPtr<SMiaIANetworkView> NetworkView;
    TSharedPtr<SWidgetSwitcher> BottomSwitcher;
    TSharedPtr<SMultiLineEditableText> ConsoleOutput;
    TSharedPtr<SScrollBar> ConsoleOutputScrollBar;
    TSharedPtr<SEditableTextBox> ConsoleInput;
};
