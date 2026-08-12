#pragma once

#include "TrainingConnectionComparisonSnapshot.h"
#include "TrainingNeuronComparisonSnapshot.h"
#include "TrainingOutputComparisonSnapshot.h"
#include "TrainingValueComparisonSnapshot.h"

#include <cstddef>
#include <vector>

namespace MiaIA::Core
{
    struct TrainingStepComparisonSnapshot
    {
        std::size_t FirstStepIndex{};
        std::size_t SecondStepIndex{};
        std::size_t FirstSampleIndex{};
        std::size_t SecondSampleIndex{};
        bool SameSample{};
        TrainingValueComparisonSnapshot LossBefore;
        TrainingValueComparisonSnapshot LossAfter;
        std::vector<TrainingOutputComparisonSnapshot> Outputs;
        std::vector<TrainingNeuronComparisonSnapshot> Neurons;
        std::vector<TrainingConnectionComparisonSnapshot> Connections;
    };
}
