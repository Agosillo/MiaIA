#pragma once

#include "LossType.h"
#include "OptimizerType.h"
#include "TrainingSampleOrder.h"
#include "TrainingSessionStatus.h"
#include "TrainingStepSnapshot.h"
#include "TrainingWorkerStopReason.h"
#include "TrainingBreakpointHitSnapshot.h"
#include "TrainingBreakpointSnapshot.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace MiaIA::Core
{
    struct TrainingSessionSnapshot
    {
        TrainingSessionStatus Status{ TrainingSessionStatus::Idle };
        TrainingWorkerStopReason WorkerStopReason{
            TrainingWorkerStopReason::None
        };
        std::size_t EpochCount{};
        std::size_t CurrentEpoch{};
        std::size_t NextSampleIndex{};
        std::size_t NextSamplePosition{};
        std::size_t SampleCount{};
        std::size_t CompletedSteps{};
        std::size_t TotalSteps{};
        double LearningRate{};
        LossType Loss{ LossType::MeanSquaredError };
        OptimizerType Optimizer{
            OptimizerType::StochasticGradientDescent
        };
        TrainingSampleOrder SampleOrder{
            TrainingSampleOrder::Sequential
        };
        std::uint64_t Seed{};
        std::vector<std::size_t> CurrentEpochSampleOrder;
        std::vector<TrainingBreakpointSnapshot> Breakpoints;
        bool HasBreakpointHit{};
        TrainingBreakpointHitSnapshot LastBreakpointHit;
        std::vector<TrainingStepSnapshot> Steps;
    };
}
