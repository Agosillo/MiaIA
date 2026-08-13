#pragma once

#include "CoreMinimal.h"
#include "StudioController.h"

struct IDE_API FMiaIAInstanceHandle
{
    uint64 Value{};

    bool IsValid() const
    {
        return Value != 0;
    }
};

class IDE_API FMiaIAInstanceService final
{
public:
    static FMiaIAInstanceHandle DefaultInstance();
    static bool IsValid(FMiaIAInstanceHandle Instance);

    static void Refresh(FMiaIAInstanceHandle Instance);
    static bool RunForwardTrace(
        FMiaIAInstanceHandle Instance,
        const TArray<double>& Inputs);
    static void ClearForwardTrace(FMiaIAInstanceHandle Instance);
    static bool FocusForwardTraceNeuron(
        FMiaIAInstanceHandle Instance,
        uint64 NeuronId);
    static bool SetForwardTraceContributionRequest(
        FMiaIAInstanceHandle Instance,
        const MiaIA::Core::ForwardTraceContributionPageRequest& Request);
    static MiaIA::Studio::StudioForwardTraceState ForwardTraceState(
        FMiaIAInstanceHandle Instance);
};
