#include "../Include/StudioController.h"

#include "../../../CLI/Include/MiaIACommandProcessor.h"
#include "../../../SDK/Include/MiaIAClient.h"

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
