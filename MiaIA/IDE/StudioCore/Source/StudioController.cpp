#include "../Include/StudioController.h"

#include "../../../CLI/Include/MiaIACommandProcessor.h"
#include "../../../SDK/Include/MiaIAClient.h"

#include <cmath>
#include <utility>

MiaIA::Studio::StudioController::StudioController(
    std::string workingDirectory)
    : WorkingDirectory(std::move(workingDirectory))
{
    Refresh();
}

void MiaIA::Studio::StudioController::SetWorkingDirectory(
    std::string workingDirectory)
{
    WorkingDirectory = std::move(workingDirectory);
}

void MiaIA::Studio::StudioController::SetViewMode(
    StudioViewMode viewMode)
{
    if (ViewMode == viewMode)
    {
        return;
    }

    ViewMode = viewMode;

    if (CurrentState.Topology.Detail == StudioTopologyDetail::Compact)
    {
        CurrentState.Topology = StudioTopologyBuilder::BuildCompact(
            CurrentState.Overview,
            ViewMode);
    }
    else
    {
        CurrentState.Topology = StudioTopologyBuilder::BuildDetailed(
            CurrentState.Network,
            ViewMode);
    }
}

MiaIA::Studio::StudioViewMode
MiaIA::Studio::StudioController::GetViewMode() const
{
    return ViewMode;
}

void MiaIA::Studio::StudioController::SetRelationshipLimit(
    std::size_t maximumPerDirection)
{
    RelationshipLimit = maximumPerDirection;
    RefreshSelectionInspection();
}

std::size_t MiaIA::Studio::StudioController::GetRelationshipLimit() const
{
    return RelationshipLimit;
}

void MiaIA::Studio::StudioController::Refresh()
{
    CurrentState.Overview = SDK::MiaIAClient::GetNetworkOverview();

    if (StudioTopologyBuilder::ChooseDetail(CurrentState.Overview) ==
        StudioTopologyDetail::Compact)
    {
        CurrentState.Network = {};
        CurrentState.Topology = StudioTopologyBuilder::BuildCompact(
            CurrentState.Overview,
            ViewMode);
    }
    else
    {
        CurrentState.Network = SDK::MiaIAClient::GetSnapshot();
        CurrentState.Topology = StudioTopologyBuilder::BuildDetailed(
            CurrentState.Network,
            ViewMode);
    }

    ValidateSelection();
    RefreshSelectionInspection();
    RefreshTrainingTimeline();
    RefreshModelCheckpoints();
}

void MiaIA::Studio::StudioController::RefreshModelCheckpoints()
{
    CurrentState.ModelCheckpoints.Checkpoints =
        SDK::MiaIAClient::GetModelCheckpoints();

    if (!CurrentState.ModelCheckpoints.HasSelectedCheckpoint)
    {
        return;
    }

    Core::ModelCheckpointSnapshot selected;
    if (SDK::MiaIAClient::TryGetModelCheckpoint(
        CurrentState.ModelCheckpoints.SelectedCheckpoint.Summary.Id,
        selected))
    {
        CurrentState.ModelCheckpoints.SelectedCheckpoint =
            std::move(selected);
    }
    else
    {
        CurrentState.ModelCheckpoints.HasSelectedCheckpoint = false;
        CurrentState.ModelCheckpoints.SelectedCheckpoint = {};
    }
}

bool MiaIA::Studio::StudioController::CaptureModelCheckpoint(
    const std::string& name)
{
    Core::ModelCheckpointSummarySnapshot captured;
    if (!SDK::MiaIAClient::CaptureModelCheckpoint(name, captured))
    {
        return false;
    }

    RefreshModelCheckpoints();
    return SelectModelCheckpoint(captured.Id);
}

bool MiaIA::Studio::StudioController::SelectModelCheckpoint(
    std::uint64_t checkpointId)
{
    Core::ModelCheckpointSnapshot selected;
    if (!SDK::MiaIAClient::TryGetModelCheckpoint(checkpointId, selected))
    {
        return false;
    }

    CurrentState.ModelCheckpoints.HasSelectedCheckpoint = true;
    CurrentState.ModelCheckpoints.SelectedCheckpoint = std::move(selected);
    return true;
}

