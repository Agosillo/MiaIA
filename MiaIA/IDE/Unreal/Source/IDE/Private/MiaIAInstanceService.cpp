#include "MiaIAInstanceService.h"

#include "Misc/ScopeLock.h"

#include <vector>

namespace
{
    constexpr uint64 DefaultInstanceValue = 1;

    class FMiaIAInstanceRegistry final
    {
    public:
        MiaIA::Studio::StudioController* Resolve(
            FMiaIAInstanceHandle instance)
        {
            if (!instance.IsValid())
            {
                return nullptr;
            }

            FScopeLock lock(&Mutex);
            TUniquePtr<MiaIA::Studio::StudioController>* controller =
                Controllers.Find(instance.Value);
            return controller ? controller->Get() : nullptr;
        }

        FMiaIAInstanceHandle DefaultInstance()
        {
            FScopeLock lock(&Mutex);

            if (!Controllers.Contains(DefaultInstanceValue))
            {
                Controllers.Add(
                    DefaultInstanceValue,
                    MakeUnique<MiaIA::Studio::StudioController>());
            }

            return {DefaultInstanceValue};
        }

    private:
        FCriticalSection Mutex;
        TMap<uint64, TUniquePtr<MiaIA::Studio::StudioController>> Controllers;
    };

    FMiaIAInstanceRegistry& InstanceRegistry()
    {
        static FMiaIAInstanceRegistry registry;
        return registry;
    }

    MiaIA::Studio::StudioController* Resolve(
        FMiaIAInstanceHandle instance)
    {
        return InstanceRegistry().Resolve(instance);
    }
}

FMiaIAInstanceHandle FMiaIAInstanceService::DefaultInstance()
{
    return InstanceRegistry().DefaultInstance();
}

bool FMiaIAInstanceService::IsValid(FMiaIAInstanceHandle Instance)
{
    return Resolve(Instance) != nullptr;
}

void FMiaIAInstanceService::Refresh(FMiaIAInstanceHandle Instance)
{
    if (MiaIA::Studio::StudioController* controller = Resolve(Instance))
    {
        controller->Refresh();
    }
}

bool FMiaIAInstanceService::RunForwardTrace(
    FMiaIAInstanceHandle Instance,
    const TArray<double>& Inputs)
{
    MiaIA::Studio::StudioController* controller = Resolve(Instance);
    if (!controller)
    {
        return false;
    }

    std::vector<double> nativeInputs;
    nativeInputs.reserve(Inputs.Num());

    for (const double input : Inputs)
    {
        nativeInputs.push_back(input);
    }

    return controller->RunForwardTrace(nativeInputs);
}

void FMiaIAInstanceService::ClearForwardTrace(
    FMiaIAInstanceHandle Instance)
{
    if (MiaIA::Studio::StudioController* controller = Resolve(Instance))
    {
        controller->ClearForwardTrace();
    }
}

bool FMiaIAInstanceService::FocusForwardTraceNeuron(
    FMiaIAInstanceHandle Instance,
    uint64 NeuronId)
{
    MiaIA::Studio::StudioController* controller = Resolve(Instance);
    return controller && controller->FocusForwardTraceNeuron(NeuronId);
}

bool FMiaIAInstanceService::SetForwardTraceContributionRequest(
    FMiaIAInstanceHandle Instance,
    const MiaIA::Core::ForwardTraceContributionPageRequest& Request)
{
    MiaIA::Studio::StudioController* controller = Resolve(Instance);
    return controller &&
        controller->SetForwardTraceContributionRequest(Request);
}

bool FMiaIAInstanceService::PlayForwardTrace(
    FMiaIAInstanceHandle Instance)
{
    MiaIA::Studio::StudioController* controller = Resolve(Instance);
    return controller && controller->PlayForwardTrace();
}

bool FMiaIAInstanceService::PauseForwardTrace(
    FMiaIAInstanceHandle Instance)
{
    MiaIA::Studio::StudioController* controller = Resolve(Instance);
    return controller && controller->PauseForwardTrace();
}

