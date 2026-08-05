#include "../Core/Execution/SnapshotBuilder.h"

#include <cassert>
#include <iostream>

int main()
{
    MiaIA::Core::Network network;

    MiaIA::Core::Layer layer;
    layer.Id = 1;
    layer.Name = "Input";

    MiaIA::Core::Neuron neuron;
    neuron.Id = 1001;
    neuron.Bias = 0.25;
    neuron.Activation = 0.75;

    layer.Neurons.push_back(neuron);
    network.Layers.push_back(layer);

    const MiaIA::Core::NetworkSnapshot snapshot =
        MiaIA::Core::SnapshotBuilder::Build(network);

    assert(snapshot.Layers.size() == 1);
    assert(snapshot.Layers[0].Id == 1);
    assert(snapshot.Layers[0].Neurons.size() == 1);
    assert(snapshot.Layers[0].Neurons[0].Id == 1001);
    assert(snapshot.Layers[0].Neurons[0].Activation == 0.75);

    std::cout << "Snapshot test passed\n";

    return 0;
}