bool MiaIA::Studio::StudioController::CompareModelCheckpoints(
    std::uint64_t firstCheckpointId,
    std::uint64_t secondCheckpointId)
{
    Core::ModelCheckpointComparisonSnapshot comparison;
    if (!SDK::MiaIAClient::TryCompareModelCheckpoints(
        firstCheckpointId,
        secondCheckpointId,
        comparison))
    {
        return false;
    }

    CurrentState.ModelCheckpoints.HasComparison = true;
    CurrentState.ModelCheckpoints.Comparison = std::move(comparison);
    return true;
}

bool MiaIA::Studio::StudioController::RestoreModelCheckpoint(
    std::uint64_t checkpointId)
{
    if (!SDK::MiaIAClient::RestoreModelCheckpoint(checkpointId))
    {
        return false;
    }

    ClearForwardTrace();
    ClearBackwardTrace();
    ClearSignalHealthDiagnostics();
    Refresh();
    SelectModelCheckpoint(checkpointId);
    return true;
}

bool MiaIA::Studio::StudioController::RemoveModelCheckpoint(
    std::uint64_t checkpointId)
{
    if (!SDK::MiaIAClient::RemoveModelCheckpoint(checkpointId))
    {
        return false;
    }

    RefreshModelCheckpoints();
    CurrentState.ModelCheckpoints.HasComparison = false;
    CurrentState.ModelCheckpoints.Comparison = {};
    return true;
}

bool MiaIA::Studio::StudioController::ClearModelCheckpoints()
{
    if (!SDK::MiaIAClient::ClearModelCheckpoints())
    {
        return false;
    }

    CurrentState.ModelCheckpoints = {};
    return true;
}

void MiaIA::Studio::StudioController::ClearModelCheckpointComparison()
{
    CurrentState.ModelCheckpoints.HasComparison = false;
    CurrentState.ModelCheckpoints.Comparison = {};
}

MiaIA::Studio::StudioCommandResult
MiaIA::Studio::StudioController::ExecuteCommand(
    const std::string& command)
{
    const CLI::CommandResult result =
        CLI::MiaIACommandProcessor::Execute(command, WorkingDirectory);
    Refresh();
    return { result.Output, result.ExitRequested };
}

std::vector<MiaIA::Studio::StudioCommandSuggestion>
MiaIA::Studio::StudioController::GetSuggestions(
    const std::string& input,
    std::size_t maximumResults) const
{
    const auto suggestions = CLI::MiaIACommandProcessor::GetSuggestions(
        input,
        maximumResults);
    std::vector<StudioCommandSuggestion> result;
    result.reserve(suggestions.size());

    for (const CLI::CommandSuggestion& suggestion : suggestions)
    {
        result.push_back({
            suggestion.Completion,
            suggestion.Syntax,
            suggestion.Description
        });
    }

    return result;
}

bool MiaIA::Studio::StudioController::SelectLayer(std::uint64_t layerId)
{
    bool exists = false;

    for (const Core::LayerOverviewSnapshot& layer :
        CurrentState.Overview.Layers)
    {
        if (layer.Id == layerId)
        {
            exists = true;
            break;
        }
    }

    if (!exists)
    {
        return false;
    }

    CurrentState.Selection = { StudioSelectionKind::Layer, layerId };
    RefreshSelectionInspection();
    return true;
}

bool MiaIA::Studio::StudioController::SelectNeuron(std::uint64_t neuronId)
{
    if (!ContainsNode(StudioNodeKind::Neuron, neuronId))
    {
        return false;
    }

    CurrentState.Selection = { StudioSelectionKind::Neuron, neuronId };
    RefreshSelectionInspection();
    if (CurrentState.ForwardTrace.Active)
    {
        FocusForwardTraceNeuron(neuronId);
    }
    if (CurrentState.BackwardTrace.Active)
    {
        FocusBackwardTraceNeuron(neuronId);
    }
    return true;
}

bool MiaIA::Studio::StudioController::SelectConnection(
    std::uint64_t connectionId)
{
    if (!ContainsConnection(connectionId))
    {
        return false;
    }

    CurrentState.Selection = {
        StudioSelectionKind::Connection,
        connectionId
    };
    RefreshSelectionInspection();
    return true;
}

