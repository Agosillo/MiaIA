#include "../Include/MiaIAClient.h"
#include "MiaIAClientState.h"

#include "../../Engine/Training/TrainingDebugController.h"
#include "../../Engine/Training/TrainingDebugInspector.h"
#include "../../Engine/Training/TrainingSessionDebugController.h"
#include "../../Core/Model/TrainingDebugSession.h"
#include "../../Core/Model/TrainingSession.h"

namespace MiaIA::SDK
{
    bool MiaIAClient::StartTrainingDebug(
        std::size_t sampleIndex,
        double learningRate,
        Core::LossType lossType,
        Core::OptimizerType optimizerType,
        Core::TrainingDebugSnapshot& result)
    {
        const std::scoped_lock lock(Detail::ClientMutex());

        const auto sessionStatus =
            Detail::ClientTrainingSession().Status;

        if (sessionStatus == Core::TrainingSessionStatus::Active ||
            sessionStatus == Core::TrainingSessionStatus::Running)
        {
            return false;
        }

        return Engine::TrainingDebugController::Start(
            Detail::ClientDataset(),
            Detail::ClientNetwork(),
            sampleIndex,
            learningRate,
            lossType,
            optimizerType,
            Detail::ClientTrainingDebugSession(),
            result);
    }

    Core::TrainingDebugSnapshot MiaIAClient::GetTrainingDebug()
    {
        const std::scoped_lock lock(Detail::ClientMutex());
        return Engine::TrainingDebugController::Snapshot(
            Detail::ClientTrainingDebugSession());
    }

    bool MiaIAClient::StartTrainingSessionDebug(
        Core::TrainingDebugSnapshot& result)
    {
        const std::scoped_lock lock(Detail::ClientMutex());
        return Engine::TrainingSessionDebugController::Start(
            Detail::ClientDataset(),
            Detail::ClientNetwork(),
            Detail::ClientTrainingSession(),
            Detail::ClientTrainingDebugSession(),
            result);
    }

    bool MiaIAClient::AdvanceTrainingDebug(
        Core::TrainingDebugSnapshot& result)
    {
        const std::scoped_lock lock(Detail::ClientMutex());
        auto& debugSession = Detail::ClientTrainingDebugSession();

        if (debugSession.AttachedToTrainingSession)
        {
            return Engine::TrainingSessionDebugController::Next(
                Detail::ClientDataset(),
                Detail::ClientNetwork(),
                Detail::ClientTrainingSession(),
                debugSession,
                result);
        }

        return Engine::TrainingDebugController::Next(
            Detail::ClientDataset(),
            Detail::ClientNetwork(),
            debugSession,
            result);
    }

    bool MiaIAClient::CancelTrainingDebug()
    {
        const std::scoped_lock lock(Detail::ClientMutex());
        return Engine::TrainingDebugController::Cancel(
            Detail::ClientTrainingDebugSession());
    }

    bool MiaIAClient::TryGetTrainingDebugNeuron(
        std::uint64_t neuronId,
        Core::TrainingDebugNeuronSnapshot& result)
    {
        const std::scoped_lock lock(Detail::ClientMutex());
        return Engine::TrainingDebugInspector::TryGetNeuron(
            Detail::ClientNetwork(),
            Detail::ClientTrainingDebugSession(),
            neuronId,
            result);
    }

    bool MiaIAClient::TryGetTrainingDebugConnection(
        std::uint64_t connectionId,
        Core::TrainingDebugConnectionSnapshot& result)
    {
        const std::scoped_lock lock(Detail::ClientMutex());
        return Engine::TrainingDebugInspector::TryGetConnection(
            Detail::ClientNetwork(),
            Detail::ClientTrainingDebugSession(),
            connectionId,
            result);
    }
}
