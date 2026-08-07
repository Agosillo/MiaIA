#include "TrainingDebugInspector.h"

#include "../Inspection/NetworkInspector.h"
#include "../../Core/Model/Network.h"
#include "../../Core/Model/TrainingDebugSession.h"

#include <algorithm>

namespace MiaIA::Engine
{
    bool TrainingDebugInspector::TryGetNeuron(
        const Core::Network& publicNetwork,
        const Core::TrainingDebugSession& session,
        std::uint64_t neuronId,
        Core::TrainingDebugNeuronSnapshot& result)
    {
        if (session.Phase == Core::TrainingDebugPhase::Idle)
        {
            return false;
        }

        Core::NeuronSnapshot publicNeuron;
        Core::NeuronSnapshot candidateNeuron;

        if (!NetworkInspector::TryGetNeuron(
            publicNetwork,
            neuronId,
            publicNeuron) ||
            !NetworkInspector::TryGetNeuron(
                session.CandidateNetwork,
                neuronId,
                candidateNeuron))
        {
            return false;
        }

        const auto layer = std::find_if(
            session.CandidateNetwork.Layers.begin(),
            session.CandidateNetwork.Layers.end(),
            [neuronId](const Core::Layer& candidateLayer)
            {
                return std::any_of(
                    candidateLayer.Neurons.begin(),
                    candidateLayer.Neurons.end(),
                    [neuronId](const Core::Neuron& neuron)
                    {
                        return neuron.Id == neuronId;
                    });
            });

        if (layer == session.CandidateNetwork.Layers.end())
        {
            return false;
        }

        Core::TrainingDebugNeuronSnapshot snapshot;
        snapshot.Phase = session.Phase;
        snapshot.Id = neuronId;
        snapshot.LayerOrder = layer->Order;
        snapshot.LayerActivation = layer->Activation;
        snapshot.PublicActivation = publicNeuron.Activation;
        snapshot.CandidateActivation = candidateNeuron.Activation;
        snapshot.PublicBias = publicNeuron.Bias;
        snapshot.CandidateBias = candidateNeuron.Bias;

        if (session.Phase >=
            Core::TrainingDebugPhase::BackwardComplete)
        {
            const auto gradient = std::find_if(
                session.Step.Before.Neurons.begin(),
                session.Step.Before.Neurons.end(),
                [neuronId](const Core::NeuronGradientSnapshot& value)
                {
                    return value.Id == neuronId;
                });

            if (gradient == session.Step.Before.Neurons.end())
            {
                return false;
            }

            snapshot.HasGradients = true;
            snapshot.ActivationGradient =
                gradient->ActivationGradient;
            snapshot.PreActivationGradient =
                gradient->PreActivationGradient;
            snapshot.BiasGradient = gradient->BiasGradient;
        }

        if (session.Phase >= Core::TrainingDebugPhase::UpdateComplete)
        {
            const auto update = std::find_if(
                session.Step.NeuronUpdates.begin(),
                session.Step.NeuronUpdates.end(),
                [neuronId](const Core::NeuronUpdateSnapshot& value)
                {
                    return value.Id == neuronId;
                });

            if (update != session.Step.NeuronUpdates.end())
            {
                snapshot.HasUpdate = true;
                snapshot.PreviousBias = update->PreviousBias;
                snapshot.UpdateGradient = update->Gradient;
                snapshot.Delta = update->Delta;
                snapshot.UpdatedBias = update->UpdatedBias;
            }
            else if (layer->Order != 0)
            {
                return false;
            }
        }

        result = snapshot;
        return true;
    }

    bool TrainingDebugInspector::TryGetConnection(
        const Core::Network& publicNetwork,
        const Core::TrainingDebugSession& session,
        std::uint64_t connectionId,
        Core::TrainingDebugConnectionSnapshot& result)
    {
        if (session.Phase == Core::TrainingDebugPhase::Idle)
        {
            return false;
        }

        Core::ConnectionSnapshot publicConnection;
        Core::ConnectionSnapshot candidateConnection;

        if (!NetworkInspector::TryGetConnection(
            publicNetwork,
            connectionId,
            publicConnection) ||
            !NetworkInspector::TryGetConnection(
                session.CandidateNetwork,
                connectionId,
                candidateConnection))
        {
            return false;
        }

        Core::TrainingDebugConnectionSnapshot snapshot;
        snapshot.Phase = session.Phase;
        snapshot.Id = connectionId;
        snapshot.FromNeuron = candidateConnection.FromNeuron;
        snapshot.ToNeuron = candidateConnection.ToNeuron;
        snapshot.PublicWeight = publicConnection.Weight;
        snapshot.CandidateWeight = candidateConnection.Weight;

        if (session.Phase >=
            Core::TrainingDebugPhase::BackwardComplete)
        {
            const auto gradient = std::find_if(
                session.Step.Before.Connections.begin(),
                session.Step.Before.Connections.end(),
                [connectionId](
                    const Core::ConnectionGradientSnapshot& value)
                {
                    return value.Id == connectionId;
                });

            if (gradient == session.Step.Before.Connections.end())
            {
                return false;
            }

            snapshot.HasGradient = true;
            snapshot.WeightGradient = gradient->WeightGradient;
        }

        if (session.Phase >= Core::TrainingDebugPhase::UpdateComplete)
        {
            const auto update = std::find_if(
                session.Step.ConnectionUpdates.begin(),
                session.Step.ConnectionUpdates.end(),
                [connectionId](
                    const Core::ConnectionUpdateSnapshot& value)
                {
                    return value.Id == connectionId;
                });

            if (update == session.Step.ConnectionUpdates.end())
            {
                return false;
            }

            snapshot.HasUpdate = true;
            snapshot.PreviousWeight = update->PreviousWeight;
            snapshot.UpdateGradient = update->Gradient;
            snapshot.Delta = update->Delta;
            snapshot.UpdatedWeight = update->UpdatedWeight;
        }

        result = snapshot;
        return true;
    }
}
