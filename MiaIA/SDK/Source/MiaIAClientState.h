#pragma once

#include <mutex>

namespace MiaIA::Core
{
    struct Dataset;
    struct Network;
    struct TrainingSession;
}

namespace MiaIA::SDK::Detail
{
    Core::Dataset& ClientDataset();
    Core::Network& ClientNetwork();
    Core::TrainingSession& ClientTrainingSession();
    std::mutex& ClientMutex();
    bool IsTrainingSessionRunning();
}
