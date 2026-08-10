#pragma once

#include "TrainingBreakpointKind.h"
#include "TrainingDebugPhase.h"

#include <cstdint>

namespace MiaIA::Core
{
    struct TrainingBreakpointSpec
    {
        TrainingBreakpointKind Kind{ TrainingBreakpointKind::Phase };
        TrainingDebugPhase Phase{ TrainingDebugPhase::BeforeForward };
        std::uint64_t TargetId{};
        double Threshold{};
    };
}
