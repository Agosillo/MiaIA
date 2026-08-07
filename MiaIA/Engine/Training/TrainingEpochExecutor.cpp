#include "TrainingEpochExecutor.h"

#include "TrainingStepExecutor.h"
#include "../../Core/Model/Dataset.h"
#include "../../Core/Model/Network.h"

#include <cmath>
#include <utility>
#include <vector>

namespace MiaIA::Engine
{
    bool TrainingEpochExecutor::Run(
        const Core::Dataset& dataset,
        Core::Network& network,
        double learningRate,
        Core::LossType lossType,
        Core::OptimizerType optimizerType,
        Core::TrainingEpochSnapshot& result)
    {
        if (dataset.Samples.empty() ||
            !std::isfinite(learningRate) ||
            learningRate <= 0.0 ||
            lossType != Core::LossType::MeanSquaredError ||
            optimizerType !=
                Core::OptimizerType::StochasticGradientDescent)
        {
            return false;
        }

        Core::Network candidate = network;
        std::vector<Core::TrainingStepSnapshot> steps;
        steps.reserve(dataset.Samples.size());

        double meanLossBeforeUpdate = 0.0;
        double meanLossAfterUpdate = 0.0;

        for (std::size_t sampleIndex = 0;
            sampleIndex < dataset.Samples.size();
            ++sampleIndex)
        {
            Core::TrainingStepSnapshot step;

            if (!TrainingStepExecutor::Run(
                dataset,
                sampleIndex,
                candidate,
                learningRate,
                lossType,
                optimizerType,
                step))
            {
                return false;
            }

            const double sampleCount =
                static_cast<double>(sampleIndex + 1);

            meanLossBeforeUpdate +=
                (step.Before.Evaluation.Loss - meanLossBeforeUpdate) /
                sampleCount;

            meanLossAfterUpdate +=
                (step.After.Loss - meanLossAfterUpdate) /
                sampleCount;

            if (!std::isfinite(meanLossBeforeUpdate) ||
                !std::isfinite(meanLossAfterUpdate))
            {
                return false;
            }

            steps.push_back(std::move(step));
        }

        Core::TrainingEpochSnapshot epoch;
        epoch.SampleCount = dataset.Samples.size();
        epoch.LearningRate = learningRate;
        epoch.Loss = lossType;
        epoch.Optimizer = optimizerType;
        epoch.MeanLossBeforeUpdate = meanLossBeforeUpdate;
        epoch.MeanLossAfterUpdate = meanLossAfterUpdate;
        epoch.Steps = std::move(steps);

        network = std::move(candidate);
        result = std::move(epoch);

        return true;
    }
}
