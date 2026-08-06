#include <cmath>
#include "../Include/MiaIAClient.h"
#include "../../Core/Model/Network.h"
#include "../../Core/Execution/Activation.h"
#include "../../Engine/Runtime/NetworkRuntime.h"

namespace MiaIA::SDK
{
    Core::Network MiaIAClient::CurrentNetwork;

    int MiaIAClient::TestConnection()
    {
        return 1001;
    }

    void MiaIAClient::ClearNetwork()
    {
        CurrentNetwork.Layers.clear();
        CurrentNetwork.Connections.clear();
    }

    bool MiaIAClient::AddLayer(std::uint64_t id, const std::string& name, std::uint64_t order)
    {
        if (name.empty())
        {
            return false;
        }

        for (const Core::Layer& layer : CurrentNetwork.Layers)
        {
            if (layer.Id == id)
            {
                return false;
            }
            
            if (layer.Order == order)
            {
                return false;
            }

            if (order > CurrentNetwork.Layers.size())
            {
                return false;
            }
            
        }

        Core::Layer layer;
        layer.Id = id;
        layer.Name = name;
        layer.Order = order;

        CurrentNetwork.Layers.push_back(layer);

        return true;
    }

    bool MiaIAClient::AddNeuron(
        std::uint64_t layerId,
        std::uint64_t neuronId,
        double bias,
        double activation)
    {
        
        if (!std::isfinite(bias) || !std::isfinite(activation))
        {
            return false;
        }

        for (const Core::Layer& layer : CurrentNetwork.Layers)
        {
            for (const Core::Neuron& neuron : layer.Neurons)
            {
                if (neuron.Id == neuronId)
                {
                    return false;
                }
            }
        }

        for (Core::Layer& layer : CurrentNetwork.Layers)
        {
            if (layer.Id == layerId)
            {
                layer.Neurons.push_back(
                    Core::Neuron{
                        neuronId,
                        bias,
                        activation
                    });

                return true;
            }
        }

        return false;
    }

    bool MiaIAClient::AddConnection(
        std::uint64_t id,
        std::uint64_t fromNeuron,
        std::uint64_t toNeuron,
        double weight)
    {
        
        if (!std::isfinite(weight))
        {
            return false;
        }

        if (fromNeuron == toNeuron)
        {
            return false;
        }

        for (const Core::Connection& connection : CurrentNetwork.Connections)
        {
            if (connection.Id == id)
            {
                return false;
            }

            if (connection.FromNeuron == fromNeuron &&
                connection.ToNeuron == toNeuron)
            {
                return false;
            }
        }

        const Core::Layer* fromLayer = nullptr;
        const Core::Layer* toLayer = nullptr;

        for (const Core::Layer& layer : CurrentNetwork.Layers)
        {
            for (const Core::Neuron& neuron : layer.Neurons)
            {
                if (neuron.Id == fromNeuron)
                {
                    fromLayer = &layer;
                }

                if (neuron.Id == toNeuron)
                {
                    toLayer = &layer;
                }
            }
        }

        if (fromLayer == nullptr || toLayer == nullptr)
        {
            return false;
        }

        if (fromLayer->Order >= toLayer->Order)
        {
            return false;
        }

        CurrentNetwork.Connections.push_back(
            Core::Connection{
                id,
                fromNeuron,
                toNeuron,
                weight
            });

        return true;
    }

    bool MiaIAClient::SetNeuronActivation(
        std::uint64_t neuronId,
        double activation)
    {
        if (!std::isfinite(activation))
        {
            return false;
        }

        for (Core::Layer& layer : CurrentNetwork.Layers)
        {
            for (Core::Neuron& neuron : layer.Neurons)
            {
                if (neuron.Id == neuronId)
                {
                    neuron.Activation = activation;
                    return true;
                }
            }
        }

        return false;
    }

    bool MiaIAClient::SetNeuronBias(
        std::uint64_t neuronId,
        double bias)
    {
        if (!std::isfinite(bias))
        {
            return false;
        }

        for (Core::Layer& layer : CurrentNetwork.Layers)
        {
            for (Core::Neuron& neuron : layer.Neurons)
            {
                if (neuron.Id == neuronId)
                {
                    neuron.Bias = bias;
                    return true;
                }
            }
        }

        return false;
    }

    bool MiaIAClient::SetConnectionWeight(
        std::uint64_t connectionId,
        double weight)
    {
        if (!std::isfinite(weight))
        {
            return false;
        }

        for (Core::Connection& connection : CurrentNetwork.Connections)
        {
            if (connection.Id == connectionId)
            {
                connection.Weight = weight;
                return true;
            }
        }

        return false;
    }

