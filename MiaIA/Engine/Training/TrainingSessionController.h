#pragma once

#include "../../Core/Public/LossType.h"
#include "../../Core/Public/OptimizerType.h"
#include "../../Core/Public/TrainingSessionSnapshot.h"
#include "../../Core/Public/TrainingStepSnapshot.h"

#include <cstddef>

namespace MiaIA::Core
{
    struct Dataset;
    struct Network;
    struct TrainingSession;
}

namespace MiaIA::Engine
{
    class TrainingSessionController
    {
    public:
        static bool Start(
            const Core::Dataset& dataset,
            const Core::Network& network,
            std::size_t epochCount,
            double learningRate,
            Core::LossType lossType,
            Core::OptimizerType optimizerType,
            Core::TrainingSession& session,
            Core::TrainingSessionSnapshot& result);

        static bool Next(
            const Core::Dataset& dataset,
            Core::Network& network,
            Core::TrainingSession& session,
            Core::TrainingStepSnapshot& result);

        static bool Cancel(Core::TrainingSession& session);

        [[nodiscard]]
        static Core::TrainingSessionSnapshot Snapshot(
            const Core::TrainingSession& session);
    };
}
