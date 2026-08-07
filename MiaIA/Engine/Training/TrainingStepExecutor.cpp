#include "TrainingStepExecutor.h"

#include "../Differentiation/SampleGradientEvaluator.h"
#include "../Evaluation/SampleEvaluator.h"
#include "../Optimization/SgdOptimizer.h"
#include "../../Core/Model/Dataset.h"
#include "../../Core/Model/Network.h"

#include <cmath>
#include <utility>
#include <vector>

namespace MiaIA::Engine
{
    bool TrainingStepExecutor::Run(
        const Core::Dataset& dataset,
        std::size_t sampleIndex,
        Core::Network& network,
        double learningRate,
        Core::LossType lossType,
        Core::OptimizerType optimizerType,
        Core::TrainingStepSnapshot& result)
    {
        if (!std::isfinite(learningRate) ||
            learningRate <= 0.0 ||
            lossType != Core::LossType::MeanSquaredError ||
            optimizerType !=
                Core::OptimizerType::StochasticGradientDescent)
        {
            return false;
        }

        Core::Network candidate = network;
        Core::SampleGradientSnapshot gradients;

        if (!SampleGradientEvaluator::Evaluate(
            dataset,
            sampleIndex,
            candidate,
            lossType,
            gradients))
        {
            return false;
        }

        std::vector<Core::ConnectionUpdateSnapshot> connectionUpdates;
        std::vector<Core::NeuronUpdateSnapshot> neuronUpdates;

        if (!SgdOptimizer::Apply(
            candidate,
            gradients,
            learningRate,
            connectionUpdates,
            neuronUpdates))
        {
            return false;
        }

        Core::SampleEvaluationSnapshot afterEvaluation;

        if (!SampleEvaluator::Evaluate(
            dataset,
            sampleIndex,
            candidate,
            lossType,
            afterEvaluation))
        {
            return false;
        }

        Core::TrainingStepSnapshot step;
        step.SampleIndex = sampleIndex;
        step.LearningRate = learningRate;
        step.Optimizer = optimizerType;
        step.Before = std::move(gradients);
        step.ConnectionUpdates = std::move(connectionUpdates);
        step.NeuronUpdates = std::move(neuronUpdates);
        step.After = std::move(afterEvaluation);

        network = std::move(candidate);
        result = std::move(step);

        return true;
    }
}
