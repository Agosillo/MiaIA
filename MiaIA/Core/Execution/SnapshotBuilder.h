#pragma once

#include "../Model/Network.h"
#include "../Public/NetworkSnapshot.h"

#include <utility>

namespace MiaIA::Core
{
    class SnapshotBuilder
    {
    public:
        [[nodiscard]]
        static NetworkSnapshot Build(const Network& network)
        {
            NetworkSnapshot snapshot;
            snapshot.Layers.reserve(network.Layers.size());
            snapshot.Connections.reserve(network.Connections.size());

            for (const Layer& layer : network.Layers)
            {
                LayerSnapshot layerSnapshot;
                layerSnapshot.Id = layer.Id;
                layerSnapshot.Name = layer.Name;
                layerSnapshot.Activation = layer.Activation;
                layerSnapshot.Neurons.reserve(layer.Neurons.size());

                for (const Neuron& neuron : layer.Neurons)
                {
                    layerSnapshot.Neurons.push_back(
                        NeuronSnapshot{
                            neuron.Id,
                            neuron.Activation,
                            neuron.Bias
                        });
                }

                snapshot.Layers.push_back(std::move(layerSnapshot));
            }

            for (const Connection& connection : network.Connections)
            {
                snapshot.Connections.push_back(
                    ConnectionSnapshot{connection.Id,
                        connection.FromNeuron,
                        connection.ToNeuron,
                        connection.Weight
                    });
            }
            return snapshot;
        }
    };
}
