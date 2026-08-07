#pragma once

#include "TrainingDebugPhase.h"

#include <cstdint>

namespace MiaIA::Core
{
    struct TrainingDebugConnectionSnapshot
    {
        TrainingDebugPhase Phase{ TrainingDebugPhase::Idle };
        std::uint64_t Id{};
        std::uint64_t FromNeuron{};
        std::uint64_t ToNeuron{};
        double PublicWeight{};
        double CandidateWeight{};
        bool HasGradient{};
        double WeightGradient{};
        bool HasUpdate{};
        double PreviousWeight{};
        double UpdateGradient{};
        double Delta{};
        double UpdatedWeight{};
    };
}