void MiaIA::Studio::StudioController::ClearSelection()
{
    CurrentState.Selection = {};
    CurrentState.HasNeuronInspection = false;
    CurrentState.NeuronInspection = {};
    CurrentState.HasConnectionInspection = false;
    CurrentState.ConnectionInspection = {};
    CurrentState.ForwardTrace.FocusedNeuronId = 0;
    CurrentState.ForwardTrace.HasContributionPage = false;
    CurrentState.ForwardTrace.ContributionPage = {};
    CurrentState.BackwardTrace.FocusedNeuronId = 0;
}

bool MiaIA::Studio::StudioController::RunForwardTrace(
    const std::vector<double>& inputs)
{
    Core::ForwardTraceSnapshot trace;

    if (!SDK::MiaIAClient::TraceForward(inputs, trace))
    {
        return false;
    }

    StudioForwardTraceState next;
    next.Active = true;
    next.Trace = std::move(trace);
    next.ContributionRequest =
        CurrentState.ForwardTrace.ContributionRequest;
    next.PlaybackFrameDurationSeconds =
        ForwardTraceFrameDurationSeconds;

    if (CurrentState.Selection.Kind == StudioSelectionKind::Neuron)
    {
        next.FocusedNeuronId = CurrentState.Selection.Id;
    }

    ClearBackwardTrace();
    ClearSignalHealthDiagnostics();
    CurrentState.ForwardTrace = std::move(next);
    BuildForwardTracePlaybackFrames();

    if (CurrentState.ForwardTrace.FocusedNeuronId != 0)
    {
        RefreshForwardTraceContributions();
    }

    return true;
}

void MiaIA::Studio::StudioController::ClearForwardTrace()
{
    CurrentState.ForwardTrace = {};
    CurrentState.ForwardTrace.PlaybackFrameDurationSeconds =
        ForwardTraceFrameDurationSeconds;
}

bool MiaIA::Studio::StudioController::FocusForwardTraceNeuron(
    std::uint64_t neuronId)
{
    if (!CurrentState.ForwardTrace.Active ||
        !ContainsForwardTraceNeuron(neuronId))
    {
        return false;
    }

    const StudioForwardTraceState previous = CurrentState.ForwardTrace;
    CurrentState.ForwardTrace.FocusedNeuronId = neuronId;
    CurrentState.ForwardTrace.ContributionRequest.Offset = 0;

    if (!RefreshForwardTraceContributions())
    {
        CurrentState.ForwardTrace = previous;
        return false;
    }

    return true;
}

bool MiaIA::Studio::StudioController::SetForwardTraceContributionRequest(
    const Core::ForwardTraceContributionPageRequest& request)
{
    if (!CurrentState.ForwardTrace.Active ||
        CurrentState.ForwardTrace.FocusedNeuronId == 0)
    {
        return false;
    }

    const StudioForwardTraceState previous = CurrentState.ForwardTrace;
    CurrentState.ForwardTrace.ContributionRequest = request;

    if (!RefreshForwardTraceContributions())
    {
        CurrentState.ForwardTrace = previous;
        return false;
    }

    return true;
}

bool MiaIA::Studio::StudioController::PlayForwardTrace()
{
    StudioForwardTraceState& state = CurrentState.ForwardTrace;

    if (!state.Active || state.PlaybackFrames.empty() ||
        state.PlaybackStatus == StudioForwardTracePlaybackStatus::Playing)
    {
        return false;
    }

    if (state.PlaybackStatus ==
        StudioForwardTracePlaybackStatus::Completed)
    {
        state.PlaybackFrameIndex = 0;
        state.PlaybackFrameElapsedSeconds = 0.0;
    }

    state.PlaybackStatus = StudioForwardTracePlaybackStatus::Playing;
    return true;
}

bool MiaIA::Studio::StudioController::PauseForwardTrace()
{
    StudioForwardTraceState& state = CurrentState.ForwardTrace;

    if (!state.Active || state.PlaybackStatus !=
        StudioForwardTracePlaybackStatus::Playing)
    {
        return false;
    }

    state.PlaybackStatus = StudioForwardTracePlaybackStatus::Paused;
    return true;
}

