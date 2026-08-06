#pragma once

#include <cstdint>
#include <string>

#include "../../Core/Public/NetworkSnapshot.h"
#include "../../Core/Model/Network.h"

namespace MiaIA::SDK
{
    class MiaIAClient
    {
    public:
        [[nodiscard]]
        static Core::NetworkSnapshot GetSnapshot();
        static void ClearNetwork();
        static bool AddLayer(std::uint64_t id, const std::string& name);
        static bool AddNeuron(std::uint64_t layerId, std::uint64_t neuronId, double bias, double activation);
        static bool AddConnection(std::uint64_t id, std::uint64_t fromNeuron, std::uint64_t toNeuron, double weight);
        static bool SetNeuronActivation(std::uint64_t neuronId, double activation);
        static bool SetNeuronBias(std::uint64_t neuronId, double bias);
        static bool SetConnectionWeight(std::uint64_t connectionId, double weight);
        static bool TryGetNeuron(std::uint64_t neuronId, Core::NeuronSnapshot& result);
        static bool TryGetConnection(std::uint64_t connectionId, Core::ConnectionSnapshot& result);
        static bool TryGetLayer(std::uint64_t layerId, Core::LayerSnapshot& result);
        static bool RemoveConnection(std::uint64_t connectionId);
        static bool RemoveNeuron(std::uint64_t neuronId);
        static bool RemoveLayer(std::uint64_t layerId);
        static bool Forward();

        static int TestConnection();

    private:
        static Core::Network CurrentNetwork;
    };
}