    bool MiaIAClient::TryGetNeuron(
        std::uint64_t neuronId,
        Core::NeuronSnapshot& result)
    {
        for (const Core::Layer& layer : CurrentNetwork.Layers)
        {
            for (const Core::Neuron& neuron : layer.Neurons)
            {
                if (neuron.Id == neuronId)
                {
                    result = Core::NeuronSnapshot{
                        neuron.Id,
                        neuron.Activation,
                        neuron.Bias
                    };

                    return true;
                }
            }
        }

        return false;
    }

    bool MiaIAClient::TryGetConnection(
        std::uint64_t connectionId,
        Core::ConnectionSnapshot& result)
    {
        for (const Core::Connection& connection : CurrentNetwork.Connections)
        {
            if (connection.Id == connectionId)
            {
                result = Core::ConnectionSnapshot{
                    connection.Id,
                    connection.FromNeuron,
                    connection.ToNeuron,
                    connection.Weight
                };

                return true;
            }
        }

        return false;
    }

    bool MiaIAClient::TryGetLayer(
        std::uint64_t layerId,
        Core::LayerSnapshot& result)
    {
        for (const Core::Layer& layer : CurrentNetwork.Layers)
        {
            if (layer.Id == layerId)
            {
                result.Id = layer.Id;
                result.Name = layer.Name;
                result.Neurons.clear();
                result.Neurons.reserve(layer.Neurons.size());

                for (const Core::Neuron& neuron : layer.Neurons)
                {
                    result.Neurons.push_back(
                        Core::NeuronSnapshot{
                            neuron.Id,
                            neuron.Activation,
                            neuron.Bias
                        });
                }

                return true;
            }
        }

        return false;
    }

    bool MiaIAClient::RemoveConnection(std::uint64_t connectionId)
    {
        for (auto it = CurrentNetwork.Connections.begin();
            it != CurrentNetwork.Connections.end();
            ++it)
        {
            if (it->Id == connectionId)
            {
                CurrentNetwork.Connections.erase(it);
                return true;
            }
        }

        return false;
    }

    bool MiaIAClient::RemoveNeuron(std::uint64_t neuronId)
    {
        for (Core::Layer& layer : CurrentNetwork.Layers)
        {
            for (auto neuronIt = layer.Neurons.begin();
                neuronIt != layer.Neurons.end();
                ++neuronIt)
            {
                if (neuronIt->Id == neuronId)
                {
                    layer.Neurons.erase(neuronIt);

                    for (auto connectionIt = CurrentNetwork.Connections.begin();
                        connectionIt != CurrentNetwork.Connections.end();)
                    {
                        if (connectionIt->FromNeuron == neuronId ||
                            connectionIt->ToNeuron == neuronId)
                        {
                            connectionIt =
                                CurrentNetwork.Connections.erase(connectionIt);
                        }
                        else
                        {
                            ++connectionIt;
                        }
                    }

                    return true;
                }
            }
        }

        return false;
    }

    bool MiaIAClient::RemoveLayer(std::uint64_t layerId)
    {
        for (auto layerIt = CurrentNetwork.Layers.begin();
            layerIt != CurrentNetwork.Layers.end();
            ++layerIt)
        {
            if (layerIt->Id == layerId)
            {
                const std::uint64_t removedOrder = layerIt->Order;

                for (const Core::Neuron& neuron : layerIt->Neurons)
                {
                    for (auto connectionIt = CurrentNetwork.Connections.begin();
                        connectionIt != CurrentNetwork.Connections.end();)
                    {
                        if (connectionIt->FromNeuron == neuron.Id ||
                            connectionIt->ToNeuron == neuron.Id)
                        {
                            connectionIt =
                                CurrentNetwork.Connections.erase(connectionIt);
                        }
                        else
                        {
                            ++connectionIt;
                        }
                    }
                }

                CurrentNetwork.Layers.erase(layerIt);

                for (Core::Layer& layer : CurrentNetwork.Layers)
                {
                    if (layer.Order > removedOrder)
                    {
                        --layer.Order;
                    }
                }

                return true;
            }
        }

        return false;
    }

    bool MiaIAClient::SetLayerActivation(
        std::uint64_t layerId,
        Core::ActivationType activation)
    {
        for (Core::Layer& layer : CurrentNetwork.Layers)
        {
            if (layer.Id == layerId)
            {
                layer.Activation = activation;
                return true;
            }
        }

        return false;
    }

    bool MiaIAClient::Forward()
    {
        return Engine::NetworkRuntime::Forward(CurrentNetwork);
    }

    Core::NetworkSnapshot MiaIAClient::GetSnapshot()
    {
        return Engine::NetworkRuntime::Snapshot(CurrentNetwork);
    }

}