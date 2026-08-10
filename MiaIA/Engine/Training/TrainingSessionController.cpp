#include "TrainingSessionController.h"

#include "TrainingStepExecutor.h"
#include "TrainingBreakpointController.h"
#include "../Validation/NetworkValidator.h"
#include "../../Core/Model/Dataset.h"
#include "../../Core/Model/Network.h"
#include "../../Core/Model/TrainingSession.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace
{
    bool IsCompatible(
        const MiaIA::Core::Dataset& dataset,
        const MiaIA::Core::Network& network)
    {
        if (dataset.Samples.empty() ||
            !MiaIA::Engine::NetworkValidator::ValidateForForward(network))
        {
            return false;
        }

        const auto inputLayer = std::find_if(
            network.Layers.begin(),
            network.Layers.end(),
            [](const MiaIA::Core::Layer& layer)
            {
                return layer.Order == 0;
            });

        const auto outputLayer = std::max_element(
            network.Layers.begin(),
            network.Layers.end(),
            [](const MiaIA::Core::Layer& left,
                const MiaIA::Core::Layer& right)
            {
                return left.Order < right.Order;
            });

        if (inputLayer == network.Layers.end() ||
            outputLayer == network.Layers.end())
        {
            return false;
        }

        return std::all_of(
            dataset.Samples.begin(),
            dataset.Samples.end(),
            [&](const MiaIA::Core::Sample& sample)
            {
                return sample.Inputs.size() == inputLayer->Neurons.size() &&
                    sample.Targets.size() == outputLayer->Neurons.size();
            });
    }
}

namespace MiaIA::Engine
{
    bool TrainingSessionController::Start(
        const Core::Dataset& dataset,
        const Core::Network& network,
        std::size_t epochCount,
        double learningRate,
        Core::LossType lossType,
        Core::OptimizerType optimizerType,
        Core::TrainingSession& session,
        Core::TrainingSessionSnapshot& result)
    {
        if (session.Status == Core::TrainingSessionStatus::Active ||
            session.Status == Core::TrainingSessionStatus::Running ||
            epochCount == 0 ||
            !std::isfinite(learningRate) ||
            learningRate <= 0.0 ||
            lossType != Core::LossType::MeanSquaredError ||
            optimizerType !=
                Core::OptimizerType::StochasticGradientDescent ||
            !IsCompatible(dataset, network) ||
            epochCount >
                (std::numeric_limits<std::size_t>::max)() /
                dataset.Samples.size())
        {
            return false;
        }

        Core::TrainingSession candidate;
        candidate.Status = Core::TrainingSessionStatus::Active;
        candidate.EpochCount = epochCount;
        candidate.SampleCount = dataset.Samples.size();
        candidate.LearningRate = learningRate;
        candidate.Loss = lossType;
        candidate.Optimizer = optimizerType;
        candidate.Breakpoints = session.Breakpoints;
        candidate.NextBreakpointId = session.NextBreakpointId;

        for (auto& breakpoint : candidate.Breakpoints)
        {
            breakpoint.HitCount = 0;
        }

        Core::TrainingSessionSnapshot snapshot = Snapshot(candidate);

        session = std::move(candidate);
        result = std::move(snapshot);

        return true;
    }

    bool TrainingSessionController::Next(
        const Core::Dataset& dataset,
        Core::Network& network,
        Core::TrainingSession& session,
        Core::TrainingStepSnapshot& result)
    {
        if ((session.Status != Core::TrainingSessionStatus::Active &&
                session.Status != Core::TrainingSessionStatus::Running) ||
            dataset.Samples.size() != session.SampleCount ||
            session.NextSampleIndex >= session.SampleCount ||
            session.CurrentEpoch >= session.EpochCount ||
            !CanRecordStep(session, session.NextSampleIndex) ||
            !IsCompatible(dataset, network))
        {
            return false;
        }

        Core::TrainingStepSnapshot step;

        if (!TrainingStepExecutor::Run(
            dataset,
            session.NextSampleIndex,
            network,
            session.LearningRate,
            session.Loss,
            session.Optimizer,
            step))
        {
            return false;
        }

        if (!RecordStep(session, step))
        {
            return false;
        }

        TrainingBreakpointController::EvaluateCommittedStep(
            network,
            step,
            session);

        result = std::move(step);

        return true;
    }

    bool TrainingSessionController::Cancel(
        Core::TrainingSession& session)
    {
        if (session.Status != Core::TrainingSessionStatus::Active)
        {
            return false;
        }

        session.Status = Core::TrainingSessionStatus::Cancelled;
        return true;
    }

