#include <cassert>
#include <cmath>
#include "../SDK/Include/MiaIAClient.h"
#include "../Core/Execution/Activation.h"

int main()
{
    using MiaIA::SDK::MiaIAClient;

    MiaIAClient::ClearNetwork();

    assert(MiaIAClient::AddLayer(0, "Input"));
    assert(!MiaIAClient::AddLayer(0, "Duplicate"));

    assert(MiaIAClient::AddNeuron(0, 1001, 0.75, 0.25));
    assert(!MiaIAClient::AddNeuron(0, 1001, 0.10, 0.10));

    assert(MiaIAClient::AddLayer(1, "Hidden"));
    assert(MiaIAClient::AddNeuron(1, 2001, 0.60, 0.30));

    assert(MiaIAClient::AddConnection(1, 1001, 2001, 0.8));
    assert(!MiaIAClient::AddConnection(1, 1001, 2001, 0.8));
    assert(!MiaIAClient::AddConnection(2, 1001, 9999, 0.5));
    
    assert(MiaIAClient::SetNeuronActivation(1001, 0.95));
    assert(MiaIAClient::SetNeuronBias(1001, 0.85));
    assert(MiaIAClient::SetConnectionWeight(1, 0.65));

    const auto snapshot = MiaIAClient::GetSnapshot();

    assert(snapshot.Layers[0].Neurons[0].Activation == 0.95);
    assert(snapshot.Layers[0].Neurons[0].Bias == 0.85);
    assert(snapshot.Connections[0].Weight == 0.65);


    assert(snapshot.Layers.size() == 2);
    assert(snapshot.Connections.size() == 1);
    assert(snapshot.Connections[0].Id == 1);
    assert(snapshot.Connections[0].FromNeuron == 1001);
    assert(snapshot.Connections[0].ToNeuron == 2001);
    assert(snapshot.Connections[0].Weight == 0.65);

    assert(MiaIAClient::RemoveNeuron(1001));

    const auto afterNeuronRemoval = MiaIAClient::GetSnapshot();

    assert(afterNeuronRemoval.Layers[0].Neurons.empty());
    assert(afterNeuronRemoval.Connections.empty());

    MiaIAClient::ClearNetwork();

    assert(MiaIAClient::AddLayer(0, "Input"));
    assert(MiaIAClient::AddLayer(1, "Hidden"));

    assert(MiaIAClient::AddNeuron(0, 1001, 0.75, 0.25));
    assert(MiaIAClient::AddNeuron(1, 2001, 0.60, 0.30));

    assert(MiaIAClient::AddConnection(1, 1001, 2001, 0.8));

    assert(MiaIAClient::RemoveLayer(0));

    const auto afterLayerRemoval = MiaIAClient::GetSnapshot();

    assert(afterLayerRemoval.Layers.size() == 1);
    assert(afterLayerRemoval.Layers[0].Id == 1);
    assert(afterLayerRemoval.Connections.empty());

    MiaIAClient::ClearNetwork();

    assert(MiaIAClient::AddLayer(0, "Input"));
    assert(MiaIAClient::AddLayer(1, "Hidden"));
    assert(MiaIAClient::AddNeuron(0, 1001, 0.75, 0.25));
    assert(MiaIAClient::AddNeuron(1, 2001, 0.60, 0.30));
    assert(MiaIAClient::AddConnection(1, 1001, 2001, 0.8));

    MiaIA::Core::NeuronSnapshot neuron;
    MiaIA::Core::ConnectionSnapshot connection;
    MiaIA::Core::LayerSnapshot layer;

    assert(MiaIAClient::TryGetNeuron(1001, neuron));
    assert(neuron.Id == 1001);

    assert(MiaIAClient::TryGetConnection(1, connection));
    assert(connection.Weight == 0.8);

    assert(MiaIAClient::TryGetLayer(0, layer));
    assert(layer.Name == "Input");

    assert(!MiaIAClient::TryGetNeuron(9999, neuron));
    assert(!MiaIAClient::TryGetConnection(9999, connection));
    assert(!MiaIAClient::TryGetLayer(9999, layer));

    assert(MiaIAClient::RemoveConnection(1));
    assert(!MiaIAClient::RemoveConnection(1));

    const auto afterConnectionRemoval = MiaIAClient::GetSnapshot();

    assert(afterConnectionRemoval.Connections.empty());


    MiaIAClient::ClearNetwork();

    assert(MiaIAClient::AddLayer(0, "Input"));
    assert(MiaIAClient::AddLayer(1, "Output"));

    assert(MiaIAClient::AddNeuron(0, 1001, 0.0, 1.0));
    assert(MiaIAClient::AddNeuron(1, 2001, 0.0, 0.0));

    assert(MiaIAClient::AddConnection(1, 1001, 2001, 1.0));

    assert(MiaIAClient::Forward());

    const auto forwardSnapshot = MiaIAClient::GetSnapshot();

    const double expected = MiaIA::Core::Activation::Sigmoid(1.0);

    assert(std::abs(forwardSnapshot.Layers[1].Neurons[0].Activation - expected) < 0.000001);

    MiaIAClient::ClearNetwork();

    assert(MiaIAClient::AddLayer(0, "Input"));
    assert(MiaIAClient::AddLayer(1, "Output"));

    assert(MiaIAClient::AddNeuron(0, 1001, 0.0, 1.0));
    assert(MiaIAClient::AddNeuron(0, 1002, 0.0, 0.5));
    assert(MiaIAClient::AddNeuron(1, 2001, 0.2, 0.0));

    assert(MiaIAClient::AddConnection(1, 1001, 2001, 0.8));
    assert(MiaIAClient::AddConnection(2, 1002, 2001, 0.4));

    assert(MiaIAClient::Forward());

    const auto multiInputSnapshot = MiaIAClient::GetSnapshot();

    const double multiInputExpected =
        MiaIA::Core::Activation::Sigmoid(
            0.2 + (1.0 * 0.8) + (0.5 * 0.4));

    assert(std::abs(
        multiInputSnapshot.Layers[1].Neurons[0].Activation -
        multiInputExpected
    ) < 0.000001);


    MiaIAClient::ClearNetwork();

    assert(MiaIAClient::AddLayer(0, "Input"));
    assert(MiaIAClient::AddLayer(1, "Hidden"));
    assert(MiaIAClient::AddLayer(2, "Output"));

    assert(MiaIAClient::AddNeuron(0, 1001, 0.0, 1.0));
    assert(MiaIAClient::AddNeuron(1, 2001, 0.0, 0.0));
    assert(MiaIAClient::AddNeuron(2, 3001, 0.0, 0.0));

    assert(MiaIAClient::AddConnection(1, 1001, 2001, 1.0));
    assert(MiaIAClient::AddConnection(2, 2001, 3001, 1.0));

    assert(MiaIAClient::Forward());

    const auto threeLayerSnapshot = MiaIAClient::GetSnapshot();

    const double hiddenExpected =
        MiaIA::Core::Activation::Sigmoid(1.0);

    const double outputExpected =
        MiaIA::Core::Activation::Sigmoid(hiddenExpected);

    assert(std::abs(
        threeLayerSnapshot.Layers[1].Neurons[0].Activation -
        hiddenExpected
    ) < 0.000001);

    assert(std::abs(
        threeLayerSnapshot.Layers[2].Neurons[0].Activation -
        outputExpected
    ) < 0.000001);

    MiaIAClient::ClearNetwork();

    assert(!MiaIAClient::Forward());

    assert(MiaIAClient::AddLayer(0, "Input"));
    assert(!MiaIAClient::Forward());

    return 0;
}