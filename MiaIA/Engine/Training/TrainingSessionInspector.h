#pragma once

#include "../../Core/Public/TrainingHistoryEntrySnapshot.h"
#include "../../Core/Public/TrainingStepComparisonSnapshot.h"
#include "../../Core/Public/TrainingStepSnapshot.h"

#include <cstddef>
#include <vector>

namespace MiaIA::Core
{
    struct TrainingSession;
}

namespace MiaIA::Engine
{
    class TrainingSessionInspector
    {
    public:
        [[nodiscard]]
        static std::vector<Core::TrainingHistoryEntrySnapshot> History(
            const Core::TrainingSession& session);

        static bool TryGetStep(
            const Core::TrainingSession& session,
            std::size_t stepIndex,
            Core::TrainingStepSnapshot& result);

        static bool TryCompareSteps(
            const Core::TrainingSession& session,
            std::size_t firstStepIndex,
            std::size_t secondStepIndex,
            Core::TrainingStepComparisonSnapshot& result);
    };
}
