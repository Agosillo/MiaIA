#include "LossEvaluator.h"

#include <cmath>
#include <utility>

namespace MiaIA::Engine
{
    bool LossEvaluator::Evaluate(
        const std::vector<double>& predictions,
        const std::vector<double>& targets,
        Core::LossType type,
        std::vector<double>& errors,
        double& loss)
    {
        if (type != Core::LossType::MeanSquaredError ||
            predictions.empty() ||
            predictions.size() != targets.size())
        {
            return false;
        }

        std::vector<double> calculatedErrors;
        calculatedErrors.reserve(predictions.size());

        double squaredErrorSum = 0.0;

        for (std::size_t index = 0;
            index < predictions.size();
            ++index)
        {
            if (!std::isfinite(predictions[index]) ||
                !std::isfinite(targets[index]))
            {
                return false;
            }

            const double error =
                predictions[index] - targets[index];

            const double squaredError = error * error;

            if (!std::isfinite(error) ||
                !std::isfinite(squaredError) ||
                !std::isfinite(squaredErrorSum + squaredError))
            {
                return false;
            }

            calculatedErrors.push_back(error);
            squaredErrorSum += squaredError;
        }

        const double calculatedLoss =
            squaredErrorSum /
            static_cast<double>(predictions.size());

        if (!std::isfinite(calculatedLoss))
        {
            return false;
        }

        errors = std::move(calculatedErrors);
        loss = calculatedLoss;

        return true;
    }

    bool LossEvaluator::EvaluateGradient(
        const std::vector<double>& predictions,
        const std::vector<double>& targets,
        Core::LossType type,
        std::vector<double>& gradients)
    {
        if (type != Core::LossType::MeanSquaredError ||
            predictions.empty() ||
            predictions.size() != targets.size())
        {
            return false;
        }

        const double scale =
            2.0 / static_cast<double>(predictions.size());

        std::vector<double> calculatedGradients;
        calculatedGradients.reserve(predictions.size());

        for (std::size_t index = 0;
            index < predictions.size();
            ++index)
        {
            if (!std::isfinite(predictions[index]) ||
                !std::isfinite(targets[index]))
            {
                return false;
            }

            const double gradient =
                scale * (predictions[index] - targets[index]);

            if (!std::isfinite(gradient))
            {
                return false;
            }

            calculatedGradients.push_back(gradient);
        }

        gradients = std::move(calculatedGradients);

        return true;
    }
}
