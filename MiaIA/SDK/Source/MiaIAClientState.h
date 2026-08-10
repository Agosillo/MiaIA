#pragma once

#include <mutex>

namespace MiaIA::Core
{
    struct Dataset;
    struct Network;
    struct ProjectInfoSnapshot;
    struct TrainingSession;
    struct TrainingDebugSession;
}

namespace MiaIA::SDK::Detail
{
    Core::Dataset& ClientDataset();
    Core::Network& ClientNetwork();
    Core::ProjectInfoSnapshot& ClientProjectInfo();
    Core::TrainingSession& ClientTrainingSession();
    Core::TrainingDebugSession& ClientTrainingDebugSession();
    std::mutex& ClientMutex();
    bool IsTrainingSessionRunning();
    bool IsTrainingDebugActive();
    bool IsClientMutationBlocked();
}
