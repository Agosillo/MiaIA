#pragma once

#include "Containers/Set.h"
#include "MiaIABlueprintTypes.h"
#include "Styling/MiaIAEditorTheme.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/MiaIAVisualizationSettings.h"

class SMiaIANetworkView;
class SMiaIA3DNetworkView;
class SEditableTextBox;
class SMultiLineEditableText;
class SScrollBar;
class SVerticalBox;
class SWidgetSwitcher;
enum class EMiaIANeuronNavigationDirection : uint8;

enum class EMiaIAStudioViewMode : uint8
{
    TwoDimensional,
    ThreeDimensional
};

enum class EMiaIADataRefreshMode : uint8
{
    Adaptive,
    OneHz,
    TwoHz,
    FourHz,
    TenHz
};

enum class EMiaIAProjectPathAction : uint8
{
    None,
    Open,
    SaveAs,
    ImportOnnx,
    ExportOnnx
};

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
    void RebuildRelationshipExplorer();
    void RebuildBreakpoints();
    bool ExpandExplorerForNeuron(int64 NeuronId);
    bool ExpandExplorerForNeurons(const TSet<int64>& NeuronIds);
    FReply HandleToggleExplorerLayer(int64 LayerId);
    FReply HandleToggleExplorerConnections();
    FReply HandleExpandAllExplorer();
    FReply HandleCollapseAllExplorer();
    void SelectNeuron(int64 NeuronId);
    void SelectNeurons(
        const TSet<int64>& NeuronIds,
        int64 PrimaryNeuronId);
    void NavigateNeuron(EMiaIANeuronNavigationDirection Direction);
    void SelectConnection(int64 ConnectionId);
    void SelectLayer(int64 LayerId);
    FReply SelectRelationshipDirection(
        EMiaIANeuronRelationshipDirection InDirection);
    TSharedRef<SWidget> BuildRelationshipSortMenu();
    FReply SelectRelationshipSort(
        EMiaIANeuronRelationshipSort InSort);
    FReply ToggleRelationshipSortDirection();
    void HandleRelationshipMinimumWeightCommitted(
        double InValue,
        ETextCommit::Type CommitType);
    FReply HandlePreviousRelationshipPage();
    FReply HandleNextRelationshipPage();
    FReply HandleSelectRelationshipConnection(int64 ConnectionId);
    FReply HandleNavigateRelationshipNeuron(int64 NeuronId);
    FReply HandleNavigateSelectedConnectionEndpoint(bool bToNeuron);
    void OpenNetworkFromPreview();
    void OpenLayerDetail(int64 LayerId);
    bool NavigateBackFromTopology();
    FReply HandleNavigateBackFromTopology();
    const FMiaIANeuronSnapshot* FindNeuron(int64 NeuronId) const;
    const FMiaIAConnectionSnapshot* FindConnection(
        int64 ConnectionId) const;
    const FMiaIALayerOverview* FindOverviewLayer(int64 LayerId) const;

    EActiveTimerReturnType HandleRefreshTimer(
        double CurrentTime,
        float DeltaTime);
    EActiveTimerReturnType HandleDeferredWorkspaceFit(
        double CurrentTime,
        float DeltaTime);
    FReply HandleRefresh();
    FReply HandleFitView();
    FReply HandleResetLayout();
    TSharedRef<SWidget> BuildLayoutMenu();
    FReply SelectLayoutMode(MiaIA::Studio::StudioLayoutMode InMode);
    FReply SelectLayoutOrientation(
        MiaIA::Studio::StudioLayoutOrientation InOrientation);
    FReply SelectConnectionDisplayMode(
        EMiaIAConnectionDisplayMode InMode);
    void HandleNeuronScaleChanged(float InValue);
    void HandleConnectionScaleChanged(float InValue);
    void HandleNeuronGapChanged(float InValue);
    void HandleLayerGapChanged(float InValue);
    FReply HandleResetVisualizationSettings();
    FReply HandleToggleTopologyWorkspace();
    FReply HandleResume();
    FReply HandlePause();
    FReply HandleStartDebug();
    FReply HandleAdvanceDebug();
    FReply HandleCancelDebug();
    FReply HandleApplySelectedNeuronBias();
    FReply HandleApplySelectedConnectionWeight();
    FReply HandleExit();
    FReply SelectBottomTab(int32 TabIndex);
    TSharedRef<SWidget> BuildProjectMenu();
    FReply HandleNewProject();
    FReply HandleOpenProject();
    FReply HandleSaveProject();
    FReply HandleSaveProjectAs();
    FReply HandleImportOnnx();
    FReply HandleExportOnnx();
    FReply HandleProjectInfo();
    FReply HandleConfirmProjectPath();
    FReply HandleCancelProjectPath();
    TSharedRef<SWidget> BuildViewModeMenu();
    FReply SelectViewMode(EMiaIAStudioViewMode InViewMode);
    TSharedRef<SWidget> BuildThemeMenu();
    FReply SelectTheme(EMiaIAEditorTheme InTheme);
    TSharedRef<SWidget> BuildDataRefreshMenu();
    FReply SelectDataRefreshMode(EMiaIADataRefreshMode InMode);
    TSharedRef<SWidget> BuildTopologyLimitsMenu();
    void HandlePendingNeuronLimitChanged(int32 InValue);
    void HandlePendingConnectionLimitChanged(int32 InValue);
    void HandlePendingInspectorConnectionLimitChanged(int32 InValue);
    FReply HandleApplyTopologyLimits();
    FReply HandleResetTopologyLimits();
    TSharedRef<SWidget> BuildHelpMenu();
    FReply HandleQuickHelp();
    FReply HandleAbout();
    FReply HandleCloseDialog();
    TSharedRef<SWidget> BuildBreakpointKindMenu();
    TSharedRef<SWidget> BuildBreakpointPhaseMenu();
    FReply SelectBreakpointKind(
        EMiaIATrainingBreakpointKind InKind);
    FReply SelectBreakpointPhase(EMiaIATrainingDebugPhase InPhase);
    FReply HandleAddBreakpoint();
    FReply HandleToggleBreakpoint(int64 BreakpointId, bool bEnabled);
    FReply HandleRemoveBreakpoint(int64 BreakpointId);
    FReply HandleClearBreakpoints();
    void ShowDialog(const FText& Title, const FText& Content);
    void ShowProjectPathDialog(EMiaIAProjectPathAction Action);
    EVisibility DialogVisibility() const;
    EVisibility DialogContentVisibility() const;
    EVisibility ProjectPathVisibility() const;
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
    bool CanEditNetworkParameters() const;

    FText SessionStatusText() const;
    FText DebugPhaseText() const;
    FText NetworkSummaryText() const;
    bool IsNetworkAggregateOverview() const;
    int64 NetworkInputCount() const;
    int64 NetworkOutputCount() const;
    EVisibility LayerDetailVisibility() const;
    FText PositiveMetricLegendText() const;
    FText NegativeMetricLegendText() const;
    FText ConsoleText() const;
    FText SelectionTitle() const;
    FText SelectionContextText() const;
    FText SelectionPrimaryText() const;
    FText SelectionSecondaryText() const;
    FText SelectionGradientText() const;
    FText SelectionUpdateText() const;
    FText SelectionRelationshipsText() const;
    FText SelectedConnectionEndpointText(bool bToNeuron) const;
    FText RelationshipSortText() const;
    EVisibility RelationshipExplorerVisibility() const;
    EVisibility NeuronBiasEditorVisibility() const;
    EVisibility InputNeuronBiasNoticeVisibility() const;
    EVisibility ConnectionWeightEditorVisibility() const;
    FSlateColor PhaseColor(EMiaIATrainingDebugPhase Phase) const;
    FSlateColor BackgroundColor() const;
    FSlateColor PanelColor() const;
    FSlateColor TextColor() const;
    FText ViewModeText() const;
    FText LayoutModeText() const;
    FText ThemeText() const;
    FText DataRefreshText() const;
    FText TopologyLimitsText() const;
    FText TopologyWorkspaceText() const;
    FText BreakpointKindText() const;
    FText BreakpointPhaseText() const;
    FText LastBreakpointHitText() const;
    EVisibility BreakpointPhaseVisibility() const;
    EVisibility BreakpointTargetVisibility() const;
    double DataRefreshInterval() const;

    FMiaIANetworkSnapshot Network;
    FMiaIANetworkOverview NetworkOverview;
    FMiaIATrainingSessionSnapshot Session;
    FMiaIATrainingDebugSnapshot Debug;
    FMiaIATrainingDebugNeuron DebugNeuron;
    FMiaIATrainingDebugConnection DebugConnection;
    FMiaIANeuronInspection NeuronInspection;
    FMiaIANeuronRelationshipPage RelationshipPage;
    FMiaIAConnectionInspection ConnectionInspection;
    TArray<FMiaIATrainingBreakpoint> Breakpoints;
    int64 SelectedNeuronId{-1};
    int64 SelectedConnectionId{-1};
    int64 SelectedLayerId{-1};
    int64 FocusedLayerId{-1};
    bool bNetworkPreview{};
    TSet<int64> SelectedNeuronIds;
    FString SelectedLayerName;
    FString TopologyKey;
    FString BreakpointKey;
    FString RelationshipKey;
    TSet<int64> ExpandedExplorerLayerIds;
    FString ConsoleHistory;
    TArray<FString> ConsoleCommandHistory;
    FString ConsoleHistoryDraft;
    FString FirstConsoleSuggestion;
    FText DialogTitle;
    FText DialogContent;
    int32 ConsoleHistoryIndex{};
    int32 DetailedNeuronLimit{};
    int32 DetailedConnectionLimit{};
    int32 PendingDetailedNeuronLimit{};
    int32 PendingDetailedConnectionLimit{};
    int32 InspectorConnectionLimit{};
    int32 PendingInspectorConnectionLimit{};
    int64 RelationshipOffset{};
    double RelationshipMinimumAbsoluteWeight{};
    int64 PendingNeuronBiasId{-1};
    int64 PendingConnectionWeightId{-1};
    double PendingNeuronBias{};
    double PendingConnectionWeight{};
    EMiaIAStudioViewMode ViewMode{
        EMiaIAStudioViewMode::TwoDimensional};
    EMiaIAEditorTheme Theme{EMiaIAEditorTheme::FollowUnreal};
    EMiaIADataRefreshMode DataRefreshMode{
        EMiaIADataRefreshMode::Adaptive};
    FMiaIAVisualizationSettings VisualizationSettings;
    EMiaIAProjectPathAction ProjectPathAction{
        EMiaIAProjectPathAction::None};
    EMiaIANeuronRelationshipDirection RelationshipDirection{
        EMiaIANeuronRelationshipDirection::Incoming};
    EMiaIANeuronRelationshipSort RelationshipSort{
        EMiaIANeuronRelationshipSort::ConnectionId};
    EMiaIATrainingBreakpointKind BreakpointKind{
        EMiaIATrainingBreakpointKind::Phase};
    EMiaIATrainingDebugPhase BreakpointPhase{
        EMiaIATrainingDebugPhase::Committed};
    FButtonStyle ButtonStyle;
    FButtonStyle ExplorerButtonStyle;
    FComboButtonStyle ComboButtonStyle;
    FEditableTextBoxStyle InputStyle;
    FScrollBarStyle ScrollBarStyle;
    FSplitterStyle SplitterStyle;
    bool bHasDebugNeuron{};
    bool bHasDebugConnection{};
    bool bHasNeuronInspection{};
    bool bHasRelationshipPage{};
    bool bHasConnectionInspection{};
    bool bPendingNeuronBiasDirty{};
    bool bPendingConnectionWeightDirty{};
    bool bExplorerConnectionsExpanded{};
    bool bExplorerExpansionInitialized{};
    bool bUpdatingConsoleInput{};
    bool bCompactTopology{};
    bool bNetworkRequiresCompactTopology{};
    bool bStandaloneMode{};
    bool bTopologyWorkspaceExpanded{};
    bool bDialogVisible{};
    bool bRelationshipSortDescending{};
    double PeriodicRefreshElapsedSeconds{};

    TSharedPtr<SVerticalBox> ExplorerContent;
    TSharedPtr<SVerticalBox> RelationshipContent;
    TSharedPtr<SVerticalBox> ConsoleSuggestionsContent;
    TSharedPtr<SVerticalBox> BreakpointContent;
    TSharedPtr<SMiaIANetworkView> NetworkView;
    TSharedPtr<SMiaIA3DNetworkView> Network3DView;
    TSharedPtr<SWidgetSwitcher> TopologySwitcher;
    TSharedPtr<SWidgetSwitcher> BottomSwitcher;
    TSharedPtr<SMultiLineEditableText> ConsoleOutput;
    TSharedPtr<SScrollBar> ConsoleOutputScrollBar;
    TSharedPtr<SEditableTextBox> ConsoleInput;
    TSharedPtr<SEditableTextBox> BreakpointTargetInput;
    TSharedPtr<SEditableTextBox> BreakpointThresholdInput;
    TSharedPtr<SEditableTextBox> ProjectPathInput;
};
