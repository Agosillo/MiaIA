#pragma once

#include "TrainingSessionStatus.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace MiaIA::Core
{
    struct ModelInstanceSnapshot
    {
        std::uint64_t Id{};
        std::string Name;
        bool Active{};
        std::size_t LayerCount{};
        std::size_t NeuronCount{};
        std::size_t ConnectionCount{};
        std::size_t DatasetSampleCount{};
        TrainingSessionStatus TrainingStatus{
            TrainingSessionStatus::Idle
        };
        std::size_t CheckpointCount{};
    };
}
