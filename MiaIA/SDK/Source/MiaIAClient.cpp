#include "../Include/MiaIAClient.h"
#include "../../Core/Execution/SnapshotBuilder.h"
#include "../../Core/Model/Network.h"

namespace MiaIA::SDK
{
    int MiaIAClient::TestConnection()
    {
        return 1001;
    }

    /*
    Core::NetworkSnapshot MiaIAClient::CreateDemoSnapshot()
    {
        Core::Network network;

        Core::Layer layer;
        layer.Id = 1;
        layer.Name = "Input";

        Core::Neuron neuron;
        neuron.Id = 1001;
        neuron.Bias = 0.25;
        neuron.Activation = 0.75;

        layer.Neurons.push_back(neuron);
        network.Layers.push_back(layer);

        return Core::SnapshotBuilder::Build(network);
    }
    */
}