    bool TrainingSessionController::CanRecordStep(
        const Core::TrainingSession& session,
        std::size_t sampleIndex)
    {
        if ((session.Status != Core::TrainingSessionStatus::Active &&
                session.Status != Core::TrainingSessionStatus::Running) ||
            session.SampleCount == 0 ||
            session.NextSampleIndex >= session.SampleCount ||
            session.CurrentEpoch >= session.EpochCount ||
            sampleIndex != session.NextSampleIndex)
        {
            return false;
        }

        const std::size_t expectedStepCount =
            session.CurrentEpoch * session.SampleCount +
            session.NextSampleIndex;

        return session.Steps.size() == expectedStepCount;
    }

    bool TrainingSessionController::RecordStep(
        Core::TrainingSession& session,
        const Core::TrainingStepSnapshot& step)
    {
        if (!CanRecordStep(session, step.SampleIndex))
        {
            return false;
        }

        session.Steps.push_back(step);
        session.WorkerStopReason = Core::TrainingWorkerStopReason::None;
        ++session.NextSampleIndex;

        if (session.NextSampleIndex == session.SampleCount)
        {
            session.NextSampleIndex = 0;
            ++session.CurrentEpoch;

            if (session.CurrentEpoch == session.EpochCount)
            {
                session.Status = Core::TrainingSessionStatus::Completed;
            }
        }

        return true;
    }

    bool TrainingSessionController::Run(
        const Core::Dataset& dataset,
        Core::Network& network,
        Core::TrainingSession& session,
        std::size_t maximumSteps,
        Core::TrainingRunSnapshot& result)
    {
        if (session.Status != Core::TrainingSessionStatus::Active ||
            maximumSteps == 0)
        {
            return false;
        }

        Core::TrainingRunSnapshot run;
        run.RequestedSteps = maximumSteps;
        run.StartEpoch = session.CurrentEpoch;
        run.StartSampleIndex = session.NextSampleIndex;
        run.StopReason = Core::TrainingRunStopReason::StepLimitReached;
        run.Steps.reserve(std::min(
            maximumSteps,
            session.EpochCount * session.SampleCount -
                session.Steps.size()));

        for (std::size_t stepIndex = 0;
            stepIndex < maximumSteps;
            ++stepIndex)
        {
            Core::TrainingStepSnapshot step;

            if (!Next(dataset, network, session, step))
            {
                run.StopReason = Core::TrainingRunStopReason::StepFailed;
                break;
            }

            const double executedSteps =
                static_cast<double>(run.ExecutedSteps + 1);

            run.MeanLossBeforeUpdate +=
                (step.Before.Evaluation.Loss -
                    run.MeanLossBeforeUpdate) /
                executedSteps;
            run.MeanLossAfterUpdate +=
                (step.After.Loss - run.MeanLossAfterUpdate) /
                executedSteps;

            ++run.ExecutedSteps;
            run.Steps.push_back(std::move(step));

            if (session.Status == Core::TrainingSessionStatus::Active &&
                session.WorkerStopReason ==
                    Core::TrainingWorkerStopReason::BreakpointHit)
            {
                run.StopReason =
                    Core::TrainingRunStopReason::BreakpointHit;
                break;
            }

            if (session.Status == Core::TrainingSessionStatus::Completed)
            {
                run.StopReason =
                    Core::TrainingRunStopReason::SessionCompleted;
                break;
            }
        }

        run.EndEpoch = session.CurrentEpoch;
        run.EndSampleIndex = session.NextSampleIndex;
        result = std::move(run);

        return true;
    }

    Core::TrainingSessionSnapshot TrainingSessionController::Snapshot(
        const Core::TrainingSession& session)
    {
        Core::TrainingSessionSnapshot snapshot;
        snapshot.Status = session.Status;
        snapshot.WorkerStopReason = session.WorkerStopReason;
        snapshot.EpochCount = session.EpochCount;
        snapshot.CurrentEpoch = session.CurrentEpoch;
        snapshot.NextSampleIndex = session.NextSampleIndex;
        snapshot.SampleCount = session.SampleCount;
        snapshot.CompletedSteps = session.Steps.size();
        snapshot.TotalSteps = session.EpochCount * session.SampleCount;
        snapshot.LearningRate = session.LearningRate;
        snapshot.Loss = session.Loss;
        snapshot.Optimizer = session.Optimizer;
        snapshot.Breakpoints = session.Breakpoints;
        snapshot.HasBreakpointHit = session.HasBreakpointHit;
        snapshot.LastBreakpointHit = session.LastBreakpointHit;
        snapshot.Steps = session.Steps;

        return snapshot;
    }
}