bool MiaIA::Studio::StudioController::RestartForwardTrace()
{
    StudioForwardTraceState& state = CurrentState.ForwardTrace;

    if (!state.Active || state.PlaybackFrames.empty())
    {
        return false;
    }

    state.PlaybackFrameIndex = 0;
    state.PlaybackFrameElapsedSeconds = 0.0;
    state.PlaybackStatus = StudioForwardTracePlaybackStatus::Paused;
    return true;
}

bool MiaIA::Studio::StudioController::StepForwardTraceForward()
{
    StudioForwardTraceState& state = CurrentState.ForwardTrace;

    if (!state.Active || state.PlaybackFrames.empty())
    {
        return false;
    }

    state.PlaybackFrameElapsedSeconds = 0.0;

    if (state.PlaybackStatus ==
        StudioForwardTracePlaybackStatus::Completed)
    {
        return false;
    }

    state.PlaybackStatus = StudioForwardTracePlaybackStatus::Paused;

    if (state.PlaybackFrameIndex + 1 < state.PlaybackFrames.size())
    {
        ++state.PlaybackFrameIndex;
        return true;
    }

    state.PlaybackStatus = StudioForwardTracePlaybackStatus::Completed;
    return true;
}

bool MiaIA::Studio::StudioController::StepForwardTraceBackward()
{
    StudioForwardTraceState& state = CurrentState.ForwardTrace;

    if (!state.Active || state.PlaybackFrames.empty())
    {
        return false;
    }

    state.PlaybackFrameElapsedSeconds = 0.0;

    if (state.PlaybackStatus ==
        StudioForwardTracePlaybackStatus::Completed)
    {
        state.PlaybackStatus = StudioForwardTracePlaybackStatus::Paused;
        return true;
    }

    state.PlaybackStatus = StudioForwardTracePlaybackStatus::Paused;

    if (state.PlaybackFrameIndex == 0)
    {
        return false;
    }

    --state.PlaybackFrameIndex;
    return true;
}

bool MiaIA::Studio::StudioController::AdvanceForwardTracePlayback(
    double elapsedSeconds)
{
    StudioForwardTraceState& state = CurrentState.ForwardTrace;

    if (!state.Active || state.PlaybackFrames.empty() ||
        state.PlaybackStatus != StudioForwardTracePlaybackStatus::Playing ||
        !std::isfinite(elapsedSeconds) || elapsedSeconds < 0.0)
    {
        return false;
    }

    state.PlaybackFrameElapsedSeconds += elapsedSeconds;
    bool changed = false;

    while (state.PlaybackFrameElapsedSeconds >=
        state.PlaybackFrameDurationSeconds)
    {
        state.PlaybackFrameElapsedSeconds -=
            state.PlaybackFrameDurationSeconds;

        if (state.PlaybackFrameIndex + 1 <
            state.PlaybackFrames.size())
        {
            ++state.PlaybackFrameIndex;
            changed = true;
            continue;
        }

        state.PlaybackFrameElapsedSeconds = 0.0;
        state.PlaybackStatus =
            StudioForwardTracePlaybackStatus::Completed;
        changed = true;
        break;
    }

    return changed;
}

bool MiaIA::Studio::StudioController::SetForwardTraceFrameDuration(
    double durationSeconds)
{
    if (!std::isfinite(durationSeconds) || durationSeconds <= 0.0)
    {
        return false;
    }

    ForwardTraceFrameDurationSeconds = durationSeconds;
    CurrentState.ForwardTrace.PlaybackFrameDurationSeconds =
        durationSeconds;
    CurrentState.ForwardTrace.PlaybackFrameElapsedSeconds = 0.0;
    return true;
}

