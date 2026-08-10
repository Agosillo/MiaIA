#pragma once

#include "TrainingBreakpointKind.h"
#include "TrainingDebugPhase.h"

#include <cstddef>
#include <cstdint>

namespace MiaIA::Core
{
    struct TrainingBreakpointHitSnapshot
    {
        std::uint64_t BreakpointId{};
        TrainingBreakpointKind Kind{ TrainingBreakpointKind::Phase };
        TrainingDebugPhase Phase{ TrainingDebugPhase::Idle };
        std::uint64_t TargetId{};
        double ObservedValue{};
        double Threshold{};
        std::size_t StepIndex{};
        std::size_t SampleIndex{};
    };
}