bool FMiaIAInstanceService::RestartForwardTrace(
    FMiaIAInstanceHandle Instance)
{
    MiaIA::Studio::StudioController* controller = Resolve(Instance);
    return controller && controller->RestartForwardTrace();
}

bool FMiaIAInstanceService::StepForwardTraceForward(
    FMiaIAInstanceHandle Instance)
{
    MiaIA::Studio::StudioController* controller = Resolve(Instance);
    return controller && controller->StepForwardTraceForward();
}

bool FMiaIAInstanceService::StepForwardTraceBackward(
    FMiaIAInstanceHandle Instance)
{
    MiaIA::Studio::StudioController* controller = Resolve(Instance);
    return controller && controller->StepForwardTraceBackward();
}

bool FMiaIAInstanceService::AdvanceForwardTracePlayback(
    FMiaIAInstanceHandle Instance,
    double ElapsedSeconds)
{
    MiaIA::Studio::StudioController* controller = Resolve(Instance);
    return controller &&
        controller->AdvanceForwardTracePlayback(ElapsedSeconds);
}

bool FMiaIAInstanceService::SetForwardTraceFrameDuration(
    FMiaIAInstanceHandle Instance,
    double DurationSeconds)
{
    MiaIA::Studio::StudioController* controller = Resolve(Instance);
    return controller &&
        controller->SetForwardTraceFrameDuration(DurationSeconds);
}

bool FMiaIAInstanceService::RunBackwardTrace(
    FMiaIAInstanceHandle Instance,
    const TArray<double>& Inputs,
    const TArray<double>& Targets)
{
    MiaIA::Studio::StudioController* controller = Resolve(Instance);

    if (controller == nullptr)
    {
        return false;
    }

    std::vector<double> nativeInputs;
    nativeInputs.reserve(Inputs.Num());
    for (const double input : Inputs)
    {
        nativeInputs.push_back(input);
    }

    std::vector<double> nativeTargets;
    nativeTargets.reserve(Targets.Num());
    for (const double target : Targets)
    {
        nativeTargets.push_back(target);
    }

    return controller->RunBackwardTrace(nativeInputs, nativeTargets);
}

void FMiaIAInstanceService::ClearBackwardTrace(
    FMiaIAInstanceHandle Instance)
{
    if (MiaIA::Studio::StudioController* controller = Resolve(Instance))
    {
        controller->ClearBackwardTrace();
    }
}

bool FMiaIAInstanceService::FocusBackwardTraceNeuron(
    FMiaIAInstanceHandle Instance,
    uint64 NeuronId)
{
    MiaIA::Studio::StudioController* controller = Resolve(Instance);
    return controller && controller->FocusBackwardTraceNeuron(NeuronId);
}

bool FMiaIAInstanceService::PlayBackwardTrace(
    FMiaIAInstanceHandle Instance)
{
    MiaIA::Studio::StudioController* controller = Resolve(Instance);
    return controller && controller->PlayBackwardTrace();
}

bool FMiaIAInstanceService::PauseBackwardTrace(
    FMiaIAInstanceHandle Instance)
{
    MiaIA::Studio::StudioController* controller = Resolve(Instance);
    return controller && controller->PauseBackwardTrace();
}

bool FMiaIAInstanceService::RestartBackwardTrace(
    FMiaIAInstanceHandle Instance)
{
    MiaIA::Studio::StudioController* controller = Resolve(Instance);
    return controller && controller->RestartBackwardTrace();
}

bool FMiaIAInstanceService::StepBackwardTraceForward(
    FMiaIAInstanceHandle Instance)
{
    MiaIA::Studio::StudioController* controller = Resolve(Instance);
    return controller && controller->StepBackwardTraceForward();
}

bool FMiaIAInstanceService::StepBackwardTraceBackward(
    FMiaIAInstanceHandle Instance)
{
    MiaIA::Studio::StudioController* controller = Resolve(Instance);
    return controller && controller->StepBackwardTraceBackward();
}

