#include "TrainingDebugController.h"

#include "../Differentiation/BackwardEngine.h"
#include "../Evaluation/SampleEvaluator.h"
#include "../Inspection/NetworkInspector.h"
#include "../Optimization/SgdOptimizer.h"
#include "../../Core/Model/Dataset.h"
#include "../../Core/Model/Network.h"
#include "../../Core/Model/TrainingDebugSession.h"

#include <cmath>
#include <utility>

namespace
{
    bool IsActive(MiaIA::Core::TrainingDebugPhase phase)
    {
        return phase >= MiaIA::Core::TrainingDebugPhase::BeforeForward &&
            phase < MiaIA::Core::TrainingDebugPhase::Committed;
    }

    bool InitializeSession(
        const MiaIA::Core::Dataset& dataset,
        const MiaIA::Core::Network& network,
        std::size_t sampleIndex,
        double learningRate,
        MiaIA::Core::LossType lossType,
        MiaIA::Core::OptimizerType optimizerType,
        MiaIA::Core::TrainingDebugSession& session)
    {
        if (IsActive(session.Phase) ||
            sampleIndex >= dataset.Samples.size() ||
            !std::isfinite(learningRate) ||
            learningRate <= 0.0 ||
            lossType != MiaIA::Core::LossType::MeanSquaredError ||
            optimizerType !=
                MiaIA::Core::OptimizerType::StochasticGradientDescent)
        {
            return false;
        }

        MiaIA::Core::TrainingDebugSession candidate;
        candidate.Phase =
            MiaIA::Core::TrainingDebugPhase::BeforeForward;
        candidate.SampleIndex = sampleIndex;
        candidate.LearningRate = learningRate;
        candidate.Loss = lossType;
        candidate.Optimizer = optimizerType;
        candidate.CandidateNetwork = network;
        candidate.Step.SampleIndex = sampleIndex;
        candidate.Step.LearningRate = learningRate;
        candidate.Step.Optimizer = optimizerType;

        session = std::move(candidate);
        return true;
    }

    bool AdvancePhase(
        const MiaIA::Core::Dataset& dataset,
        MiaIA::Core::Network& network,
        MiaIA::Core::TrainingDebugSession& session,
        bool preserveCandidate)
    {
        switch (session.Phase)
        {
        case MiaIA::Core::TrainingDebugPhase::BeforeForward:
            if (!MiaIA::Engine::SampleEvaluator::Evaluate(
                dataset,
                session.SampleIndex,
                session.CandidateNetwork,
                session.Loss,
                session.Step.Before.Evaluation))
            {
                return false;
            }

            session.Phase =
                MiaIA::Core::TrainingDebugPhase::ForwardComplete;
            return true;

        case MiaIA::Core::TrainingDebugPhase::ForwardComplete:
            if (!MiaIA::Engine::BackwardEngine::Run(
                session.CandidateNetwork,
                session.Step.Before.Evaluation,
                session.Step.Before))
            {
                return false;
            }

            session.Phase =
                MiaIA::Core::TrainingDebugPhase::BackwardComplete;
            return true;

        case MiaIA::Core::TrainingDebugPhase::BackwardComplete:
            if (!MiaIA::Engine::SgdOptimizer::Apply(
                session.CandidateNetwork,
                session.Step.Before,
                session.LearningRate,
                session.Step.ConnectionUpdates,
                session.Step.NeuronUpdates))
            {
                return false;
            }

            session.Phase =
                MiaIA::Core::TrainingDebugPhase::UpdateComplete;
            return true;

        case MiaIA::Core::TrainingDebugPhase::UpdateComplete:
            if (!MiaIA::Engine::SampleEvaluator::Evaluate(
                dataset,
                session.SampleIndex,
                session.CandidateNetwork,
                session.Loss,
                session.Step.After))
            {
                return false;
            }

            session.Phase = MiaIA::Core::TrainingDebugPhase::Verified;
            return true;

        case MiaIA::Core::TrainingDebugPhase::Verified:
            if (preserveCandidate)
            {
                network = session.CandidateNetwork;
            }
            else
            {
                network = std::move(session.CandidateNetwork);
            }

            session.Phase = MiaIA::Core::TrainingDebugPhase::Committed;
            return true;

        default:
            return false;
        }
    }
}

namespace MiaIA::Engine
{
    bool TrainingDebugController::Start(
        const Core::Dataset& dataset,
        const Core::Network& network,
        std::size_t sampleIndex,
        double learningRate,
        Core::LossType lossType,
        Core::OptimizerType optimizerType,
        Core::TrainingDebugSession& session,
        Core::TrainingDebugSnapshot& result)
    {
        if (IsActive(session.Phase))
        {
            return false;
        }

        Core::TrainingDebugSession candidate;

        if (!InitializeSession(
            dataset,
            network,
            sampleIndex,
            learningRate,
            lossType,
            optimizerType,
            candidate))
        {
            return false;
        }

        Core::TrainingDebugSnapshot snapshot = Snapshot(candidate);
        session = std::move(candidate);
        result = std::move(snapshot);
        return true;
    }

    bool TrainingDebugController::Next(
        const Core::Dataset& dataset,
        Core::Network& network,
        Core::TrainingDebugSession& session,
        Core::TrainingDebugSnapshot& result)
    {
        Core::TrainingDebugSession candidate = session;

        if (!AdvancePhase(dataset, network, candidate, true))
        {
            return false;
        }

        Core::TrainingDebugSnapshot snapshot = Snapshot(candidate);
        session = std::move(candidate);
        result = std::move(snapshot);
        return true;
    }

    Core::TrainingDebugSnapshot TrainingDebugController::Snapshot(
        const Core::TrainingDebugSession& session)
    {
        Core::TrainingDebugSnapshot snapshot;
        snapshot.Phase = session.Phase;
        snapshot.SampleIndex = session.SampleIndex;
        snapshot.LearningRate = session.LearningRate;
        snapshot.Loss = session.Loss;
        snapshot.Optimizer = session.Optimizer;
        snapshot.CandidateNetwork =
            NetworkInspector::Snapshot(session.CandidateNetwork);
        snapshot.Step = session.Step;
        return snapshot;
    }

    bool TrainingDebugController::Cancel(
        Core::TrainingDebugSession& session)
    {
        if (!IsActive(session.Phase))
        {
            return false;
        }

        session = {};
        return true;
    }

    bool TrainingDebugController::RunToCommit(
        const Core::Dataset& dataset,
        Core::Network& network,
        std::size_t sampleIndex,
        double learningRate,
        Core::LossType lossType,
        Core::OptimizerType optimizerType,
        Core::TrainingStepSnapshot& result)
    {
        Core::TrainingDebugSession session;
        if (!InitializeSession(
            dataset,
            network,
            sampleIndex,
            learningRate,
            lossType,
            optimizerType,
            session))
        {
            return false;
        }

        while (session.Phase != Core::TrainingDebugPhase::Committed)
        {
            if (!AdvancePhase(dataset, network, session, false))
            {
                return false;
            }
        }

        result = std::move(session.Step);
        return true;
    }
}
