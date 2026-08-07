#include "TrainingStepExecutor.h"

#include "TrainingDebugController.h"

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
        return TrainingDebugController::RunToCommit(
            dataset,
            network,
            sampleIndex,
            learningRate,
            lossType,
            optimizerType,
            result);
    }
}