bool MiaIA::Studio::StudioController::RunBackwardTrace(
    const std::vector<double>& inputs,
    const std::vector<double>& targets)
{
    Core::BackwardTraceSnapshot trace;

    if (!SDK::MiaIAClient::TraceBackward(
        inputs,
        targets,
        Core::LossType::MeanSquaredError,
        trace))
    {
        return false;
    }

    StudioBackwardTraceState next;
    next.Active = true;
    next.Trace = std::move(trace);
    next.PlaybackFrameDurationSeconds =
        BackwardTraceFrameDurationSeconds;

    if (CurrentState.Selection.Kind == StudioSelectionKind::Neuron)
    {
        next.FocusedNeuronId = CurrentState.Selection.Id;
    }

    ClearForwardTrace();
    ClearSignalHealthDiagnostics();
    CurrentState.BackwardTrace = std::move(next);
    BuildBackwardTracePlaybackFrames();
    return true;
}

void MiaIA::Studio::StudioController::ClearBackwardTrace()
{
    CurrentState.BackwardTrace = {};
    CurrentState.BackwardTrace.PlaybackFrameDurationSeconds =
        BackwardTraceFrameDurationSeconds;
}

bool MiaIA::Studio::StudioController::FocusBackwardTraceNeuron(
    std::uint64_t neuronId)
{
    if (!CurrentState.BackwardTrace.Active ||
        !ContainsBackwardTraceNeuron(neuronId))
    {
        return false;
    }

    CurrentState.BackwardTrace.FocusedNeuronId = neuronId;
    return true;
}

bool MiaIA::Studio::StudioController::PlayBackwardTrace()
{
    StudioBackwardTraceState& state = CurrentState.BackwardTrace;

    if (!state.Active || state.PlaybackFrames.empty() ||
        state.PlaybackStatus == StudioForwardTracePlaybackStatus::Playing)
    {
        return false;
    }

    if (state.PlaybackStatus == StudioForwardTracePlaybackStatus::Completed)
    {
        state.PlaybackFrameIndex = 0;
        state.PlaybackFrameElapsedSeconds = 0.0;
    }

    state.PlaybackStatus = StudioForwardTracePlaybackStatus::Playing;
    return true;
}

bool MiaIA::Studio::StudioController::PauseBackwardTrace()
{
    StudioBackwardTraceState& state = CurrentState.BackwardTrace;

    if (!state.Active || state.PlaybackStatus !=
        StudioForwardTracePlaybackStatus::Playing)
    {
        return false;
    }

    state.PlaybackStatus = StudioForwardTracePlaybackStatus::Paused;
    return true;
}

bool MiaIA::Studio::StudioController::RestartBackwardTrace()
{
    StudioBackwardTraceState& state = CurrentState.BackwardTrace;

    if (!state.Active || state.PlaybackFrames.empty())
    {
        return false;
    }

    state.PlaybackFrameIndex = 0;
    state.PlaybackFrameElapsedSeconds = 0.0;
    state.PlaybackStatus = StudioForwardTracePlaybackStatus::Paused;
    return true;
}

bool MiaIA::Studio::StudioController::StepBackwardTraceForward()
{
    StudioBackwardTraceState& state = CurrentState.BackwardTrace;

    if (!state.Active || state.PlaybackFrames.empty())
    {
        return false;
    }

    state.PlaybackFrameElapsedSeconds = 0.0;

    if (state.PlaybackStatus == StudioForwardTracePlaybackStatus::Completed)
    {
        return false;
    }

    state.PlaybackStatus = StudioForwardTracePlaybackStatus::Paused;

    if (state.PlaybackFrameIndex + 1 < state.PlaybackFrames.size())
    {
        ++state.PlaybackFrameIndex;
        return true;
    }

    state.PlaybackStatus = StudioForwardTracePlaybackStatus::Completed;
    return true;
}

bool MiaIA::Studio::StudioController::StepBackwardTraceBackward()
{
    StudioBackwardTraceState& state = CurrentState.BackwardTrace;

    if (!state.Active || state.PlaybackFrames.empty())
    {
        return false;
    }

    state.PlaybackFrameElapsedSeconds = 0.0;

    if (state.PlaybackStatus == StudioForwardTracePlaybackStatus::Completed)
    {
        state.PlaybackStatus = StudioForwardTracePlaybackStatus::Paused;
        return true;
    }

    state.PlaybackStatus = StudioForwardTracePlaybackStatus::Paused;

    if (state.PlaybackFrameIndex == 0)
    {
        return false;
    }

    --state.PlaybackFrameIndex;
    return true;
}

