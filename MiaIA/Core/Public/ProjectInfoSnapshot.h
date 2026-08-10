#pragma once

#include "LossType.h"
#include "OptimizerType.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace MiaIA::Core
{
    inline constexpr std::uint32_t ProjectFormatVersion = 1;

    struct ProjectTrainingConfigurationSnapshot
    {
        bool Available{};
        std::size_t EpochCount{};
        double LearningRate{};
        LossType Loss{ LossType::MeanSquaredError };
        OptimizerType Optimizer{
            OptimizerType::StochasticGradientDescent
        };
    };

    struct ProjectInfoSnapshot
    {
        std::uint32_t FormatVersion{};
        std::string Path;
        bool HasModel{};
        bool HasDatasetReference{};
        bool DatasetLoaded{};
        std::string DatasetSource;
        std::size_t DatasetInputCount{};
        std::size_t DatasetTargetCount{};
        bool DatasetHasHeader{ true };
        ProjectTrainingConfigurationSnapshot Training;
        std::size_t BreakpointCount{};
    };
}
