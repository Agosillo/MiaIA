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
#include <numeric>
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

    std::uint64_t NextDeterministicValue(std::uint64_t& state)
    {
        state += 0x9e3779b97f4a7c15ull;
        std::uint64_t value = state;
        value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ull;
        value = (value ^ (value >> 27)) * 0x94d049bb133111ebull;
        return value ^ (value >> 31);
    }

    std::size_t NextBoundedIndex(
        std::uint64_t& state,
        std::size_t exclusiveUpperBound)
    {
        const std::uint64_t bound =
            static_cast<std::uint64_t>(exclusiveUpperBound);
        const std::uint64_t rejectionThreshold =
            (0ull - bound) % bound;
        std::uint64_t value{};

        do
        {
            value = NextDeterministicValue(state);
        }
        while (value < rejectionThreshold);

        return static_cast<std::size_t>(value % bound);
    }

    bool BuildEpochSampleOrder(
        std::size_t sampleCount,
        MiaIA::Core::TrainingSampleOrder sampleOrder,
        std::uint64_t seed,
        std::size_t epochIndex,
        std::vector<std::size_t>& result)
    {
        if (sampleCount == 0 ||
            (sampleOrder != MiaIA::Core::TrainingSampleOrder::Sequential &&
                sampleOrder != MiaIA::Core::TrainingSampleOrder::
                    ShuffleEachEpoch))
        {
            return false;
        }

        std::vector<std::size_t> order(sampleCount);
        std::iota(order.begin(), order.end(), std::size_t{});

        if (sampleOrder ==
            MiaIA::Core::TrainingSampleOrder::ShuffleEachEpoch)
        {
            std::uint64_t state = seed +
                static_cast<std::uint64_t>(epochIndex) *
                    0x9e3779b97f4a7c15ull;

            for (std::size_t remaining = sampleCount;
                remaining > 1;
                --remaining)
            {
                const std::size_t selected =
                    NextBoundedIndex(state, remaining);
                std::swap(order[remaining - 1], order[selected]);
            }
        }

        result = std::move(order);
        return true;
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
        return Start(
            dataset,
            network,
            epochCount,
            learningRate,
            lossType,
            optimizerType,
            Core::TrainingSampleOrder::Sequential,
            0,
            session,
            result);
    }

    bool TrainingSessionController::Start(
        const Core::Dataset& dataset,
        const Core::Network& network,
        std::size_t epochCount,
        double learningRate,
        Core::LossType lossType,
        Core::OptimizerType optimizerType,
        Core::TrainingSampleOrder sampleOrder,
        std::uint64_t seed,
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
            (sampleOrder != Core::TrainingSampleOrder::Sequential &&
                sampleOrder !=
                    Core::TrainingSampleOrder::ShuffleEachEpoch) ||
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
        candidate.SampleOrder = sampleOrder;
        candidate.Seed = sampleOrder ==
            Core::TrainingSampleOrder::ShuffleEachEpoch
            ? seed
            : 0;
        candidate.Breakpoints = session.Breakpoints;
        candidate.NextBreakpointId = session.NextBreakpointId;

        if (!BuildEpochSampleOrder(
                candidate.SampleCount,
                candidate.SampleOrder,
                candidate.Seed,
                0,
                candidate.CurrentEpochSampleOrder))
        {
            return false;
        }

        candidate.NextSampleIndex =
            candidate.CurrentEpochSampleOrder.front();

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
            session.NextSamplePosition >= session.SampleCount ||
            session.CurrentEpochSampleOrder.size() != session.SampleCount ||
            session.CurrentEpochSampleOrder[session.NextSamplePosition] >=
                session.SampleCount ||
            session.NextSampleIndex !=
                session.CurrentEpochSampleOrder[
                    session.NextSamplePosition] ||
            session.CurrentEpoch >= session.EpochCount ||
            sampleIndex != session.NextSampleIndex)
        {
            return false;
        }

        const std::size_t expectedStepCount =
            session.CurrentEpoch * session.SampleCount +
            session.NextSamplePosition;

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

        const bool completesEpoch =
            session.NextSamplePosition + 1 == session.SampleCount;
        const bool completesSession = completesEpoch &&
            session.CurrentEpoch + 1 == session.EpochCount;
        std::vector<std::size_t> nextEpochOrder;

        if (completesEpoch && !completesSession &&
            !BuildEpochSampleOrder(
                session.SampleCount,
                session.SampleOrder,
                session.Seed,
                session.CurrentEpoch + 1,
                nextEpochOrder))
        {
            return false;
        }

        session.Steps.push_back(step);
        session.WorkerStopReason = Core::TrainingWorkerStopReason::None;
        ++session.NextSamplePosition;

        if (completesEpoch)
        {
            session.NextSamplePosition = 0;
            ++session.CurrentEpoch;

            if (completesSession)
            {
                session.Status = Core::TrainingSessionStatus::Completed;
                session.NextSampleIndex = 0;
            }
            else
            {
                session.CurrentEpochSampleOrder =
                    std::move(nextEpochOrder);
                session.NextSampleIndex =
                    session.CurrentEpochSampleOrder.front();
            }
        }
        else
        {
            session.NextSampleIndex =
                session.CurrentEpochSampleOrder[
                    session.NextSamplePosition];
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
        snapshot.NextSamplePosition = session.NextSamplePosition;
        snapshot.SampleCount = session.SampleCount;
        snapshot.CompletedSteps = session.Steps.size();
        snapshot.TotalSteps = session.EpochCount * session.SampleCount;
        snapshot.LearningRate = session.LearningRate;
        snapshot.Loss = session.Loss;
        snapshot.Optimizer = session.Optimizer;
        snapshot.SampleOrder = session.SampleOrder;
        snapshot.Seed = session.Seed;
        snapshot.CurrentEpochSampleOrder =
            session.CurrentEpochSampleOrder;
        snapshot.Breakpoints = session.Breakpoints;
        snapshot.HasBreakpointHit = session.HasBreakpointHit;
        snapshot.LastBreakpointHit = session.LastBreakpointHit;
        snapshot.Steps = session.Steps;

        return snapshot;
    }
}
