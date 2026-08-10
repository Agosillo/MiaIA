#include "../Include/MiaIAClient.h"
#include "MiaIAClientState.h"

#include "../../Engine/Training/TrainingBreakpointController.h"
#include "../../Core/Model/TrainingSession.h"

namespace MiaIA::SDK
{
    bool MiaIAClient::AddTrainingBreakpoint(
        const Core::TrainingBreakpointSpec& spec,
        Core::TrainingBreakpointSnapshot& result)
    {
        const std::scoped_lock lock(Detail::ClientMutex());

        if (Detail::IsTrainingDebugActive())
        {
            return false;
        }

        return Engine::TrainingBreakpointController::Add(
            Detail::ClientTrainingSession(),
            spec,
            result);
    }

    std::vector<Core::TrainingBreakpointSnapshot>
    MiaIAClient::GetTrainingBreakpoints()
    {
        const std::scoped_lock lock(Detail::ClientMutex());
        return Engine::TrainingBreakpointController::List(
            Detail::ClientTrainingSession());
    }

    bool MiaIAClient::SetTrainingBreakpointEnabled(
        std::uint64_t breakpointId,
        bool enabled)
    {
        const std::scoped_lock lock(Detail::ClientMutex());

        if (Detail::IsTrainingDebugActive())
        {
            return false;
        }

        return Engine::TrainingBreakpointController::SetEnabled(
            Detail::ClientTrainingSession(),
            breakpointId,
            enabled);
    }

    bool MiaIAClient::RemoveTrainingBreakpoint(
        std::uint64_t breakpointId)
    {
        const std::scoped_lock lock(Detail::ClientMutex());

        if (Detail::IsTrainingDebugActive())
        {
            return false;
        }

        return Engine::TrainingBreakpointController::Remove(
            Detail::ClientTrainingSession(),
            breakpointId);
    }

    bool MiaIAClient::ClearTrainingBreakpoints()
    {
        const std::scoped_lock lock(Detail::ClientMutex());

        if (Detail::IsTrainingDebugActive() ||
            Detail::IsTrainingSessionRunning())
        {
            return false;
        }

        Engine::TrainingBreakpointController::Clear(
            Detail::ClientTrainingSession());
        return true;
    }

    bool MiaIAClient::TryGetLastTrainingBreakpointHit(
        Core::TrainingBreakpointHitSnapshot& result)
    {
        const std::scoped_lock lock(Detail::ClientMutex());
        return Engine::TrainingBreakpointController::TryGetLastHit(
            Detail::ClientTrainingSession(),
            result);
    }
}
