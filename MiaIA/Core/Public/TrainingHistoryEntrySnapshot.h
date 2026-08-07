#pragma once

#include <cstddef>

namespace MiaIA::Core
{
    struct TrainingHistoryEntrySnapshot
    {
        std::size_t StepIndex{};
        std::size_t EpochIndex{};
        std::size_t SampleIndex{};
        double LossBefore{};
        double LossAfter{};
        std::size_t WeightUpdateCount{};
        std::size_t BiasUpdateCount{};
    };
}
