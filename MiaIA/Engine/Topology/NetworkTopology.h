#pragma once

#include "../../Core/Model/Network.h"

#include <unordered_map>

namespace MiaIA::Engine
{
    class NetworkTopology
    {
    public:

        static Core::Neuron* FindNeuron(
            Core::Network& network,
            std::uint64_t neuronId);

        static const Core::Neuron* FindNeuron(
            const Core::Network& network,
            std::uint64_t neuronId);


        static Core::Layer* FindLayer(
            Core::Network& network,
            std::uint64_t layerId);

        static const Core::Layer* FindLayer(
            const Core::Network& network,
            std::uint64_t layerId);


        static Core::Connection* FindConnection(
            Core::Network& network,
            std::uint64_t connectionId);

        static const Core::Connection* FindConnection(
            const Core::Network& network,
            std::uint64_t connectionId);


        static Core::Layer* FindLayerForNeuron(
            Core::Network& network,
            std::uint64_t neuronId);

        static const Core::Layer* FindLayerForNeuron(
            const Core::Network& network,
            std::uint64_t neuronId);
    };
}
