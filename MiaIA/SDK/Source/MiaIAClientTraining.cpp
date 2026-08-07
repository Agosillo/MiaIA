#include "../Include/MiaIAClient.h"
#include "MiaIAClientState.h"

#include "../../Engine/Training/TrainingStepExecutor.h"
#include "../../Engine/Training/TrainingEpochExecutor.h"
#include "../../Engine/Training/TrainingSessionController.h"
#include "../../Core/Model/Dataset.h"
#include "../../Core/Model/TrainingSession.h"

namespace MiaIA::SDK
{
    bool MiaIAClient::TrainDatasetSample(
        std::size_t index,
        double learningRate,
        Core::LossType lossType,
        Core::OptimizerType optimizerType,
        Core::TrainingStepSnapshot& result)
    {
        const std::scoped_lock lock(Detail::ClientMutex());

        if (Detail::IsTrainingSessionRunning())
        {
            return false;
        }

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
        const std::scoped_lock lock(Detail::ClientMutex());

        if (Detail::IsTrainingSessionRunning())
        {
            return false;
        }

        return Engine::TrainingEpochExecutor::Run(
            Detail::ClientDataset(),
            Detail::ClientNetwork(),
            learningRate,
            lossType,
            optimizerType,
            result);
    }

    bool MiaIAClient::StartTrainingSession(
        std::size_t epochCount,
        double learningRate,
        Core::LossType lossType,
        Core::OptimizerType optimizerType,
        Core::TrainingSessionSnapshot& result)
    {
        const std::scoped_lock lock(Detail::ClientMutex());
        return Engine::TrainingSessionController::Start(
            Detail::ClientDataset(),
            Detail::ClientNetwork(),
            epochCount,
            learningRate,
            lossType,
            optimizerType,
            Detail::ClientTrainingSession(),
            result);
    }

    Core::TrainingSessionSnapshot MiaIAClient::GetTrainingSession()
    {
        const std::scoped_lock lock(Detail::ClientMutex());
        return Engine::TrainingSessionController::Snapshot(
            Detail::ClientTrainingSession());
    }

    bool MiaIAClient::AdvanceTrainingSession(
        Core::TrainingStepSnapshot& result)
    {
        const std::scoped_lock lock(Detail::ClientMutex());

        if (Detail::ClientTrainingSession().Status !=
            Core::TrainingSessionStatus::Active)
        {
            return false;
        }

        return Engine::TrainingSessionController::Next(
            Detail::ClientDataset(),
            Detail::ClientNetwork(),
            Detail::ClientTrainingSession(),
            result);
    }

    bool MiaIAClient::RunTrainingSession(
        std::size_t maximumSteps,
        Core::TrainingRunSnapshot& result)
    {
        const std::scoped_lock lock(Detail::ClientMutex());
        return Engine::TrainingSessionController::Run(
            Detail::ClientDataset(),
            Detail::ClientNetwork(),
            Detail::ClientTrainingSession(),
            maximumSteps,
            result);
    }
}
