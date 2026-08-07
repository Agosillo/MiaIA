#include "DatasetEvaluator.h"

#include "SampleEvaluator.h"
#include "../../Core/Model/Dataset.h"
#include "../../Core/Model/Network.h"

#include <cmath>
#include <utility>
#include <vector>

namespace MiaIA::Engine
{
    bool DatasetEvaluator::Evaluate(
        const Core::Dataset& dataset,
        const Core::Network& network,
        Core::LossType type,
        Core::DatasetEvaluationSnapshot& result)
    {
        if (dataset.Samples.empty() ||
            type != Core::LossType::MeanSquaredError)
        {
            return false;
        }

        Core::Network candidate = network;
        std::vector<Core::SampleEvaluationSnapshot> evaluations;
        evaluations.reserve(dataset.Samples.size());

        double meanLoss = 0.0;

        for (std::size_t sampleIndex = 0;
            sampleIndex < dataset.Samples.size();
            ++sampleIndex)
        {
            Core::SampleEvaluationSnapshot evaluation;

            if (!SampleEvaluator::Evaluate(
                dataset,
                sampleIndex,
                candidate,
                type,
                evaluation))
            {
                return false;
            }

            const double sampleCount =
                static_cast<double>(sampleIndex + 1);

            meanLoss +=
                (evaluation.Loss - meanLoss) / sampleCount;

            if (!std::isfinite(meanLoss))
            {
                return false;
            }

            evaluations.push_back(std::move(evaluation));
        }

        Core::DatasetEvaluationSnapshot datasetEvaluation;
        datasetEvaluation.SampleCount = dataset.Samples.size();
        datasetEvaluation.Type = type;
        datasetEvaluation.MeanLoss = meanLoss;
        datasetEvaluation.Evaluations = std::move(evaluations);

        result = std::move(datasetEvaluation);

        return true;
    }
}
