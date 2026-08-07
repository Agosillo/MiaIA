#include "SampleGradientEvaluator.h"

#include "BackwardEngine.h"
#include "../Evaluation/SampleEvaluator.h"

namespace MiaIA::Engine
{
    bool SampleGradientEvaluator::Evaluate(
        const Core::Dataset& dataset,
        std::size_t index,
        Core::Network& network,
        Core::LossType type,
        Core::SampleGradientSnapshot& result)
    {
        Core::SampleEvaluationSnapshot evaluation;

        if (!SampleEvaluator::Evaluate(
            dataset,
            index,
            network,
            type,
            evaluation))
        {
            return false;
        }

        return BackwardEngine::Run(
            network,
            evaluation,
            result);
    }
}
