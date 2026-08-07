#pragma once

#include "../../Core/Public/TrainingDebugSnapshot.h"

namespace MiaIA::Core
{
    struct Dataset;
    struct Network;
    struct TrainingDebugSession;
    struct TrainingSession;
}

namespace MiaIA::Engine
{
    class TrainingSessionDebugController
    {
    public:
        static bool Start(
            const Core::Dataset& dataset,
            const Core::Network& network,
            Core::TrainingSession& trainingSession,
            Core::TrainingDebugSession& debugSession,
            Core::TrainingDebugSnapshot& result);

        static bool Next(
            const Core::Dataset& dataset,
            Core::Network& network,
            Core::TrainingSession& trainingSession,
            Core::TrainingDebugSession& debugSession,
            Core::TrainingDebugSnapshot& result);
    };
}
