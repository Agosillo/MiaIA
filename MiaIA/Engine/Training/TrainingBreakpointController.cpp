#include "TrainingBreakpointController.h"

#include "../../Core/Model/Network.h"
#include "../../Core/Model/TrainingSession.h"

#include <algorithm>
#include <cmath>

namespace
{
    using MiaIA::Core::TrainingBreakpointHitSnapshot;
    using MiaIA::Core::TrainingBreakpointKind;
    using MiaIA::Core::TrainingBreakpointSnapshot;
    using MiaIA::Core::TrainingBreakpointSpec;
    using MiaIA::Core::TrainingDebugPhase;

    bool IsValid(const TrainingBreakpointSpec& spec)
    {
        if (!std::isfinite(spec.Threshold))
        {
            return false;
        }

        switch (spec.Kind)
        {
        case TrainingBreakpointKind::Phase:
            return spec.Phase != TrainingDebugPhase::Idle;

        case TrainingBreakpointKind::NeuronActivationAbove:
        case TrainingBreakpointKind::NeuronActivationBelow:
            return spec.TargetId != 0;

        case TrainingBreakpointKind::NeuronGradientMagnitudeAbove:
        case TrainingBreakpointKind::ConnectionUpdateMagnitudeAbove:
            return spec.TargetId != 0 && spec.Threshold >= 0.0;
        }

        return false;
    }

    bool TryGetActivation(
        const MiaIA::Core::Network& network,
        std::uint64_t neuronId,
        double& result)
    {
        for (const auto& layer : network.Layers)
        {
            const auto neuron = std::find_if(
                layer.Neurons.begin(),
                layer.Neurons.end(),
                [neuronId](const MiaIA::Core::Neuron& candidate)
                {
                    return candidate.Id == neuronId;
                });

            if (neuron != layer.Neurons.end())
            {
                result = neuron->Activation;
                return true;
            }
        }

        return false;
    }

    bool TryGetActivation(
        const MiaIA::Core::NetworkSnapshot& network,
        std::uint64_t neuronId,
        double& result)
    {
        for (const auto& layer : network.Layers)
        {
            const auto neuron = std::find_if(
                layer.Neurons.begin(),
                layer.Neurons.end(),
                [neuronId](const MiaIA::Core::NeuronSnapshot& candidate)
                {
                    return candidate.Id == neuronId;
                });

            if (neuron != layer.Neurons.end())
            {
                result = neuron->Activation;
                return true;
            }
        }

        return false;
    }

    bool TryGetNeuronGradient(
        const MiaIA::Core::TrainingStepSnapshot& step,
        std::uint64_t neuronId,
        double& result)
    {
        const auto neuron = std::find_if(
            step.Before.Neurons.begin(),
            step.Before.Neurons.end(),
            [neuronId](const MiaIA::Core::NeuronGradientSnapshot& candidate)
            {
                return candidate.Id == neuronId;
            });

        if (neuron == step.Before.Neurons.end())
        {
            return false;
        }

        result = std::abs(neuron->BiasGradient);
        return true;
    }

    bool TryGetConnectionUpdate(
        const MiaIA::Core::TrainingStepSnapshot& step,
        std::uint64_t connectionId,
        double& result)
    {
        const auto update = std::find_if(
            step.ConnectionUpdates.begin(),
            step.ConnectionUpdates.end(),
            [connectionId](
                const MiaIA::Core::ConnectionUpdateSnapshot& candidate)
            {
                return candidate.Id == connectionId;
            });

        if (update == step.ConnectionUpdates.end())
        {
            return false;
        }

        result = std::abs(update->Delta);
        return true;
    }

    bool MatchesValue(
        const TrainingBreakpointSnapshot& breakpoint,
        double value)
    {
        switch (breakpoint.Spec.Kind)
        {
        case TrainingBreakpointKind::NeuronActivationBelow:
            return value < breakpoint.Spec.Threshold;

        case TrainingBreakpointKind::NeuronActivationAbove:
        case TrainingBreakpointKind::NeuronGradientMagnitudeAbove:
        case TrainingBreakpointKind::ConnectionUpdateMagnitudeAbove:
            return value > breakpoint.Spec.Threshold;

        default:
            return false;
        }
    }

