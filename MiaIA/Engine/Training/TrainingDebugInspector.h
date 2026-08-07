#pragma once

#include "../../Core/Public/TrainingDebugConnectionSnapshot.h"
#include "../../Core/Public/TrainingDebugNeuronSnapshot.h"

#include <cstdint>

namespace MiaIA::Core
{
    struct Network;
    struct TrainingDebugSession;
}

namespace MiaIA::Engine
{
    class TrainingDebugInspector
    {
    public:
        static bool TryGetNeuron(
            const Core::Network& publicNetwork,
            const Core::TrainingDebugSession& session,
            std::uint64_t neuronId,
            Core::TrainingDebugNeuronSnapshot& result);

        static bool TryGetConnection(
            const Core::Network& publicNetwork,
            const Core::TrainingDebugSession& session,
            std::uint64_t connectionId,
            Core::TrainingDebugConnectionSnapshot& result);
    };
}
