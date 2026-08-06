#pragma once

#include "../../Core/Model/Network.h"

#include <unordered_map>

namespace MiaIA::Engine
{
    class NetworkTopology
    {
    public:

        explicit NetworkTopology(
            Core::Network& network);


        Core::Neuron* FindNeuron(
            std::uint64_t neuronId);


        Core::Layer* FindLayer(
            std::uint64_t layerId);


        Core::Connection* FindConnection(
            std::uint64_t connectionId);

        Core::Layer* FindLayerForNeuron(
            std::uint64_t neuronId);

    private:

        Core::Network& network;


        std::unordered_map<
            std::uint64_t,
            Core::Neuron*> neurons;


        std::unordered_map<
            std::uint64_t,
            Core::Layer*> layers;


        std::unordered_map<
            std::uint64_t,
            Core::Connection*> connections;
    };
}