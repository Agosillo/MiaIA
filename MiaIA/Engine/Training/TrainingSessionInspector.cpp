#include "TrainingSessionInspector.h"

#include "../../Core/Model/TrainingSession.h"
#include "TrainingStepComparer.h"

namespace MiaIA::Engine
{
    std::vector<Core::TrainingHistoryEntrySnapshot>
    TrainingSessionInspector::History(
        const Core::TrainingSession& session)
    {
        std::vector<Core::TrainingHistoryEntrySnapshot> history;
        history.reserve(session.Steps.size());

        for (std::size_t stepIndex = 0;
            stepIndex < session.Steps.size();
            ++stepIndex)
        {
            const Core::TrainingStepSnapshot& step =
                session.Steps[stepIndex];

            Core::TrainingHistoryEntrySnapshot entry;
            entry.StepIndex = stepIndex;
            entry.EpochIndex = session.SampleCount == 0
                ? 0
                : stepIndex / session.SampleCount;
            entry.SampleIndex = step.SampleIndex;
            entry.LossBefore = step.Before.Evaluation.Loss;
            entry.LossAfter = step.After.Loss;
            entry.WeightUpdateCount = step.ConnectionUpdates.size();
            entry.BiasUpdateCount = step.NeuronUpdates.size();
            history.push_back(entry);
        }

        return history;
    }

    bool TrainingSessionInspector::TryGetStep(
        const Core::TrainingSession& session,
        std::size_t stepIndex,
        Core::TrainingStepSnapshot& result)
    {
        if (stepIndex >= session.Steps.size())
        {
            return false;
        }

        result = session.Steps[stepIndex];
        return true;
    }

    bool TrainingSessionInspector::TryCompareSteps(
        const Core::TrainingSession& session,
        std::size_t firstStepIndex,
        std::size_t secondStepIndex,
        Core::TrainingStepComparisonSnapshot& result)
    {
        if (firstStepIndex >= session.Steps.size() ||
            secondStepIndex >= session.Steps.size())
        {
            return false;
        }

        return TrainingStepComparer::Compare(
            session.Steps[firstStepIndex],
            firstStepIndex,
            session.Steps[secondStepIndex],
            secondStepIndex,
            result);
    }
}
