// Console.cpp : Questo file contiene la funzione 'main', in cui inizia e termina l'esecuzione del programma.
//

#include <iostream>
#include "../SDK/Include/MiaIAClient.h"

int main()
{
    using MiaIA::SDK::MiaIAClient;

    MiaIAClient::ClearNetwork();

    MiaIAClient::AddLayer(0, "Input", 0);
    MiaIAClient::AddLayer(1, "Hidden", 1);

    MiaIAClient::AddNeuron(0, 1001, 0.75, 0.25);
    MiaIAClient::AddNeuron(0, 1002, 0.45, 0.10);

    MiaIAClient::AddNeuron(1, 2001, 0.60, 0.30);
    MiaIAClient::AddNeuron(1, 2002, 0.20, 0.15);

    MiaIAClient::AddConnection(1, 1001, 2001, 0.8);
    MiaIAClient::AddConnection(2, 1002, 2002, 0.5);

    std::cout << std::boolalpha;

    std::cout << "Set activation: "
        << MiaIAClient::SetNeuronActivation(1001, 0.95)
        << '\n';

    const auto snapshot = MiaIAClient::GetSnapshot();

    for (const auto& layer : snapshot.Layers)
    {
        std::cout << "Layer: " << layer.Name << '\n';

        for (const auto& neuron : layer.Neurons)
        {
            std::cout
                << "  Neuron " << neuron.Id
                << " Bias " << neuron.Bias
                << " Activation " << neuron.Activation
                << '\n';
        }
    }

    for (const auto& connection : snapshot.Connections)
    {
        std::cout
            << "Connection " << connection.Id
            << ": " << connection.FromNeuron
            << " -> " << connection.ToNeuron
            << " Weight " << connection.Weight
            << '\n';
    }

    std::cout << std::boolalpha;

    std::cout << "Duplicate layer: "
        << MiaIAClient::AddLayer(0, "Duplicate", 0)
        << '\n';

    std::cout << "Duplicate neuron: "
        << MiaIAClient::AddNeuron(1, 1001, 0.2, 0.3)
        << '\n';

    std::cout << "Missing neuron connection: "
        << MiaIAClient::AddConnection(3, 1001, 9999, 0.4)
        << '\n';

    std::cout << "Duplicate connection: "
        << MiaIAClient::AddConnection(1, 1001, 2001, 0.8)
        << '\n';

    return 0;
}