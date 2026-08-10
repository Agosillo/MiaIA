#pragma once

#include "TrainingBreakpointSpec.h"

#include <cstddef>
#include <cstdint>

namespace MiaIA::Core
{
    struct TrainingBreakpointSnapshot
    {
        std::uint64_t Id{};
        bool Enabled{ true };
        TrainingBreakpointSpec Spec;
        std::size_t HitCount{};
    };
}
