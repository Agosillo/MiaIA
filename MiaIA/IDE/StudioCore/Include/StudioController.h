#pragma once

#include "StudioTopology.h"
#include "../../../Core/Public/ConnectionInspectionSnapshot.h"
#include "../../../Core/Public/ForwardTraceSnapshot.h"
#include "../../../Core/Public/NeuronInspectionSnapshot.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace MiaIA::Studio
{
    enum class StudioSelectionKind
    {
        None,
        Layer,
        Neuron,
        Connection
    };

    struct StudioSelection
    {
        StudioSelectionKind Kind{ StudioSelectionKind::None };
        std::uint64_t Id{};
    };

    struct StudioCommandResult
    {
        std::string Output;
        bool ExitRequested{};
    };

    struct StudioCommandSuggestion
    {
        std::string Completion;
        std::string Syntax;
        std::string Description;
    };

    struct StudioForwardTraceState
    {
        bool Active{};
        Core::ForwardTraceSnapshot Trace;
        std::uint64_t FocusedNeuronId{};
        Core::ForwardTraceContributionPageRequest ContributionRequest;
        bool HasContributionPage{};
        Core::ForwardTraceContributionPageSnapshot ContributionPage;
    };

    struct StudioState
    {
        Core::NetworkOverviewSnapshot Overview;
        Core::NetworkSnapshot Network;
        StudioTopologyScene Topology;
        StudioSelection Selection;
        bool HasNeuronInspection{};
        Core::NeuronInspectionSnapshot NeuronInspection;
        bool HasConnectionInspection{};
        Core::ConnectionInspectionSnapshot ConnectionInspection;
        StudioForwardTraceState ForwardTrace;
    };

    class StudioController
    {
    public:
        explicit StudioController(
            std::string workingDirectory = {});

        void SetWorkingDirectory(std::string workingDirectory);
        void SetViewMode(StudioViewMode viewMode);
        [[nodiscard]] StudioViewMode GetViewMode() const;
        void SetRelationshipLimit(std::size_t maximumPerDirection);
        [[nodiscard]] std::size_t GetRelationshipLimit() const;

        void Refresh();
        [[nodiscard]] StudioCommandResult ExecuteCommand(
            const std::string& command);
        [[nodiscard]] std::vector<StudioCommandSuggestion>
            GetSuggestions(
                const std::string& input,
                std::size_t maximumResults = 16) const;

        bool SelectLayer(std::uint64_t layerId);
        bool SelectNeuron(std::uint64_t neuronId);
        bool SelectConnection(std::uint64_t connectionId);
        void ClearSelection();

        bool RunForwardTrace(const std::vector<double>& inputs);
        void ClearForwardTrace();
        bool FocusForwardTraceNeuron(std::uint64_t neuronId);
        bool SetForwardTraceContributionRequest(
            const Core::ForwardTraceContributionPageRequest& request);

        [[nodiscard]] const StudioState& State() const;

    private:
        bool ContainsNode(
            StudioNodeKind kind,
            std::uint64_t id) const;
        bool ContainsConnection(std::uint64_t id) const;
        void ValidateSelection();
        void RefreshSelectionInspection();
        bool ContainsForwardTraceNeuron(std::uint64_t neuronId) const;
        bool RefreshForwardTraceContributions();

        std::string WorkingDirectory;
        StudioViewMode ViewMode{ StudioViewMode::TwoDimensional };
        std::size_t RelationshipLimit{ 10 };
        StudioState CurrentState;
    };
}
