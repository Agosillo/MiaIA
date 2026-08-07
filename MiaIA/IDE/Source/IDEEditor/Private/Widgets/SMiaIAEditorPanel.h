#pragma once

#include "MiaIABlueprintTypes.h"
#include "Widgets/SCompoundWidget.h"

class SMiaIANetworkView;
class SVerticalBox;
class SWidgetSwitcher;

class SMiaIAEditorPanel final : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SMiaIAEditorPanel)
    {
    }
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

private:
    void RefreshData();
    void RebuildExplorer();
    void SelectNeuron(int64 NeuronId);
    const FMiaIANeuronSnapshot* FindNeuron(int64 NeuronId) const;

    EActiveTimerReturnType HandleRefreshTimer(
        double CurrentTime,
        float DeltaTime);
    FReply HandleRefresh();
    FReply HandleResume();
    FReply HandlePause();
    FReply HandleAdvanceDebug();
    FReply SelectBottomTab(int32 TabIndex);

    bool CanResume() const;
    bool CanPause() const;
    bool CanAdvanceDebug() const;

    FText SessionStatusText() const;
    FText DebugPhaseText() const;
    FText NetworkSummaryText() const;
    FText ConsoleText() const;
    FText SelectedNeuronTitle() const;
    FText SelectedLayerText() const;
    FText SelectedActivationText() const;
    FText SelectedBiasText() const;
    FText SelectedGradientText() const;
    FSlateColor PhaseColor(EMiaIATrainingDebugPhase Phase) const;

    FMiaIANetworkSnapshot Network;
    FMiaIATrainingSessionSnapshot Session;
    FMiaIATrainingDebugSnapshot Debug;
    FMiaIATrainingDebugNeuron DebugNeuron;
    int64 SelectedNeuronId{-1};
    FString SelectedLayerName;
    FString TopologyKey;
    bool bHasDebugNeuron{};

    TSharedPtr<SVerticalBox> ExplorerContent;
    TSharedPtr<SMiaIANetworkView> NetworkView;
    TSharedPtr<SWidgetSwitcher> BottomSwitcher;
};