bool MiaIA::Studio::StudioController::AdvanceBackwardTracePlayback(
    double elapsedSeconds)
{
    StudioBackwardTraceState& state = CurrentState.BackwardTrace;

    if (!state.Active || state.PlaybackFrames.empty() ||
        state.PlaybackStatus != StudioForwardTracePlaybackStatus::Playing ||
        !std::isfinite(elapsedSeconds) || elapsedSeconds < 0.0)
    {
        return false;
    }

    state.PlaybackFrameElapsedSeconds += elapsedSeconds;
    bool changed = false;

    while (state.PlaybackFrameElapsedSeconds >=
        state.PlaybackFrameDurationSeconds)
    {
        state.PlaybackFrameElapsedSeconds -=
            state.PlaybackFrameDurationSeconds;

        if (state.PlaybackFrameIndex + 1 < state.PlaybackFrames.size())
        {
            ++state.PlaybackFrameIndex;
            changed = true;
            continue;
        }

        state.PlaybackFrameElapsedSeconds = 0.0;
        state.PlaybackStatus = StudioForwardTracePlaybackStatus::Completed;
        changed = true;
        break;
    }

    return changed;
}

bool MiaIA::Studio::StudioController::SetBackwardTraceFrameDuration(
    double durationSeconds)
{
    if (!std::isfinite(durationSeconds) || durationSeconds <= 0.0)
    {
        return false;
    }

    BackwardTraceFrameDurationSeconds = durationSeconds;
    CurrentState.BackwardTrace.PlaybackFrameDurationSeconds =
        durationSeconds;
    CurrentState.BackwardTrace.PlaybackFrameElapsedSeconds = 0.0;
    return true;
}

bool MiaIA::Studio::StudioController::RunSignalHealthDiagnostics(
    const Core::SignalHealthConfiguration& configuration)
{
    Core::SignalHealthSnapshot snapshot;

    if (!SDK::MiaIAClient::DiagnoseDataset(
        Core::LossType::MeanSquaredError,
        configuration,
        snapshot))
    {
        return false;
    }

    const StudioSignalHealthFilter filter =
        CurrentState.SignalHealth.Filter;
    CurrentState.SignalHealth = {};
    CurrentState.SignalHealth.Active = true;
    CurrentState.SignalHealth.Filter = filter;
    CurrentState.SignalHealth.Snapshot = std::move(snapshot);
    ClearForwardTrace();
    ClearBackwardTrace();
    return true;
}

void MiaIA::Studio::StudioController::ClearSignalHealthDiagnostics()
{
    const StudioSignalHealthFilter filter =
        CurrentState.SignalHealth.Filter;
    CurrentState.SignalHealth = {};
    CurrentState.SignalHealth.Filter = filter;
}

void MiaIA::Studio::StudioController::SetSignalHealthFilter(
    StudioSignalHealthFilter filter)
{
    CurrentState.SignalHealth.Filter = filter;
}

void MiaIA::Studio::StudioController::RefreshTrainingTimeline()
{
    StudioTrainingTimelineState& state = CurrentState.TrainingTimeline;
    const bool hadSelectedStep = state.HasSelectedStep;
    const std::size_t selectedStepIndex = state.SelectedStepIndex;
    Core::TrainingHistoryEntrySnapshot selectedEntry;
    const bool hadSelectedEntry = hadSelectedStep &&
        selectedStepIndex < state.History.size();

    if (hadSelectedEntry)
    {
        selectedEntry = state.History[selectedStepIndex];
    }

    state.Session = SDK::MiaIAClient::GetTrainingSession();
    state.Session.Steps = {};
    state.Debug = SDK::MiaIAClient::GetTrainingDebug();
    state.History = SDK::MiaIAClient::GetTrainingSessionHistory();
    state.HasSelectedStep = false;
    state.SelectedStepIndex = 0;
    state.SelectedStep = {};

    if (hadSelectedEntry && selectedStepIndex < state.History.size())
    {
        const Core::TrainingHistoryEntrySnapshot& refreshedEntry =
            state.History[selectedStepIndex];
        const bool sameEntry =
            refreshedEntry.StepIndex == selectedEntry.StepIndex &&
            refreshedEntry.EpochIndex == selectedEntry.EpochIndex &&
            refreshedEntry.SampleIndex == selectedEntry.SampleIndex &&
            refreshedEntry.LossBefore == selectedEntry.LossBefore &&
            refreshedEntry.LossAfter == selectedEntry.LossAfter &&
            refreshedEntry.WeightUpdateCount ==
                selectedEntry.WeightUpdateCount &&
            refreshedEntry.BiasUpdateCount == selectedEntry.BiasUpdateCount;

        if (sameEntry)
        {
            SelectTrainingTimelineStep(selectedStepIndex);
        }
    }
}

