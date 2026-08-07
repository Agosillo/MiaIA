#include "TrainingSessionDebugController.h"

#include "TrainingDebugController.h"
#include "TrainingSessionController.h"
#include "../../Core/Model/TrainingDebugSession.h"
#include "../../Core/Model/TrainingSession.h"

#include <utility>

namespace MiaIA::Engine
{
    bool TrainingSessionDebugController::Start(
        const Core::Dataset& dataset,
        const Core::Network& network,
        Core::TrainingSession& trainingSession,
        Core::TrainingDebugSession& debugSession,
        Core::TrainingDebugSnapshot& result)
    {
        if (trainingSession.Status !=
                Core::TrainingSessionStatus::Active ||
            !TrainingSessionController::CanRecordStep(
                trainingSession,
                trainingSession.NextSampleIndex))
        {
            return false;
        }

        Core::TrainingDebugSnapshot snapshot;

        if (!TrainingDebugController::Start(
            dataset,
            network,
            trainingSession.NextSampleIndex,
            trainingSession.LearningRate,
            trainingSession.Loss,
            trainingSession.Optimizer,
            debugSession,
            snapshot))
        {
            return false;
        }

        debugSession.AttachedToTrainingSession = true;
        result = std::move(snapshot);
        return true;
    }

    bool TrainingSessionDebugController::Next(
        const Core::Dataset& dataset,
        Core::Network& network,
        Core::TrainingSession& trainingSession,
        Core::TrainingDebugSession& debugSession,
        Core::TrainingDebugSnapshot& result)
    {
        if (!debugSession.AttachedToTrainingSession ||
            trainingSession.Status !=
                Core::TrainingSessionStatus::Active)
        {
            return false;
        }

        if (debugSession.Phase == Core::TrainingDebugPhase::Verified &&
            !TrainingSessionController::CanRecordStep(
                trainingSession,
                debugSession.Step.SampleIndex))
        {
            return false;
        }

        Core::TrainingDebugSnapshot snapshot;

        if (!TrainingDebugController::Next(
            dataset,
            network,
            debugSession,
            snapshot))
        {
            return false;
        }

        if (debugSession.Phase == Core::TrainingDebugPhase::Committed &&
            !TrainingSessionController::RecordStep(
                trainingSession,
                debugSession.Step))
        {
            return false;
        }

        result = std::move(snapshot);
        return true;
    }
}
