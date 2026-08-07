#include "../Include/MiaIAClient.h"
#include "MiaIAClientState.h"

#include "../../Engine/Training/TrainingSessionController.h"
#include "../../Core/Model/Dataset.h"
#include "../../Core/Model/Network.h"
#include "../../Core/Model/TrainingSession.h"

#include <mutex>
#include <thread>

namespace
{
    std::mutex& WorkerControlMutex()
    {
        static std::mutex mutex;
        return mutex;
    }

    std::jthread& TrainingWorker()
    {
        static std::jthread worker;
        return worker;
    }

    void ExecuteTrainingWorker(std::stop_token stopToken)
    {
        while (!stopToken.stop_requested())
        {
            {
                const std::scoped_lock lock(
                    MiaIA::SDK::Detail::ClientMutex());

                auto& session =
                    MiaIA::SDK::Detail::ClientTrainingSession();

                if (session.Status !=
                    MiaIA::Core::TrainingSessionStatus::Running)
                {
                    return;
                }

                MiaIA::Core::TrainingStepSnapshot step;

                if (!MiaIA::Engine::TrainingSessionController::Next(
                    MiaIA::SDK::Detail::ClientDataset(),
                    MiaIA::SDK::Detail::ClientNetwork(),
                    session,
                    step))
                {
                    session.Status =
                        MiaIA::Core::TrainingSessionStatus::Active;
                    session.WorkerStopReason =
                        MiaIA::Core::TrainingWorkerStopReason::StepFailed;
                    return;
                }

                if (session.Status ==
                    MiaIA::Core::TrainingSessionStatus::Completed)
                {
                    return;
                }
            }

            std::this_thread::yield();
        }

        const std::scoped_lock lock(
            MiaIA::SDK::Detail::ClientMutex());

        auto& session = MiaIA::SDK::Detail::ClientTrainingSession();

        if (session.Status ==
            MiaIA::Core::TrainingSessionStatus::Running)
        {
            session.Status = MiaIA::Core::TrainingSessionStatus::Active;
            session.WorkerStopReason =
                MiaIA::Core::TrainingWorkerStopReason::PauseRequested;
        }
    }
}

namespace MiaIA::SDK
{
    bool MiaIAClient::ResumeTrainingSession()
    {
        const std::scoped_lock workerLock(WorkerControlMutex());
        auto& worker = TrainingWorker();

        {
            const std::scoped_lock clientLock(Detail::ClientMutex());

            if (Detail::ClientTrainingSession().Status !=
                Core::TrainingSessionStatus::Active)
            {
                return false;
            }
        }

        if (worker.joinable())
        {
            worker.join();
        }

        {
            const std::scoped_lock clientLock(Detail::ClientMutex());
            auto& session = Detail::ClientTrainingSession();
            session.Status = Core::TrainingSessionStatus::Running;
            session.WorkerStopReason =
                Core::TrainingWorkerStopReason::None;
        }

        worker = std::jthread(ExecuteTrainingWorker);
        return true;
    }

    bool MiaIAClient::PauseTrainingSession()
    {
        const std::scoped_lock workerLock(WorkerControlMutex());
        auto& worker = TrainingWorker();

        {
            const std::scoped_lock clientLock(Detail::ClientMutex());

            if (Detail::ClientTrainingSession().Status !=
                Core::TrainingSessionStatus::Running ||
                !worker.joinable())
            {
                return false;
            }
        }

        worker.request_stop();
        worker.join();

        const std::scoped_lock clientLock(Detail::ClientMutex());
        return Detail::ClientTrainingSession().Status ==
            Core::TrainingSessionStatus::Active;
    }

    bool MiaIAClient::CancelTrainingSession()
    {
        const std::scoped_lock workerLock(WorkerControlMutex());
        auto& worker = TrainingWorker();

        {
            const std::scoped_lock clientLock(Detail::ClientMutex());

            if (Detail::ClientTrainingSession().Status ==
                Core::TrainingSessionStatus::Running &&
                worker.joinable())
            {
                worker.request_stop();
            }
        }

        if (worker.joinable())
        {
            worker.join();
        }

        const std::scoped_lock clientLock(Detail::ClientMutex());
        auto& session = Detail::ClientTrainingSession();

        if (session.Status != Core::TrainingSessionStatus::Active)
        {
            return false;
        }

        session.WorkerStopReason =
            Core::TrainingWorkerStopReason::CancelRequested;
        return Engine::TrainingSessionController::Cancel(
            session);
    }
}