bool MiaIA::Studio::StudioController::SelectTrainingTimelineStep(
    std::size_t stepIndex)
{
    StudioTrainingTimelineState& state = CurrentState.TrainingTimeline;

    if (stepIndex >= state.History.size() ||
        state.History[stepIndex].StepIndex != stepIndex)
    {
        return false;
    }

    Core::TrainingStepSnapshot step;

    if (!SDK::MiaIAClient::TryGetTrainingSessionStep(stepIndex, step))
    {
        return false;
    }

    state.HasSelectedStep = true;
    state.SelectedStepIndex = stepIndex;
    state.SelectedStep = std::move(step);
    return true;
}

void MiaIA::Studio::StudioController::ClearTrainingTimelineSelection()
{
    CurrentState.TrainingTimeline.HasSelectedStep = false;
    CurrentState.TrainingTimeline.SelectedStepIndex = 0;
    CurrentState.TrainingTimeline.SelectedStep = {};
}

const MiaIA::Studio::StudioState&
MiaIA::Studio::StudioController::State() const
{
    return CurrentState;
}

bool MiaIA::Studio::StudioController::ContainsNode(
    StudioNodeKind kind,
    std::uint64_t id) const
{
    for (const StudioTopologyNode& node : CurrentState.Topology.Nodes)
    {
        if (node.Kind == kind && node.Id == id)
        {
            return true;
        }
    }

    return false;
}

bool MiaIA::Studio::StudioController::ContainsConnection(
    std::uint64_t id) const
{
    for (const StudioTopologyLink& link : CurrentState.Topology.Links)
    {
        if (!link.Aggregate && link.Id == id)
        {
            return true;
        }
    }

    return false;
}

void MiaIA::Studio::StudioController::ValidateSelection()
{
    bool valid = false;

    switch (CurrentState.Selection.Kind)
    {
    case StudioSelectionKind::None:
        return;
    case StudioSelectionKind::Layer:
        for (const Core::LayerOverviewSnapshot& layer :
            CurrentState.Overview.Layers)
        {
            if (layer.Id == CurrentState.Selection.Id)
            {
                valid = true;
                break;
            }
        }
        break;
    case StudioSelectionKind::Neuron:
        valid = ContainsNode(
            StudioNodeKind::Neuron,
            CurrentState.Selection.Id);
        break;
    case StudioSelectionKind::Connection:
        valid = ContainsConnection(CurrentState.Selection.Id);
        break;
    }

    if (!valid)
    {
        ClearSelection();
    }
}

void MiaIA::Studio::StudioController::RefreshSelectionInspection()
{
    CurrentState.HasNeuronInspection = false;
    CurrentState.NeuronInspection = {};
    CurrentState.HasConnectionInspection = false;
    CurrentState.ConnectionInspection = {};

    if (CurrentState.Selection.Kind == StudioSelectionKind::Neuron)
    {
        CurrentState.HasNeuronInspection =
            SDK::MiaIAClient::TryInspectNeuron(
                CurrentState.Selection.Id,
                RelationshipLimit,
                CurrentState.NeuronInspection);
    }
    else if (CurrentState.Selection.Kind ==
        StudioSelectionKind::Connection)
    {
        CurrentState.HasConnectionInspection =
            SDK::MiaIAClient::TryInspectConnection(
                CurrentState.Selection.Id,
                CurrentState.ConnectionInspection);
    }
}

