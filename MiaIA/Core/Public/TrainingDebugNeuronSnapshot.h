#pragma once

#include "TrainingDebugPhase.h"
#include "../Model/Layer.h"

#include <cstdint>

namespace MiaIA::Core
{
    struct TrainingDebugNeuronSnapshot
    {
        TrainingDebugPhase Phase{ TrainingDebugPhase::Idle };
        std::uint64_t Id{};
        std::uint64_t LayerOrder{};
        ActivationType LayerActivation{ ActivationType::Sigmoid };
        double PublicActivation{};
        double CandidateActivation{};
        double PublicBias{};
        double CandidateBias{};
        bool HasGradients{};
        double ActivationGradient{};
        double PreActivationGradient{};
        double BiasGradient{};
        bool HasUpdate{};
        double PreviousBias{};
        double UpdateGradient{};
        double Delta{};
        double UpdatedBias{};
    };
}
