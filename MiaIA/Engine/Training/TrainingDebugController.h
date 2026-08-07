#pragma once

#include "../../Core/Public/LossType.h"
#include "../../Core/Public/OptimizerType.h"
#include "../../Core/Public/TrainingDebugSnapshot.h"

#include <cstddef>

namespace MiaIA::Core
{
    struct Dataset;
    struct Network;
    struct TrainingDebugSession;
}

namespace MiaIA::Engine
{
    class TrainingDebugController
    {
    public:
        static bool Start(
            const Core::Dataset& dataset,
            const Core::Network& network,
            std::size_t sampleIndex,
            double learningRate,
            Core::LossType lossType,
            Core::OptimizerType optimizerType,
            Core::TrainingDebugSession& session,
            Core::TrainingDebugSnapshot& result);

        static bool Next(
            const Core::Dataset& dataset,
            Core::Network& network,
            Core::TrainingDebugSession& session,
            Core::TrainingDebugSnapshot& result);

        static Core::TrainingDebugSnapshot Snapshot(
            const Core::TrainingDebugSession& session);

        static bool Cancel(Core::TrainingDebugSession& session);

        static bool RunToCommit(
            const Core::Dataset& dataset,
            Core::Network& network,
            std::size_t sampleIndex,
            double learningRate,
            Core::LossType lossType,
            Core::OptimizerType optimizerType,
            Core::TrainingStepSnapshot& result);
    };
}
