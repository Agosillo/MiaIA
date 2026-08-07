#include "Widgets/SMiaIAEditorPanel.h"

#include "MiaIABlueprintLibrary.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/SMultiLineEditableText.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
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

        return key;
    }
}

void SMiaIAEditorPanel::Construct(const FArguments& InArgs)
{
    const auto panelBorder = FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder"));

    ChildSlot
    [
        SNew(SVerticalBox)
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SBorder)
            .BorderImage(panelBorder)
            .Padding(FMargin(6.0f, 4.0f))
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(2.0f)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("Refresh", "Refresh"))
                    .OnClicked(this, &SMiaIAEditorPanel::HandleRefresh)
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(2.0f)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("Resume", "Continue"))
                    .IsEnabled(this, &SMiaIAEditorPanel::CanResume)
                    .OnClicked(this, &SMiaIAEditorPanel::HandleResume)
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(2.0f)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("Pause", "Pause"))
                    .IsEnabled(this, &SMiaIAEditorPanel::CanPause)
                    .OnClicked(this, &SMiaIAEditorPanel::HandlePause)
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(2.0f)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("Advance", "Step phase"))
                    .IsEnabled(this, &SMiaIAEditorPanel::CanAdvanceDebug)
                    .OnClicked(this, &SMiaIAEditorPanel::HandleAdvanceDebug)
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
                    .ColorAndOpacity(FLinearColor(0.35f, 0.68f, 1.0f))
                ]
            ]
        ]
        + SVerticalBox::Slot()
        .FillHeight(1.0f)
        [
            SNew(SSplitter)
            .Orientation(Orient_Vertical)
            + SSplitter::Slot()
            .Value(0.72f)
            [
                SNew(SSplitter)
                .Orientation(Orient_Horizontal)
                + SSplitter::Slot()
                .Value(0.20f)
                [
                    SNew(SBorder)
                    .BorderImage(panelBorder)
                    .Padding(6.0f)
                    [
                        SNew(SScrollBox)
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
                        ]
                    ]
                ]
                + SSplitter::Slot()
                .Value(0.22f)
                [
                    SNew(SBorder)
                    .BorderImage(panelBorder)
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
                            .Text(this, &SMiaIAEditorPanel::SelectedNeuronTitle)
                        ]
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(0.0f, 3.0f)
                        [
                            SNew(STextBlock)
                            .Text(this, &SMiaIAEditorPanel::SelectedLayerText)
                        ]
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(0.0f, 3.0f)
                        [
                            SNew(STextBlock)
                            .Text(this, &SMiaIAEditorPanel::SelectedActivationText)
                        ]
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(0.0f, 3.0f)
                        [
                            SNew(STextBlock)
                            .Text(this, &SMiaIAEditorPanel::SelectedBiasText)
                        ]
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(0.0f, 3.0f)
                        [
                            SNew(STextBlock)
                            .Text(this, &SMiaIAEditorPanel::SelectedGradientText)
                        ]
                    ]
                ]
            ]
            + SSplitter::Slot()
            .Value(0.28f)
            [
                SNew(SBorder)
                .BorderImage(panelBorder)
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
                            SNew(SMultiLineEditableText)
                            .IsReadOnly(true)
                            .Text(this, &SMiaIAEditorPanel::ConsoleText)
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
    ];

    RefreshData();
    RegisterActiveTimer(
        0.25f,
        FWidgetActiveTimerDelegate::CreateSP(
            this,
            &SMiaIAEditorPanel::HandleRefreshTimer));
}

void SMiaIAEditorPanel::RefreshData()
{
    Network = UMiaIABlueprintLibrary::GetNetworkSnapshot();
    Session = UMiaIABlueprintLibrary::GetTrainingSession();
    Debug = UMiaIABlueprintLibrary::GetDebugStatus();

    if (Debug.Phase != EMiaIATrainingDebugPhase::Idle)
    {
        FMiaIANetworkSnapshot candidate =
            UMiaIABlueprintLibrary::GetDebugNetworkSnapshot();

        if (!candidate.Layers.IsEmpty())
        {
            Network = MoveTemp(candidate);
        }
    }

    const FString newTopologyKey = BuildTopologyKey(Network);
    const bool topologyChanged = newTopologyKey != TopologyKey;
    TopologyKey = newTopologyKey;

    if (!FindNeuron(SelectedNeuronId))
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
        NetworkView->SetSelectedNeuron(SelectedNeuronId);
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
                .ButtonStyle(FAppStyle::Get(), TEXT("SimpleButton"))
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
}

