#pragma once

#include "../../Core/Public/TrainingBreakpointHitSnapshot.h"
#include "../../Core/Public/TrainingBreakpointSnapshot.h"
#include "../../Core/Public/TrainingBreakpointSpec.h"
#include "../../Core/Public/TrainingDebugSnapshot.h"
#include "../../Core/Public/TrainingStepSnapshot.h"

#include <cstdint>
#include <vector>

namespace MiaIA::Core
{
    struct Network;
    struct TrainingSession;
}

namespace MiaIA::Engine
{
    class TrainingBreakpointController
    {
    public:
        static bool Add(
            Core::TrainingSession& session,
            const Core::TrainingBreakpointSpec& spec,
            Core::TrainingBreakpointSnapshot& result);

        static bool SetEnabled(
            Core::TrainingSession& session,
            std::uint64_t breakpointId,
            bool enabled);

        static bool Remove(
            Core::TrainingSession& session,
            std::uint64_t breakpointId);

        static void Clear(Core::TrainingSession& session);

        [[nodiscard]]
        static std::vector<Core::TrainingBreakpointSnapshot> List(
            const Core::TrainingSession& session);

        static bool TryGetLastHit(
            const Core::TrainingSession& session,
            Core::TrainingBreakpointHitSnapshot& result);

        static bool EvaluateCommittedStep(
            const Core::Network& network,
            const Core::TrainingStepSnapshot& step,
            Core::TrainingSession& session);

        static bool EvaluateDebugPhase(
            const Core::TrainingDebugSnapshot& debug,
            Core::TrainingSession& session);
    };
}
