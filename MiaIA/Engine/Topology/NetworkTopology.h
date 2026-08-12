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


        explicit NetworkTopology(
            const Core::Network& network);


        Core::Neuron* FindNeuron(
            std::uint64_t neuronId);

        const Core::Neuron* FindNeuron(
            std::uint64_t neuronId) const;


        Core::Layer* FindLayer(
            std::uint64_t layerId);

        const Core::Layer* FindLayer(
            std::uint64_t layerId) const;


        Core::Connection* FindConnection(
            std::uint64_t connectionId);


        const Core::Connection* FindConnection(
            std::uint64_t connectionId) const;


        Core::Layer* FindLayerForNeuron(
            std::uint64_t neuronId);

        const Core::Layer* FindLayerForNeuron(
            std::uint64_t neuronId) const;


    private:

        Core::Network* network;

        const Core::Network* constNetwork;


        std::unordered_map<
            std::uint64_t,
            Core::Neuron*> neurons;


        std::unordered_map<
            std::uint64_t,
            Core::Layer*> layers;


        std::unordered_map<
            std::uint64_t,
            Core::Connection*> connections;


        std::unordered_map<
            std::uint64_t,
            const Core::Neuron*> constNeurons;


        std::unordered_map<
            std::uint64_t,
            const Core::Layer*> constLayers;


        std::unordered_map<
            std::uint64_t,
            const Core::Connection*> constConnections;
    };
}
