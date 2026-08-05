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

            for (const Layer& layer : network.Layers)
            {
                LayerSnapshot layerSnapshot;
                layerSnapshot.Id = layer.Id;
                layerSnapshot.Neurons.reserve(layer.Neurons.size());

                for (const Neuron& neuron : layer.Neurons)
                {
                    layerSnapshot.Neurons.push_back(
                        NeuronSnapshot{
                            neuron.Id,
                            neuron.Activation
                        });
                }

                snapshot.Layers.push_back(std::move(layerSnapshot));
            }

            return snapshot;
        }
    };
}