void SMiaIAEditorPanel::SelectNeuron(int64 NeuronId)
{
    SelectedNeuronId = NeuronId;
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

FReply SMiaIAEditorPanel::HandleAdvanceDebug()
{
    FMiaIATrainingDebugSnapshot result;
    UMiaIABlueprintLibrary::AdvanceDebugPhase(result);
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

bool SMiaIAEditorPanel::CanResume() const
{
    return Session.Status == EMiaIATrainingSessionStatus::Active &&
        Debug.Phase == EMiaIATrainingDebugPhase::Idle;
}

bool SMiaIAEditorPanel::CanPause() const
{
    return Session.Status == EMiaIATrainingSessionStatus::Running;
}

bool SMiaIAEditorPanel::CanAdvanceDebug() const
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
    int32 neuronCount = 0;

    for (const FMiaIALayerSnapshot& layer : Network.Layers)
    {
        neuronCount += layer.Neurons.Num();
    }

    return FText::Format(
        LOCTEXT(
            "NetworkSummary",
            "Network topology  ·  {0} layers  ·  {1} neurons  ·  {2} connections"),
        FText::AsNumber(Network.Layers.Num()),
        FText::AsNumber(neuronCount),
        FText::AsNumber(Network.Connections.Num()));
}

FText SMiaIAEditorPanel::ConsoleText() const
{
    return FText::Format(
        LOCTEXT(
            "ConsoleStatus",
            "> session status\nStatus: {0}\nEpoch: {1}/{2}\n\n> train debug status\nPhase: {3}\nSample: {4}"),
        SessionStatusName(Session.Status),
        FText::AsNumber(Session.CurrentEpoch),
        FText::AsNumber(Session.EpochCount),
        DebugPhaseName(Debug.Phase),
        FText::AsNumber(Debug.SampleIndex));
}

FText SMiaIAEditorPanel::SelectedNeuronTitle() const
{
    return SelectedNeuronId < 0
        ? LOCTEXT("NoNeuron", "No neuron selected")
        : FText::Format(
            LOCTEXT("SelectedNeuron", "Neuron #{0}"),
            FText::AsNumber(SelectedNeuronId));
}

FText SMiaIAEditorPanel::SelectedLayerText() const
{
    return FText::Format(
        LOCTEXT("SelectedLayer", "Layer: {0}"),
        FText::FromString(SelectedLayerName));
}

FText SMiaIAEditorPanel::SelectedActivationText() const
{
    const FMiaIANeuronSnapshot* neuron = FindNeuron(SelectedNeuronId);
    const double activation = bHasDebugNeuron
        ? DebugNeuron.CandidateActivation
        : neuron ? neuron->Activation : 0.0;
    return FText::Format(
        LOCTEXT("SelectedActivation", "Activation: {0}"),
        FText::AsNumber(activation));
}

FText SMiaIAEditorPanel::SelectedBiasText() const
{
    const FMiaIANeuronSnapshot* neuron = FindNeuron(SelectedNeuronId);
    const double bias = bHasDebugNeuron
        ? DebugNeuron.CandidateBias
        : neuron ? neuron->Bias : 0.0;
    return FText::Format(
        LOCTEXT("SelectedBias", "Bias: {0}"),
        FText::AsNumber(bias));
}

FText SMiaIAEditorPanel::SelectedGradientText() const
{
    return bHasDebugNeuron && DebugNeuron.bHasGradients
        ? FText::Format(
            LOCTEXT("SelectedGradient", "Bias gradient: {0}"),
            FText::AsNumber(DebugNeuron.BiasGradient))
        : LOCTEXT("GradientUnavailable", "Bias gradient: unavailable");
}

FSlateColor SMiaIAEditorPanel::PhaseColor(
    EMiaIATrainingDebugPhase Phase) const
{
    return static_cast<uint8>(Debug.Phase) >= static_cast<uint8>(Phase)
        ? FSlateColor(FLinearColor(0.30f, 0.68f, 1.0f))
        : FSlateColor::UseSubduedForeground();
}

#undef LOCTEXT_NAMESPACE
