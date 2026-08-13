#pragma once

#include "StudioTopology.h"
#include "../../../Core/Public/BackwardTraceSnapshot.h"
#include "../../../Core/Public/ConnectionInspectionSnapshot.h"
#include "../../../Core/Public/ForwardTraceSnapshot.h"
#include "../../../Core/Public/NeuronInspectionSnapshot.h"
#include "../../../Core/Public/SignalHealthSnapshot.h"
#include "../../../Core/Public/ModelCheckpointSnapshot.h"
#include "../../../Core/Public/TrainingDebugSnapshot.h"
#include "../../../Core/Public/TrainingHistoryEntrySnapshot.h"
#include "../../../Core/Public/TrainingSessionSnapshot.h"
#include "../../../Core/Public/TrainingStepSnapshot.h"

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

    enum class StudioForwardTraceFrameKind
    {
        InputActivations,
        IncomingSignal,
        LayerActivations
    };

    enum class StudioForwardTracePlaybackStatus
    {
        Paused,
        Playing,
        Completed
    };

    struct StudioForwardTraceFrame
    {
        StudioForwardTraceFrameKind Kind{
            StudioForwardTraceFrameKind::InputActivations };
        std::size_t LayerIndex{};
        std::uint64_t LayerId{};
    };

    struct StudioForwardTraceState
    {
        bool Active{};
        Core::ForwardTraceSnapshot Trace;
        std::uint64_t FocusedNeuronId{};
        Core::ForwardTraceContributionPageRequest ContributionRequest;
        bool HasContributionPage{};
        Core::ForwardTraceContributionPageSnapshot ContributionPage;
        std::vector<StudioForwardTraceFrame> PlaybackFrames;
        std::size_t PlaybackFrameIndex{};
        StudioForwardTracePlaybackStatus PlaybackStatus{
            StudioForwardTracePlaybackStatus::Paused };
        double PlaybackFrameDurationSeconds{ 0.65 };
        double PlaybackFrameElapsedSeconds{};
    };

    enum class StudioBackwardTraceFrameKind
    {
        OutputGradients,
        ConnectionFlow,
        LayerGradients
    };

    struct StudioBackwardTraceFrame
    {
        StudioBackwardTraceFrameKind Kind{
            StudioBackwardTraceFrameKind::OutputGradients };
        std::size_t LayerIndex{};
        std::uint64_t LayerId{};
    };

    struct StudioBackwardTraceState
    {
        bool Active{};
        Core::BackwardTraceSnapshot Trace;
        std::uint64_t FocusedNeuronId{};
        std::vector<StudioBackwardTraceFrame> PlaybackFrames;
        std::size_t PlaybackFrameIndex{};
        StudioForwardTracePlaybackStatus PlaybackStatus{
            StudioForwardTracePlaybackStatus::Paused };
        double PlaybackFrameDurationSeconds{ 0.65 };
        double PlaybackFrameElapsedSeconds{};
    };

    struct StudioTrainingTimelineState
    {
        Core::TrainingSessionSnapshot Session;
        Core::TrainingDebugSnapshot Debug;
        std::vector<Core::TrainingHistoryEntrySnapshot> History;
        bool HasSelectedStep{};
        std::size_t SelectedStepIndex{};
        Core::TrainingStepSnapshot SelectedStep;
    };

    enum class StudioSignalHealthFilter
    {
        AllFindings,
        Inactive,
        Saturated,
        VanishingGradient,
        ExplodingGradient
    };

    struct StudioSignalHealthState
    {
        bool Active{};
        StudioSignalHealthFilter Filter{
            StudioSignalHealthFilter::AllFindings };
        Core::SignalHealthSnapshot Snapshot;
    };

    struct StudioModelCheckpointState
    {
        std::vector<Core::ModelCheckpointSummarySnapshot> Checkpoints;
        bool HasSelectedCheckpoint{};
        Core::ModelCheckpointSnapshot SelectedCheckpoint;
        bool HasComparison{};
        Core::ModelCheckpointComparisonSnapshot Comparison;
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
        StudioBackwardTraceState BackwardTrace;
        StudioSignalHealthState SignalHealth;
        StudioModelCheckpointState ModelCheckpoints;
        StudioTrainingTimelineState TrainingTimeline;
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
        bool PlayForwardTrace();
        bool PauseForwardTrace();
        bool RestartForwardTrace();
        bool StepForwardTraceForward();
        bool StepForwardTraceBackward();
        bool AdvanceForwardTracePlayback(double elapsedSeconds);
        bool SetForwardTraceFrameDuration(double durationSeconds);
        bool RunBackwardTrace(
            const std::vector<double>& inputs,
            const std::vector<double>& targets);
        void ClearBackwardTrace();
        bool FocusBackwardTraceNeuron(std::uint64_t neuronId);
        bool PlayBackwardTrace();
        bool PauseBackwardTrace();
        bool RestartBackwardTrace();
        bool StepBackwardTraceForward();
        bool StepBackwardTraceBackward();
        bool AdvanceBackwardTracePlayback(double elapsedSeconds);
        bool SetBackwardTraceFrameDuration(double durationSeconds);
        bool RunSignalHealthDiagnostics(
            const Core::SignalHealthConfiguration& configuration);
        void ClearSignalHealthDiagnostics();
        void SetSignalHealthFilter(StudioSignalHealthFilter filter);
        void RefreshModelCheckpoints();
        bool CaptureModelCheckpoint(const std::string& name);
        bool SelectModelCheckpoint(std::uint64_t checkpointId);
        bool CompareModelCheckpoints(
            std::uint64_t firstCheckpointId,
            std::uint64_t secondCheckpointId);
        bool RestoreModelCheckpoint(std::uint64_t checkpointId);
        bool RemoveModelCheckpoint(std::uint64_t checkpointId);
        bool ClearModelCheckpoints();
        void ClearModelCheckpointComparison();
        void RefreshTrainingTimeline();
        bool SelectTrainingTimelineStep(std::size_t stepIndex);
        void ClearTrainingTimelineSelection();

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
        void BuildForwardTracePlaybackFrames();
        bool ContainsBackwardTraceNeuron(std::uint64_t neuronId) const;
        void BuildBackwardTracePlaybackFrames();

        std::string WorkingDirectory;
        StudioViewMode ViewMode{ StudioViewMode::TwoDimensional };
        std::size_t RelationshipLimit{ 10 };
        double ForwardTraceFrameDurationSeconds{ 0.65 };
        double BackwardTraceFrameDurationSeconds{ 0.65 };
        StudioState CurrentState;
    };
}
