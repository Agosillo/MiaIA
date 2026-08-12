#include "TrainingStepComparer.h"

#include <algorithm>
#include <cmath>
#include <map>

namespace
{
    MiaIA::Core::TrainingValueComparisonSnapshot CompareValues(
        double first,
        double second)
    {
        MiaIA::Core::TrainingValueComparisonSnapshot comparison;
        comparison.FirstValue = first;
        comparison.SecondValue = second;
        comparison.Delta = second - first;
        comparison.AbsoluteDelta = std::abs(comparison.Delta);
        return comparison;
    }
}

namespace MiaIA::Engine
{
    bool TrainingStepComparer::Compare(
        const Core::TrainingStepSnapshot& first,
        std::size_t firstStepIndex,
        const Core::TrainingStepSnapshot& second,
        std::size_t secondStepIndex,
        Core::TrainingStepComparisonSnapshot& result)
    {
        Core::TrainingStepComparisonSnapshot candidate;
        candidate.FirstStepIndex = firstStepIndex;
        candidate.SecondStepIndex = secondStepIndex;
        candidate.FirstSampleIndex = first.SampleIndex;
        candidate.SecondSampleIndex = second.SampleIndex;
        candidate.SameSample = first.SampleIndex == second.SampleIndex;
        candidate.LossBefore = CompareValues(
            first.Before.Evaluation.Loss,
            second.Before.Evaluation.Loss);
        candidate.LossAfter = CompareValues(
            first.After.Loss,
            second.After.Loss);

        const std::size_t outputCount = std::max(
            std::max(
                first.Before.Evaluation.Predictions.size(),
                first.After.Predictions.size()),
            std::max(
                second.Before.Evaluation.Predictions.size(),
                second.After.Predictions.size()));
        candidate.Outputs.reserve(outputCount);

        for (std::size_t outputIndex = 0;
            outputIndex < outputCount;
            ++outputIndex)
        {
            Core::TrainingOutputComparisonSnapshot output;
            output.OutputIndex = outputIndex;
            output.HasFirstPrediction =
                outputIndex < first.Before.Evaluation.Predictions.size() &&
                outputIndex < first.After.Predictions.size();
            output.HasSecondPrediction =
                outputIndex < second.Before.Evaluation.Predictions.size() &&
                outputIndex < second.After.Predictions.size();

            if (output.HasFirstPrediction && output.HasSecondPrediction)
            {
                output.BeforePrediction = CompareValues(
                    first.Before.Evaluation.Predictions[outputIndex],
                    second.Before.Evaluation.Predictions[outputIndex]);
                output.AfterPrediction = CompareValues(
                    first.After.Predictions[outputIndex],
                    second.After.Predictions[outputIndex]);
            }

            candidate.Outputs.push_back(output);
        }

        std::map<std::uint64_t, Core::TrainingNeuronComparisonSnapshot>
            neuronComparisons;

        for (const Core::NeuronGradientSnapshot& gradient :
            first.Before.Neurons)
        {
            auto& comparison = neuronComparisons[gradient.Id];
            comparison.Id = gradient.Id;
            comparison.LayerOrder = gradient.LayerOrder;
            comparison.HasFirstGradient = true;
            comparison.ActivationGradient.FirstValue =
                gradient.ActivationGradient;
            comparison.PreActivationGradient.FirstValue =
                gradient.PreActivationGradient;
            comparison.BiasGradient.FirstValue = gradient.BiasGradient;
        }

        for (const Core::NeuronGradientSnapshot& gradient :
            second.Before.Neurons)
        {
            auto& comparison = neuronComparisons[gradient.Id];
            comparison.Id = gradient.Id;
            comparison.LayerOrder = gradient.LayerOrder;
            comparison.HasSecondGradient = true;
            comparison.ActivationGradient.SecondValue =
                gradient.ActivationGradient;
            comparison.PreActivationGradient.SecondValue =
                gradient.PreActivationGradient;
            comparison.BiasGradient.SecondValue = gradient.BiasGradient;
        }

        for (const Core::NeuronUpdateSnapshot& update : first.NeuronUpdates)
        {
            auto& comparison = neuronComparisons[update.Id];
            comparison.Id = update.Id;
            comparison.HasFirstUpdate = true;
            comparison.Bias.FirstValue = update.UpdatedBias;
        }

        for (const Core::NeuronUpdateSnapshot& update : second.NeuronUpdates)
        {
            auto& comparison = neuronComparisons[update.Id];
            comparison.Id = update.Id;
            comparison.HasSecondUpdate = true;
            comparison.Bias.SecondValue = update.UpdatedBias;
        }

        candidate.Neurons.reserve(neuronComparisons.size());

        for (auto& entry : neuronComparisons)
        {
            auto& comparison = entry.second;

            if (comparison.HasFirstGradient &&
                comparison.HasSecondGradient)
            {
                comparison.ActivationGradient = CompareValues(
                    comparison.ActivationGradient.FirstValue,
                    comparison.ActivationGradient.SecondValue);
                comparison.PreActivationGradient = CompareValues(
                    comparison.PreActivationGradient.FirstValue,
                    comparison.PreActivationGradient.SecondValue);
                comparison.BiasGradient = CompareValues(
                    comparison.BiasGradient.FirstValue,
                    comparison.BiasGradient.SecondValue);
            }

            if (comparison.HasFirstUpdate && comparison.HasSecondUpdate)
            {
                comparison.Bias = CompareValues(
                    comparison.Bias.FirstValue,
                    comparison.Bias.SecondValue);
            }

            candidate.Neurons.push_back(comparison);
        }

        std::map<std::uint64_t, Core::TrainingConnectionComparisonSnapshot>
            connectionComparisons;

        for (const Core::ConnectionGradientSnapshot& gradient :
            first.Before.Connections)
        {
            auto& comparison = connectionComparisons[gradient.Id];
            comparison.Id = gradient.Id;
            comparison.FromNeuron = gradient.FromNeuron;
            comparison.ToNeuron = gradient.ToNeuron;
            comparison.HasFirstGradient = true;
            comparison.WeightGradient.FirstValue = gradient.WeightGradient;
        }

        for (const Core::ConnectionGradientSnapshot& gradient :
            second.Before.Connections)
        {
            auto& comparison = connectionComparisons[gradient.Id];
            comparison.Id = gradient.Id;
            comparison.FromNeuron = gradient.FromNeuron;
            comparison.ToNeuron = gradient.ToNeuron;
            comparison.HasSecondGradient = true;
            comparison.WeightGradient.SecondValue = gradient.WeightGradient;
        }

        for (const Core::ConnectionUpdateSnapshot& update :
            first.ConnectionUpdates)
        {
            auto& comparison = connectionComparisons[update.Id];
            comparison.Id = update.Id;
            comparison.HasFirstUpdate = true;
            comparison.Weight.FirstValue = update.UpdatedWeight;
        }

        for (const Core::ConnectionUpdateSnapshot& update :
            second.ConnectionUpdates)
        {
            auto& comparison = connectionComparisons[update.Id];
            comparison.Id = update.Id;
            comparison.HasSecondUpdate = true;
            comparison.Weight.SecondValue = update.UpdatedWeight;
        }

        candidate.Connections.reserve(connectionComparisons.size());

        for (auto& entry : connectionComparisons)
        {
            auto& comparison = entry.second;

            if (comparison.HasFirstGradient &&
                comparison.HasSecondGradient)
            {
                comparison.WeightGradient = CompareValues(
                    comparison.WeightGradient.FirstValue,
                    comparison.WeightGradient.SecondValue);
            }

            if (comparison.HasFirstUpdate && comparison.HasSecondUpdate)
            {
                comparison.Weight = CompareValues(
                    comparison.Weight.FirstValue,
                    comparison.Weight.SecondValue);
            }

            candidate.Connections.push_back(comparison);
        }

        result = std::move(candidate);
        return true;
    }
}