bool MiaIA::Studio::StudioController::ContainsForwardTraceNeuron(
    std::uint64_t neuronId) const
{
    for (const Core::ForwardTraceLayerSnapshot& layer :
        CurrentState.ForwardTrace.Trace.Layers)
    {
        for (const Core::ForwardTraceNeuronSnapshot& neuron : layer.Neurons)
        {
            if (neuron.Id == neuronId)
            {
                return true;
            }
        }
    }

    return false;
}

bool MiaIA::Studio::StudioController::ContainsBackwardTraceNeuron(
    std::uint64_t neuronId) const
{
    for (const Core::BackwardTraceLayerSnapshot& layer :
        CurrentState.BackwardTrace.Trace.Layers)
    {
        for (const Core::BackwardTraceNeuronSnapshot& neuron : layer.Neurons)
        {
            if (neuron.Id == neuronId)
            {
                return true;
            }
        }
    }

    return false;
}

void MiaIA::Studio::StudioController::BuildForwardTracePlaybackFrames()
{
    StudioForwardTraceState& state = CurrentState.ForwardTrace;
    state.PlaybackFrames.clear();
    state.PlaybackFrameIndex = 0;
    state.PlaybackFrameElapsedSeconds = 0.0;
    state.PlaybackStatus = StudioForwardTracePlaybackStatus::Paused;

    for (std::size_t layerIndex = 0;
        layerIndex < state.Trace.Layers.size();
        ++layerIndex)
    {
        const Core::ForwardTraceLayerSnapshot& layer =
            state.Trace.Layers[layerIndex];

        if (layerIndex == 0)
        {
            state.PlaybackFrames.push_back({
                StudioForwardTraceFrameKind::InputActivations,
                layerIndex,
                layer.Id
            });
            continue;
        }

        state.PlaybackFrames.push_back({
            StudioForwardTraceFrameKind::IncomingSignal,
            layerIndex,
            layer.Id
        });
        state.PlaybackFrames.push_back({
            StudioForwardTraceFrameKind::LayerActivations,
            layerIndex,
            layer.Id
        });
    }
}

void MiaIA::Studio::StudioController::BuildBackwardTracePlaybackFrames()
{
    StudioBackwardTraceState& state = CurrentState.BackwardTrace;
    state.PlaybackFrames.clear();
    state.PlaybackFrameIndex = 0;
    state.PlaybackFrameElapsedSeconds = 0.0;
    state.PlaybackStatus = StudioForwardTracePlaybackStatus::Paused;

    if (state.Trace.Layers.empty())
    {
        return;
    }

    const std::size_t outputIndex = state.Trace.Layers.size() - 1;
    state.PlaybackFrames.push_back({
        StudioBackwardTraceFrameKind::OutputGradients,
        outputIndex,
        state.Trace.Layers[outputIndex].Id
    });

    for (std::size_t targetIndex = outputIndex;
        targetIndex > 0;
        --targetIndex)
    {
        state.PlaybackFrames.push_back({
            StudioBackwardTraceFrameKind::ConnectionFlow,
            targetIndex,
            state.Trace.Layers[targetIndex].Id
        });
        const std::size_t sourceIndex = targetIndex - 1;
        state.PlaybackFrames.push_back({
            StudioBackwardTraceFrameKind::LayerGradients,
            sourceIndex,
            state.Trace.Layers[sourceIndex].Id
        });
    }
}

bool MiaIA::Studio::StudioController::RefreshForwardTraceContributions()
{
    Core::ForwardTraceContributionPageSnapshot page;
    const bool succeeded =
        SDK::MiaIAClient::TryGetForwardTraceContributions(
            CurrentState.ForwardTrace.Trace.Inputs,
            CurrentState.ForwardTrace.FocusedNeuronId,
            CurrentState.ForwardTrace.ContributionRequest,
            page);

    if (!succeeded)
    {
        CurrentState.ForwardTrace.HasContributionPage = false;
        CurrentState.ForwardTrace.ContributionPage = {};
        return false;
    }

    CurrentState.ForwardTrace.HasContributionPage = true;
    CurrentState.ForwardTrace.ContributionPage = std::move(page);
    return true;
}