    void RecordHit(
        MiaIA::Core::TrainingSession& session,
        TrainingBreakpointSnapshot& breakpoint,
        TrainingDebugPhase phase,
        double observedValue,
        std::size_t stepIndex,
        std::size_t sampleIndex)
    {
        ++breakpoint.HitCount;
        session.HasBreakpointHit = true;
        session.LastBreakpointHit.BreakpointId = breakpoint.Id;
        session.LastBreakpointHit.Kind = breakpoint.Spec.Kind;
        session.LastBreakpointHit.Phase = phase;
        session.LastBreakpointHit.TargetId = breakpoint.Spec.TargetId;
        session.LastBreakpointHit.ObservedValue = observedValue;
        session.LastBreakpointHit.Threshold = breakpoint.Spec.Threshold;
        session.LastBreakpointHit.StepIndex = stepIndex;
        session.LastBreakpointHit.SampleIndex = sampleIndex;
        if (session.Status == MiaIA::Core::TrainingSessionStatus::Active ||
            session.Status == MiaIA::Core::TrainingSessionStatus::Running)
        {
            session.WorkerStopReason =
                MiaIA::Core::TrainingWorkerStopReason::BreakpointHit;
        }

        if (session.Status == MiaIA::Core::TrainingSessionStatus::Running)
        {
            session.Status = MiaIA::Core::TrainingSessionStatus::Active;
        }
    }
}

namespace MiaIA::Engine
{
    bool TrainingBreakpointController::Add(
        Core::TrainingSession& session,
        const Core::TrainingBreakpointSpec& spec,
        Core::TrainingBreakpointSnapshot& result)
    {
        if (!IsValid(spec) ||
            session.Status == Core::TrainingSessionStatus::Running)
        {
            return false;
        }

        Core::TrainingBreakpointSnapshot breakpoint;
        breakpoint.Id = session.NextBreakpointId++;
        breakpoint.Spec = spec;
        session.Breakpoints.push_back(breakpoint);
        result = breakpoint;
        return true;
    }

    bool TrainingBreakpointController::SetEnabled(
        Core::TrainingSession& session,
        std::uint64_t breakpointId,
        bool enabled)
    {
        if (session.Status == Core::TrainingSessionStatus::Running)
        {
            return false;
        }

        const auto breakpoint = std::find_if(
            session.Breakpoints.begin(),
            session.Breakpoints.end(),
            [breakpointId](const Core::TrainingBreakpointSnapshot& candidate)
            {
                return candidate.Id == breakpointId;
            });

        if (breakpoint == session.Breakpoints.end())
        {
            return false;
        }

        breakpoint->Enabled = enabled;
        return true;
    }

    bool TrainingBreakpointController::Remove(
        Core::TrainingSession& session,
        std::uint64_t breakpointId)
    {
        if (session.Status == Core::TrainingSessionStatus::Running)
        {
            return false;
        }

        const auto breakpoint = std::find_if(
            session.Breakpoints.begin(),
            session.Breakpoints.end(),
            [breakpointId](const Core::TrainingBreakpointSnapshot& candidate)
            {
                return candidate.Id == breakpointId;
            });

        if (breakpoint == session.Breakpoints.end())
        {
            return false;
        }

        session.Breakpoints.erase(breakpoint);
        return true;
    }

    void TrainingBreakpointController::Clear(
        Core::TrainingSession& session)
    {
        if (session.Status == Core::TrainingSessionStatus::Running)
        {
            return;
        }

        session.Breakpoints.clear();
        session.HasBreakpointHit = false;
        session.LastBreakpointHit = {};

        if (session.WorkerStopReason ==
            Core::TrainingWorkerStopReason::BreakpointHit)
        {
            session.WorkerStopReason =
                Core::TrainingWorkerStopReason::None;
        }
    }

    std::vector<Core::TrainingBreakpointSnapshot>
    TrainingBreakpointController::List(
        const Core::TrainingSession& session)
    {
        return session.Breakpoints;
    }

