#pragma once

#include "../../Core/Public/TrainingStepComparisonSnapshot.h"
#include "../../Core/Public/TrainingStepSnapshot.h"

#include <cstddef>

namespace MiaIA::Engine
{
    class TrainingStepComparer
    {
    public:
        static bool Compare(
            const Core::TrainingStepSnapshot& first,
            std::size_t firstStepIndex,
            const Core::TrainingStepSnapshot& second,
            std::size_t secondStepIndex,
            Core::TrainingStepComparisonSnapshot& result);
    };
}
