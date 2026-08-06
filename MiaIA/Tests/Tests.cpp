#include <cmath>
#include <limits>
#include "TestHarness.h"
#include "../SDK/Include/MiaIAClient.h"
#include "../Core/Execution/Activation.h"
#include "../Engine/Validation/NetworkValidator.h"

// Keep the existing checks active in every build configuration.
#ifdef assert
#undef assert
#endif
#define assert(expression) MIAIA_CHECK(expression)

int main()
{
    using MiaIA::SDK::MiaIAClient;

    MiaIA::Tests::TestRunner runner;

    runner.Run("Network editing and snapshots", [&]()
    {
    MiaIAClient::ClearNetwork();

    assert(MiaIAClient::AddLayer(0, "Input", 0));
    assert(!MiaIAClient::AddLayer(0, "Duplicate", 1));

    assert(MiaIAClient::AddNeuron(0, 1001, 0.75, 0.25));
    assert(!MiaIAClient::AddNeuron(0, 1001, 0.10, 0.10));

    const double nan =
        std::numeric_limits<double>::quiet_NaN();

    const double infinity =
        std::numeric_limits<double>::infinity();

    assert(!MiaIAClient::AddNeuron(0, 1002, nan, 0.0));
    assert(!MiaIAClient::AddNeuron(0, 1002, infinity, 0.0));
    assert(!MiaIAClient::AddNeuron(0, 1002, 0.0, nan));
    assert(!MiaIAClient::AddNeuron(0, 1002, 0.0, infinity));

    assert(MiaIAClient::AddLayer(1, "Hidden", 1));
    assert(!MiaIAClient::AddNeuron(1, 1001, 0.60, 0.30));
    assert(MiaIAClient::AddNeuron(1, 2001, 0.60, 0.30));

    const auto afterInvalidNeurons = MiaIAClient::GetSnapshot();

    assert(afterInvalidNeurons.Layers[0].Neurons.size() == 1);
    assert(afterInvalidNeurons.Layers[1].Neurons.size() == 1);

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

    assert(MiaIAClient::AddLayer(0, "Input", 0));
    assert(MiaIAClient::AddLayer(1, "Hidden", 1));

    assert(MiaIAClient::AddNeuron(0, 1001, 0.75, 0.25));
    assert(MiaIAClient::AddNeuron(1, 2001, 0.60, 0.30));

    assert(MiaIAClient::AddConnection(1, 1001, 2001, 0.8));

    assert(MiaIAClient::RemoveLayer(0));

    const auto afterLayerRemoval = MiaIAClient::GetSnapshot();

    assert(afterLayerRemoval.Layers.size() == 1);
    assert(afterLayerRemoval.Layers[0].Id == 1);
    assert(afterLayerRemoval.Connections.empty());

    MiaIAClient::ClearNetwork();

    assert(MiaIAClient::AddLayer(0, "Input", 0));
    assert(MiaIAClient::AddLayer(1, "Hidden", 1));
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

    assert(MiaIAClient::SetLayerActivation(
        1,
        MiaIA::Core::ActivationType::Tanh));

    assert(!MiaIAClient::SetLayerActivation(
        9999,
        MiaIA::Core::ActivationType::Linear));

    layer.Order = 999;
    layer.Activation = MiaIA::Core::ActivationType::Linear;

    assert(MiaIAClient::TryGetLayer(1, layer));
    assert(layer.Id == 1);
    assert(layer.Name == "Hidden");
    assert(layer.Order == 1);
    assert(layer.Activation == MiaIA::Core::ActivationType::Tanh);
    assert(layer.Neurons.size() == 1);
    assert(layer.Neurons[0].Id == 2001);

    assert(!MiaIAClient::TryGetNeuron(9999, neuron));
    assert(!MiaIAClient::TryGetConnection(9999, connection));
    assert(!MiaIAClient::TryGetLayer(9999, layer));

    assert(MiaIAClient::RemoveConnection(1));
    assert(!MiaIAClient::RemoveConnection(1));

    const auto afterConnectionRemoval = MiaIAClient::GetSnapshot();

    assert(afterConnectionRemoval.Connections.empty());

    });

    runner.Run("Forward propagation and activations", [&]()
    {

    MiaIAClient::ClearNetwork();

    assert(MiaIAClient::AddLayer(0, "Input", 0));
    assert(MiaIAClient::AddLayer(1, "Output", 1));

    assert(MiaIAClient::AddNeuron(0, 1001, 0.0, 1.0));
    assert(MiaIAClient::AddNeuron(1, 2001, 0.0, 0.0));

    assert(MiaIAClient::AddConnection(1, 1001, 2001, 1.0));

    assert(MiaIAClient::Forward());

    const auto forwardSnapshot = MiaIAClient::GetSnapshot();

    const double expected = MiaIA::Core::Activation::Sigmoid(1.0);

    assert(std::abs(forwardSnapshot.Layers[1].Neurons[0].Activation - expected) < 0.000001);

    MiaIAClient::ClearNetwork();

    assert(MiaIAClient::AddLayer(0, "Input", 0));
    assert(MiaIAClient::AddLayer(1, "Output", 1));

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

    assert(MiaIAClient::AddLayer(0, "Input", 0));
    assert(MiaIAClient::AddLayer(1, "Hidden", 1));
    assert(MiaIAClient::AddLayer(2, "Output", 2));

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

    assert(MiaIAClient::AddLayer(0, "Input", 0));
    assert(!MiaIAClient::Forward());


    assert(MiaIA::Core::Activation::ReLU(2.5) == 2.5);
    assert(MiaIA::Core::Activation::ReLU(0.0) == 0.0);
    assert(MiaIA::Core::Activation::ReLU(-3.0) == 0.0);

    MiaIAClient::ClearNetwork();

    assert(MiaIAClient::AddLayer(0, "Input", 0));
    assert(MiaIAClient::AddLayer(1, "Output", 1));

    assert(MiaIAClient::SetLayerActivation(1, MiaIA::Core::ActivationType::ReLU));

    assert(MiaIAClient::AddNeuron(0, 1001, 0.0, 1.0));
    assert(MiaIAClient::AddNeuron(1, 2001, -2.0, 0.0));

    assert(MiaIAClient::AddConnection(1, 1001, 2001, 1.0));

    assert(MiaIAClient::Forward());

    const auto reluSnapshot = MiaIAClient::GetSnapshot();

    assert(reluSnapshot.Layers[1].Neurons[0].Activation == 0.0);

    assert(reluSnapshot.Layers[1].Activation ==  MiaIA::Core::ActivationType::ReLU);

    MiaIAClient::ClearNetwork();

    assert(MiaIAClient::AddLayer(0, "Input", 0));
    assert(MiaIAClient::AddLayer(1, "Output", 1));

    assert(MiaIAClient::SetLayerActivation(
        1,
        MiaIA::Core::ActivationType::Tanh));

    assert(MiaIAClient::AddNeuron(0, 1001, 0.0, 1.0));
    assert(MiaIAClient::AddNeuron(1, 2001, 0.0, 0.0));

    assert(MiaIAClient::AddConnection(1, 1001, 2001, 1.0));

    assert(MiaIAClient::Forward());

    const auto tanhSnapshot = MiaIAClient::GetSnapshot();

    const double tanhExpected =
        MiaIA::Core::Activation::Tanh(1.0);

    assert(std::abs(
        tanhSnapshot.Layers[1].Neurons[0].Activation -
        tanhExpected
    ) < 0.000001);

    assert(
        tanhSnapshot.Layers[1].Activation ==
        MiaIA::Core::ActivationType::Tanh);

    MiaIAClient::ClearNetwork();

    assert(MiaIAClient::AddLayer(0, "Input", 0));
    assert(MiaIAClient::AddLayer(1, "Output", 1));

    assert(MiaIAClient::SetLayerActivation(
        1,
        MiaIA::Core::ActivationType::Linear));

    assert(MiaIAClient::AddNeuron(0, 1001, 0.0, 2.0));
    assert(MiaIAClient::AddNeuron(1, 2001, 0.5, 0.0));

    assert(MiaIAClient::AddConnection(1, 1001, 2001, 1.5));

    assert(MiaIAClient::Forward());

    const auto linearSnapshot = MiaIAClient::GetSnapshot();

    assert(std::abs(
        linearSnapshot.Layers[1].Neurons[0].Activation - 3.5
    ) < 0.000001);

    assert(
        linearSnapshot.Layers[1].Activation ==
        MiaIA::Core::ActivationType::Linear);

    MiaIAClient::ClearNetwork();

    assert(MiaIAClient::AddLayer(20, "Output", 2));
    assert(MiaIAClient::AddLayer(10, "Input", 0));
    assert(MiaIAClient::AddLayer(15, "Hidden", 1));

    assert(MiaIAClient::AddNeuron(10, 1001, 0.0, 1.0));
    assert(MiaIAClient::AddNeuron(15, 2001, 0.0, 0.0));
    assert(MiaIAClient::AddNeuron(20, 3001, 0.0, 0.0));

    assert(MiaIAClient::AddConnection(1, 1001, 2001, 1.0));
    assert(MiaIAClient::AddConnection(2, 2001, 3001, 1.0));

    assert(MiaIAClient::Forward());

    const auto orderedSnapshot = MiaIAClient::GetSnapshot();

    assert(orderedSnapshot.Layers[0].Order == 0);
    assert(orderedSnapshot.Layers[1].Order == 1);
    assert(orderedSnapshot.Layers[2].Order == 2);

    assert(orderedSnapshot.Layers[0].Name == "Input");
    assert(orderedSnapshot.Layers[1].Name == "Hidden");
    assert(orderedSnapshot.Layers[2].Name == "Output");

    });

    runner.Run("Topology editing and layer ordering", [&]()
    {

    MiaIAClient::ClearNetwork();

    assert(MiaIAClient::AddLayer(100, "Input", 0));
    assert(MiaIAClient::AddLayer(10, "Output", 1));

    assert(MiaIAClient::AddNeuron(100, 1001, 0.0, 1.0));
    assert(MiaIAClient::AddNeuron(10, 2001, 0.0, 0.0));

    assert(MiaIAClient::AddConnection(1, 1001, 2001, 1.0));
    assert(!MiaIAClient::AddConnection(2, 2001, 1001, 1.0));

    MiaIAClient::ClearNetwork();

    assert(MiaIAClient::AddLayer(0, "Input", 0));
    assert(MiaIAClient::AddLayer(1, "Output", 1));

    MiaIAClient::ClearNetwork();

    assert(MiaIAClient::AddLayer(10, "Input", 0));
    assert(MiaIAClient::AddLayer(20, "Hidden", 1));
    assert(MiaIAClient::AddLayer(30, "Output", 2));

    assert(MiaIAClient::RemoveLayer(20));

    const auto compactedSnapshot = MiaIAClient::GetSnapshot();

    assert(compactedSnapshot.Layers.size() == 2);
    assert(compactedSnapshot.Layers[0].Order == 0);
    assert(compactedSnapshot.Layers[1].Order == 1);
    assert(compactedSnapshot.Layers[1].Name == "Output");

    assert(!MiaIAClient::Forward());

    assert(MiaIAClient::AddNeuron(10, 1001, 0.0, 1.0));
    assert(MiaIAClient::AddNeuron(30, 3001, 0.0, 0.0));

    assert(MiaIAClient::AddConnection(10, 1001, 3001, 1.0));

    assert(MiaIAClient::Forward());

    const auto rewiredSnapshot = MiaIAClient::GetSnapshot();

    const double rewiredExpected =
        MiaIA::Core::Activation::Sigmoid(1.0);

    assert(std::abs(
        rewiredSnapshot.Layers[1].Neurons[0].Activation -
        rewiredExpected
    ) < 0.000001);

    });

    runner.Run("Network validation", [&]()
    {
    MiaIAClient::ClearNetwork();

    assert(MiaIAClient::AddLayer(0, "Input", 0));
    assert(MiaIAClient::AddLayer(1, "Output", 1));

    assert(MiaIAClient::AddNeuron(0, 1001, 0.0, 1.0));
    assert(MiaIAClient::AddNeuron(1, 2001, 0.0, 0.0));

    assert(!MiaIAClient::Forward());

    MiaIAClient::ClearNetwork();

    assert(MiaIAClient::AddLayer(0, "Input", 0));
    assert(MiaIAClient::AddLayer(1, "Output", 1));

    assert(MiaIAClient::AddNeuron(0, 1001, 0.0, 1.0));
    assert(MiaIAClient::AddNeuron(1, 2001, 0.0, 0.0));

    assert(!MiaIAClient::AddConnection(1, 2001, 1001, 1.0));

    MiaIA::Core::Network invalidNetwork;

    assert(!MiaIA::Engine::NetworkValidator::ValidateForForward(
        invalidNetwork));

    MiaIA::Core::Network validNetwork;

    MiaIA::Core::Layer inputLayer;
    inputLayer.Id = 10;
    inputLayer.Name = "Input";
    inputLayer.Order = 0;
    inputLayer.Neurons.push_back({ 1001, 0.0, 1.0 });

    MiaIA::Core::Layer outputLayer;
    outputLayer.Id = 20;
    outputLayer.Name = "Output";
    outputLayer.Order = 1;
    outputLayer.Neurons.push_back({ 2001, 0.0, 0.0 });

    validNetwork.Layers.push_back(inputLayer);
    validNetwork.Layers.push_back(outputLayer);

    validNetwork.Connections.push_back({
        1,
        1001,
        2001,
        1.0
        });

    assert(
        MiaIA::Engine::NetworkValidator::ValidateForForward(
            validNetwork));

    MiaIA::Core::Network invalidOrderNetwork;

    MiaIA::Core::Layer firstLayer;
    firstLayer.Id = 10;
    firstLayer.Name = "Input";
    firstLayer.Order = 0;
    firstLayer.Neurons.push_back({ 1001, 0.0, 1.0 });

    MiaIA::Core::Layer secondLayer;
    secondLayer.Id = 20;
    secondLayer.Name = "Output";
    secondLayer.Order = 2;
    secondLayer.Neurons.push_back({ 2001, 0.0, 0.0 });

    invalidOrderNetwork.Layers.push_back(firstLayer);
    invalidOrderNetwork.Layers.push_back(secondLayer);

    invalidOrderNetwork.Connections.push_back({
        1,
        1001,
        2001,
        1.0
        });

    assert(
        !MiaIA::Engine::NetworkValidator::ValidateForForward(
            invalidOrderNetwork));

    MiaIA::Core::Network invalidConnectionNetwork;

    MiaIA::Core::Layer input;
    input.Id = 10;
    input.Name = "Input";
    input.Order = 0;
    input.Neurons.push_back({ 1001, 0.0, 1.0 });

    MiaIA::Core::Layer output;
    output.Id = 20;
    output.Name = "Output";
    output.Order = 1;
    output.Neurons.push_back({ 2001, 0.0, 0.0 });

    invalidConnectionNetwork.Layers.push_back(input);
    invalidConnectionNetwork.Layers.push_back(output);

    invalidConnectionNetwork.Connections.push_back({
        1,
        1001,
        9999,
        1.0
        });

    assert(
        !MiaIA::Engine::NetworkValidator::ValidateForForward(
            invalidConnectionNetwork));


    MiaIA::Core::Network backwardConnectionNetwork;

    MiaIA::Core::Layer backwardInput;
    backwardInput.Id = 10;
    backwardInput.Name = "Input";
    backwardInput.Order = 0;
    backwardInput.Neurons.push_back({ 1001, 0.0, 1.0 });

    MiaIA::Core::Layer backwardOutput;
    backwardOutput.Id = 20;
    backwardOutput.Name = "Output";
    backwardOutput.Order = 1;
    backwardOutput.Neurons.push_back({ 2001, 0.0, 0.0 });

    backwardConnectionNetwork.Layers.push_back(backwardInput);
    backwardConnectionNetwork.Layers.push_back(backwardOutput);

    backwardConnectionNetwork.Connections.push_back({
        1,
        2001,
        1001,
        1.0
        });

    assert(
        !MiaIA::Engine::NetworkValidator::ValidateForForward(
            backwardConnectionNetwork));


    MiaIA::Core::Network disconnectedNeuronNetwork;

    MiaIA::Core::Layer disconnectedInput;
    disconnectedInput.Id = 10;
    disconnectedInput.Name = "Input";
    disconnectedInput.Order = 0;
    disconnectedInput.Neurons.push_back({ 1001, 0.0, 1.0 });

    MiaIA::Core::Layer disconnectedOutput;
    disconnectedOutput.Id = 20;
    disconnectedOutput.Name = "Output";
    disconnectedOutput.Order = 1;
    disconnectedOutput.Neurons.push_back({ 2001, 0.0, 0.0 });
    disconnectedOutput.Neurons.push_back({ 2002, 0.0, 0.0 });

    disconnectedNeuronNetwork.Layers.push_back(disconnectedInput);
    disconnectedNeuronNetwork.Layers.push_back(disconnectedOutput);

    disconnectedNeuronNetwork.Connections.push_back({
        1,
        1001,
        2001,
        1.0
        });

    assert(
        !MiaIA::Engine::NetworkValidator::ValidateForForward(
            disconnectedNeuronNetwork));


    MiaIA::Core::Network duplicateNeuronNetwork;

    MiaIA::Core::Layer duplicateInput;
    duplicateInput.Id = 10;
    duplicateInput.Name = "Input";
    duplicateInput.Order = 0;
    duplicateInput.Neurons.push_back({ 1001, 0.0, 1.0 });

    MiaIA::Core::Layer duplicateOutput;
    duplicateOutput.Id = 20;
    duplicateOutput.Name = "Output";
    duplicateOutput.Order = 1;
    duplicateOutput.Neurons.push_back({ 1001, 0.0, 0.0 });

    duplicateNeuronNetwork.Layers.push_back(duplicateInput);
    duplicateNeuronNetwork.Layers.push_back(duplicateOutput);

    duplicateNeuronNetwork.Connections.push_back({
        1,
        1001,
        1001,
        1.0
        });

    assert(
        !MiaIA::Engine::NetworkValidator::ValidateForForward(
            duplicateNeuronNetwork));

    });

    runner.Run("Dense network factory", [&]()
    {
    MiaIAClient::ClearNetwork();

    assert(MiaIAClient::CreateDenseNetwork(2, 3, 2, 1));

    const auto denseSnapshot = MiaIAClient::GetSnapshot();

    assert(denseSnapshot.Layers.size() == 4);
    assert(denseSnapshot.Layers[0].Name == "Input");
    assert(denseSnapshot.Layers[1].Name == "Hidden");
    assert(denseSnapshot.Layers[2].Name == "Hidden");
    assert(denseSnapshot.Layers[3].Name == "Output");
    assert(denseSnapshot.Layers[0].Order == 0);
    assert(denseSnapshot.Layers[1].Order == 1);
    assert(denseSnapshot.Layers[2].Order == 2);
    assert(denseSnapshot.Layers[3].Order == 3);
    assert(denseSnapshot.Layers[0].Neurons.size() == 2);
    assert(denseSnapshot.Layers[1].Neurons.size() == 3);
    assert(denseSnapshot.Layers[2].Neurons.size() == 3);
    assert(denseSnapshot.Layers[3].Neurons.size() == 1);
    assert(denseSnapshot.Connections.size() == 18);

    assert(!MiaIAClient::CreateDenseNetwork(0, 3, 2, 1));
    assert(!MiaIAClient::CreateDenseNetwork(2, 0, 2, 1));
    assert(!MiaIAClient::CreateDenseNetwork(2, 3, -1, 1));
    assert(!MiaIAClient::CreateDenseNetwork(2, 3, 2, 0));

    const auto afterInvalidCreation = MiaIAClient::GetSnapshot();

    assert(afterInvalidCreation.Layers.size() == 4);
    assert(afterInvalidCreation.Connections.size() == 18);

    assert(MiaIAClient::CreateDenseNetwork(2, 1, 0, 1));

    const auto directSnapshot = MiaIAClient::GetSnapshot();

    assert(directSnapshot.Layers.size() == 2);
    assert(directSnapshot.Layers[0].Name == "Input");
    assert(directSnapshot.Layers[1].Name == "Output");
    assert(directSnapshot.Connections.size() == 2);

    });

    runner.Run("Connection weights", [&]()
    {
    MiaIAClient::ClearNetwork();

    assert(MiaIAClient::AddLayer(0, "Input", 0));
    assert(MiaIAClient::AddLayer(1, "Output", 1));

    assert(MiaIAClient::AddNeuron(0, 1001, 0.0, 1.0));
    assert(MiaIAClient::AddNeuron(1, 2001, 0.0, 0.0));

    assert(MiaIAClient::AddConnection(
        1,
        1001,
        2001,
        0.5));

    assert(MiaIAClient::SetConnectionWeight(
        1,
        0.8));

    const auto weightSnapshot = MiaIAClient::GetSnapshot();

    assert(weightSnapshot.Connections[0].Weight == 0.8);

    MiaIAClient::ClearNetwork();

    assert(MiaIAClient::AddLayer(0, "Input", 0));
    assert(MiaIAClient::AddLayer(1, "Output", 1));

    assert(MiaIAClient::AddNeuron(0, 1001, 0.0, 1.0));
    assert(MiaIAClient::AddNeuron(1, 2001, 0.0, 0.0));

    assert(MiaIAClient::AddConnection(
        1,
        1001,
        2001,
        0.5));

    assert(MiaIAClient::SetConnectionWeight(
        1,
        0.8));

    double readWeight = 0.0;

    assert(MiaIAClient::GetConnectionWeight(
        1,
        readWeight));

    assert(readWeight == 0.8);

    double missingWeight = 0.0;

    assert(!MiaIAClient::GetConnectionWeight(
        9999,
        missingWeight));

    const auto afterWeightChangeSnapshot = MiaIAClient::GetSnapshot();

    assert(afterWeightChangeSnapshot.Connections[0].Weight == 0.8);

    });

    return runner.Finish();
}