bool FMiaIAInstanceService::AdvanceBackwardTracePlayback(
    FMiaIAInstanceHandle Instance,
    double ElapsedSeconds)
{
    MiaIA::Studio::StudioController* controller = Resolve(Instance);
    return controller &&
        controller->AdvanceBackwardTracePlayback(ElapsedSeconds);
}

bool FMiaIAInstanceService::SetBackwardTraceFrameDuration(
    FMiaIAInstanceHandle Instance,
    double DurationSeconds)
{
    MiaIA::Studio::StudioController* controller = Resolve(Instance);
    return controller &&
        controller->SetBackwardTraceFrameDuration(DurationSeconds);
}

bool FMiaIAInstanceService::RunSignalHealthDiagnostics(
    FMiaIAInstanceHandle Instance,
    const MiaIA::Core::SignalHealthConfiguration& Configuration)
{
    MiaIA::Studio::StudioController* controller = Resolve(Instance);
    return controller &&
        controller->RunSignalHealthDiagnostics(Configuration);
}

void FMiaIAInstanceService::ClearSignalHealthDiagnostics(
    FMiaIAInstanceHandle Instance)
{
    if (MiaIA::Studio::StudioController* controller = Resolve(Instance))
    {
        controller->ClearSignalHealthDiagnostics();
    }
}

void FMiaIAInstanceService::SetSignalHealthFilter(
    FMiaIAInstanceHandle Instance,
    MiaIA::Studio::StudioSignalHealthFilter Filter)
{
    if (MiaIA::Studio::StudioController* controller = Resolve(Instance))
    {
        controller->SetSignalHealthFilter(Filter);
    }
}

void FMiaIAInstanceService::RefreshTrainingTimeline(
    FMiaIAInstanceHandle Instance)
{
    if (MiaIA::Studio::StudioController* controller = Resolve(Instance))
    {
        controller->RefreshTrainingTimeline();
    }
}

bool FMiaIAInstanceService::SelectTrainingTimelineStep(
    FMiaIAInstanceHandle Instance,
    uint64 StepIndex)
{
    MiaIA::Studio::StudioController* controller = Resolve(Instance);
    return controller && controller->SelectTrainingTimelineStep(
        static_cast<std::size_t>(StepIndex));
}

void FMiaIAInstanceService::ClearTrainingTimelineSelection(
    FMiaIAInstanceHandle Instance)
{
    if (MiaIA::Studio::StudioController* controller = Resolve(Instance))
    {
        controller->ClearTrainingTimelineSelection();
    }
}

MiaIA::Studio::StudioForwardTraceState
FMiaIAInstanceService::ForwardTraceState(FMiaIAInstanceHandle Instance)
{
    MiaIA::Studio::StudioController* controller = Resolve(Instance);
    return controller
        ? controller->State().ForwardTrace
        : MiaIA::Studio::StudioForwardTraceState{};
}

MiaIA::Studio::StudioBackwardTraceState
FMiaIAInstanceService::BackwardTraceState(FMiaIAInstanceHandle Instance)
{
    const MiaIA::Studio::StudioController* controller = Resolve(Instance);
    return controller
        ? controller->State().BackwardTrace
        : MiaIA::Studio::StudioBackwardTraceState{};
}

MiaIA::Studio::StudioSignalHealthState
FMiaIAInstanceService::SignalHealthState(FMiaIAInstanceHandle Instance)
{
    const MiaIA::Studio::StudioController* controller = Resolve(Instance);
    return controller
        ? controller->State().SignalHealth
        : MiaIA::Studio::StudioSignalHealthState{};
}

MiaIA::Studio::StudioTrainingTimelineState
FMiaIAInstanceService::TrainingTimelineState(
    FMiaIAInstanceHandle Instance)
{
    MiaIA::Studio::StudioController* controller = Resolve(Instance);
    return controller
        ? controller->State().TrainingTimeline
        : MiaIA::Studio::StudioTrainingTimelineState{};
}
