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

MiaIA::Studio::StudioForwardTraceState
FMiaIAInstanceService::ForwardTraceState(FMiaIAInstanceHandle Instance)
{
    MiaIA::Studio::StudioController* controller = Resolve(Instance);
    return controller
        ? controller->State().ForwardTrace
        : MiaIA::Studio::StudioForwardTraceState{};
}
