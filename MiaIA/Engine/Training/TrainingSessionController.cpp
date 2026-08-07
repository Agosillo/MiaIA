#include "TrainingSessionController.h"

#include "TrainingStepExecutor.h"
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
        if (session.Status != Core::TrainingSessionStatus::Active ||
            dataset.Samples.size() != session.SampleCount ||
            session.NextSampleIndex >= session.SampleCount ||
            session.CurrentEpoch >= session.EpochCount ||
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

        session.Steps.push_back(step);
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

    Core::TrainingSessionSnapshot TrainingSessionController::Snapshot(
        const Core::TrainingSession& session)
    {
        Core::TrainingSessionSnapshot snapshot;
        snapshot.Status = session.Status;
        snapshot.EpochCount = session.EpochCount;
        snapshot.CurrentEpoch = session.CurrentEpoch;
        snapshot.NextSampleIndex = session.NextSampleIndex;
        snapshot.SampleCount = session.SampleCount;
        snapshot.CompletedSteps = session.Steps.size();
        snapshot.TotalSteps = session.EpochCount * session.SampleCount;
        snapshot.LearningRate = session.LearningRate;
        snapshot.Loss = session.Loss;
        snapshot.Optimizer = session.Optimizer;
        snapshot.Steps = session.Steps;

        return snapshot;
    }
}