    bool TrainingBreakpointController::TryGetLastHit(
        const Core::TrainingSession& session,
        Core::TrainingBreakpointHitSnapshot& result)
    {
        if (!session.HasBreakpointHit)
        {
            return false;
        }

        result = session.LastBreakpointHit;
        return true;
    }

    bool TrainingBreakpointController::EvaluateCommittedStep(
        const Core::Network& network,
        const Core::TrainingStepSnapshot& step,
        Core::TrainingSession& session)
    {
        for (auto& breakpoint : session.Breakpoints)
        {
            if (!breakpoint.Enabled)
            {
                continue;
            }

            double observedValue{};
            bool matches{};

            switch (breakpoint.Spec.Kind)
            {
            case Core::TrainingBreakpointKind::Phase:
                matches = breakpoint.Spec.Phase ==
                    Core::TrainingDebugPhase::Committed;
                break;

            case Core::TrainingBreakpointKind::NeuronActivationAbove:
            case Core::TrainingBreakpointKind::NeuronActivationBelow:
                matches = TryGetActivation(
                        network,
                        breakpoint.Spec.TargetId,
                        observedValue) &&
                    MatchesValue(breakpoint, observedValue);
                break;

            case Core::TrainingBreakpointKind::NeuronGradientMagnitudeAbove:
                matches = TryGetNeuronGradient(
                        step,
                        breakpoint.Spec.TargetId,
                        observedValue) &&
                    MatchesValue(breakpoint, observedValue);
                break;

            case Core::TrainingBreakpointKind::ConnectionUpdateMagnitudeAbove:
                matches = TryGetConnectionUpdate(
                        step,
                        breakpoint.Spec.TargetId,
                        observedValue) &&
                    MatchesValue(breakpoint, observedValue);
                break;
            }

            if (matches)
            {
                const std::size_t stepIndex = session.Steps.empty()
                    ? 0
                    : session.Steps.size() - 1;
                RecordHit(
                    session,
                    breakpoint,
                    Core::TrainingDebugPhase::Committed,
                    observedValue,
                    stepIndex,
                    step.SampleIndex);
                return true;
            }
        }

        return false;
    }

    bool TrainingBreakpointController::EvaluateDebugPhase(
        const Core::TrainingDebugSnapshot& debug,
        Core::TrainingSession& session)
    {
        for (auto& breakpoint : session.Breakpoints)
        {
            if (!breakpoint.Enabled)
            {
                continue;
            }

            double observedValue{};
            bool matches{};

            switch (breakpoint.Spec.Kind)
            {
            case Core::TrainingBreakpointKind::Phase:
                matches = breakpoint.Spec.Phase == debug.Phase;
                break;

            case Core::TrainingBreakpointKind::NeuronActivationAbove:
            case Core::TrainingBreakpointKind::NeuronActivationBelow:
                matches = debug.Phase ==
                        Core::TrainingDebugPhase::ForwardComplete &&
                    TryGetActivation(
                        debug.CandidateNetwork,
                        breakpoint.Spec.TargetId,
                        observedValue) &&
                    MatchesValue(breakpoint, observedValue);
                break;

            case Core::TrainingBreakpointKind::NeuronGradientMagnitudeAbove:
                matches = debug.Phase ==
                        Core::TrainingDebugPhase::BackwardComplete &&
                    TryGetNeuronGradient(
                        debug.Step,
                        breakpoint.Spec.TargetId,
                        observedValue) &&
                    MatchesValue(breakpoint, observedValue);
                break;

            case Core::TrainingBreakpointKind::ConnectionUpdateMagnitudeAbove:
                matches = debug.Phase ==
                        Core::TrainingDebugPhase::UpdateComplete &&
                    TryGetConnectionUpdate(
                        debug.Step,
                        breakpoint.Spec.TargetId,
                        observedValue) &&
                    MatchesValue(breakpoint, observedValue);
                break;
            }

            if (matches)
            {
                RecordHit(
                    session,
                    breakpoint,
                    debug.Phase,
                    observedValue,
                    session.Steps.size(),
                    debug.SampleIndex);
                return true;
            }
        }

        return false;
    }
}
