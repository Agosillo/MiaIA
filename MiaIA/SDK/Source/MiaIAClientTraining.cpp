#include "../Include/MiaIAClient.h"
#include "MiaIAClientState.h"

#include "../../Engine/Training/TrainingStepExecutor.h"
#include "../../Engine/Training/TrainingEpochExecutor.h"
#include "../../Core/Model/Dataset.h"

namespace MiaIA::SDK
{
    bool MiaIAClient::TrainDatasetSample(
        std::size_t index,
        double learningRate,
        Core::LossType lossType,
        Core::OptimizerType optimizerType,
        Core::TrainingStepSnapshot& result)
    {
        return Engine::TrainingStepExecutor::Run(
            Detail::ClientDataset(),
            index,
            Detail::ClientNetwork(),
            learningRate,
            lossType,
            optimizerType,
            result);
    }

    bool MiaIAClient::TrainDatasetEpoch(
        double learningRate,
        Core::LossType lossType,
        Core::OptimizerType optimizerType,
        Core::TrainingEpochSnapshot& result)
    {
        return Engine::TrainingEpochExecutor::Run(
            Detail::ClientDataset(),
            Detail::ClientNetwork(),
            learningRate,
            lossType,
            optimizerType,
            result);
    }
}
