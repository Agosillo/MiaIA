#pragma once

#include "../Public/LossType.h"
#include "../Public/OptimizerType.h"
#include "../Public/TrainingSessionStatus.h"
#include "../Public/TrainingStepSnapshot.h"
#include "../Public/TrainingBreakpointHitSnapshot.h"
#include "../Public/TrainingBreakpointSnapshot.h"
#include "../Public/TrainingWorkerStopReason.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace MiaIA::Core
{
    struct TrainingSession
    {
        TrainingSessionStatus Status{ TrainingSessionStatus::Idle };
        TrainingWorkerStopReason WorkerStopReason{
            TrainingWorkerStopReason::None
        };
        std::size_t EpochCount{};
        std::size_t CurrentEpoch{};
        std::size_t NextSampleIndex{};
        std::size_t SampleCount{};
        double LearningRate{};
        LossType Loss{ LossType::MeanSquaredError };
        OptimizerType Optimizer{
            OptimizerType::StochasticGradientDescent
        };
        std::vector<TrainingBreakpointSnapshot> Breakpoints;
        std::uint64_t NextBreakpointId{ 1 };
        bool HasBreakpointHit{};
        TrainingBreakpointHitSnapshot LastBreakpointHit;
        std::vector<TrainingStepSnapshot> Steps;
    };
}
