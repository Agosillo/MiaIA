#include "OnnxExporter.h"

#include "../Validation/NetworkValidator.h"
#include "../../Core/Model/Network.h"

#include <onnx/onnx_pb.h>

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace
{
    constexpr std::int64_t OnnxIrVersion = 8;
    constexpr std::int64_t OnnxOpsetVersion = 18;

    using MiaIA::Core::ActivationType;
    using MiaIA::Core::Connection;
    using MiaIA::Core::Layer;
    using MiaIA::Core::Network;

    struct NeuronLocation
    {
        std::uint64_t LayerOrder{};
        std::size_t Index{};
    };

    void SetTensorType(
        onnx::ValueInfoProto& value,
        const std::string& name,
        std::size_t featureCount)
    {
        value.set_name(name);

        auto* tensorType = value.mutable_type()->mutable_tensor_type();
        tensorType->set_elem_type(onnx::TensorProto_DataType_DOUBLE);

        auto* shape = tensorType->mutable_shape();
        shape->add_dim()->set_dim_param("batch");
        shape->add_dim()->set_dim_value(
            static_cast<std::int64_t>(featureCount));
    }

    onnx::TensorProto* AddTensor(
        onnx::GraphProto& graph,
        const std::string& name,
        const std::vector<std::int64_t>& dimensions,
        const std::vector<double>& values)
    {
        auto* tensor = graph.add_initializer();
        tensor->set_name(name);
        tensor->set_data_type(onnx::TensorProto_DataType_DOUBLE);

        for (const std::int64_t dimension : dimensions)
        {
            tensor->add_dims(dimension);
        }

        for (const double value : values)
        {
            tensor->add_double_data(value);
        }

        return tensor;
    }

    std::string ActivationOperator(ActivationType activation)
    {
        switch (activation)
        {
        case ActivationType::Sigmoid:
            return "Sigmoid";
        case ActivationType::ReLU:
            return "Relu";
        case ActivationType::Tanh:
            return "Tanh";
        case ActivationType::Linear:
            return {};
        }

        return {};
    }

    bool BuildTopology(
        const Network& network,
        std::vector<const Layer*>& orderedLayers,
        std::unordered_map<std::uint64_t, NeuronLocation>& locations)
    {
        orderedLayers.reserve(network.Layers.size());

        for (const Layer& layer : network.Layers)
        {
            if (layer.Neurons.empty())
            {
                return false;
            }

            orderedLayers.push_back(&layer);
        }

        std::sort(
            orderedLayers.begin(),
            orderedLayers.end(),
            [](const Layer* left, const Layer* right)
            {
                return left->Order < right->Order;
            });

        for (const Layer* layer : orderedLayers)
        {
            for (std::size_t index = 0;
                index < layer->Neurons.size();
                ++index)
            {
                locations[layer->Neurons[index].Id] = {
                    layer->Order,
                    index
                };
            }
        }

        std::unordered_set<std::string> connectionPairs;

        for (const Connection& connection : network.Connections)
        {
            const auto from = locations.find(connection.FromNeuron);
            const auto to = locations.find(connection.ToNeuron);

            if (from == locations.end() || to == locations.end())
            {
                return false;
            }

            if (from->second.LayerOrder + 1 != to->second.LayerOrder)
            {
                return false;
            }

            const std::string pair =
                std::to_string(connection.FromNeuron) + ":" +
                std::to_string(connection.ToNeuron);

            if (!connectionPairs.insert(pair).second)
            {
                return false;
            }
        }

        return true;
    }

    void AddMetadata(
        onnx::ModelProto& model,
        const std::vector<const Layer*>& orderedLayers)
    {
        auto* schema = model.add_metadata_props();
        schema->set_key("miaia.schema_version");
        schema->set_value("1");

        for (const Layer* layer : orderedLayers)
        {
            const std::string prefix =
                "miaia.layer." + std::to_string(layer->Order);

            auto* id = model.add_metadata_props();
            id->set_key(prefix + ".id");
            id->set_value(std::to_string(layer->Id));

            auto* name = model.add_metadata_props();
            name->set_key(prefix + ".name");
            name->set_value(layer->Name);

            std::string neuronIds;

            for (const auto& neuron : layer->Neurons)
            {
                if (!neuronIds.empty())
                {
                    neuronIds += ',';
                }

                neuronIds += std::to_string(neuron.Id);
            }

            auto* neurons = model.add_metadata_props();
            neurons->set_key(prefix + ".neuron_ids");
            neurons->set_value(neuronIds);
        }
    }
}

namespace MiaIA::Engine
{
    bool OnnxExporter::Export(
        const Core::Network& network,
        const std::string& path)
    {
        if (path.empty() ||
            !NetworkValidator::ValidateForForward(network))
        {
            return false;
        }

        std::vector<const Core::Layer*> orderedLayers;
        std::unordered_map<std::uint64_t, NeuronLocation> locations;

        if (!BuildTopology(network, orderedLayers, locations))
        {
            return false;
        }

        onnx::ModelProto model;
        model.set_ir_version(OnnxIrVersion);
        model.set_producer_name("MiaIA");
        model.set_producer_version("0.1.0");
        model.set_domain("ai.miaia");
        model.set_model_version(1);

        auto* opset = model.add_opset_import();
        opset->set_domain("");
        opset->set_version(OnnxOpsetVersion);

        auto* graph = model.mutable_graph();
        graph->set_name("MiaIA Network");

        const std::string inputName = "input";
        SetTensorType(
            *graph->add_input(),
            inputName,
            orderedLayers.front()->Neurons.size());

        std::string previousOutput = inputName;

        for (std::size_t layerIndex = 1;
            layerIndex < orderedLayers.size();
            ++layerIndex)
        {
            const Core::Layer& previousLayer =
                *orderedLayers[layerIndex - 1];
            const Core::Layer& layer = *orderedLayers[layerIndex];

            const std::string prefix =
                "layer_" + std::to_string(layer.Order);
            const std::string weightsName = prefix + "_weights";
            const std::string biasesName = prefix + "_biases";
            const std::string linearOutput = prefix + "_linear";
            const std::string activatedOutput = prefix + "_output";

            std::vector<double> weights(
                previousLayer.Neurons.size() * layer.Neurons.size(),
                0.0);
            std::vector<double> biases;
            biases.reserve(layer.Neurons.size());

            for (const Core::Neuron& neuron : layer.Neurons)
            {
                biases.push_back(neuron.Bias);
            }

            for (const Core::Connection& connection : network.Connections)
            {
                const NeuronLocation& from = locations.at(connection.FromNeuron);
                const NeuronLocation& to = locations.at(connection.ToNeuron);

                if (to.LayerOrder != layer.Order)
                {
                    continue;
                }

                const std::size_t weightIndex =
                    from.Index * layer.Neurons.size() + to.Index;
                weights[weightIndex] = connection.Weight;
            }

            AddTensor(
                *graph,
                weightsName,
                {
                    static_cast<std::int64_t>(previousLayer.Neurons.size()),
                    static_cast<std::int64_t>(layer.Neurons.size())
                },
                weights);
            AddTensor(
                *graph,
                biasesName,
                { static_cast<std::int64_t>(layer.Neurons.size()) },
                biases);

            auto* gemm = graph->add_node();
            gemm->set_name(prefix + "_gemm");
            gemm->set_op_type("Gemm");
            gemm->add_input(previousOutput);
            gemm->add_input(weightsName);
            gemm->add_input(biasesName);

            const std::string activationOperator =
                ActivationOperator(layer.Activation);

            if (activationOperator.empty())
            {
                gemm->add_output(activatedOutput);
            }
            else
            {
                gemm->add_output(linearOutput);

                auto* activation = graph->add_node();
                activation->set_name(prefix + "_activation");
                activation->set_op_type(activationOperator);
                activation->add_input(linearOutput);
                activation->add_output(activatedOutput);
            }

            previousOutput = activatedOutput;
        }

        SetTensorType(
            *graph->add_output(),
            previousOutput,
            orderedLayers.back()->Neurons.size());

        AddMetadata(model, orderedLayers);

        std::string serializedModel;

        if (!model.SerializeToString(&serializedModel))
        {
            return false;
        }

        std::ofstream output(
            path,
            std::ios::binary | std::ios::trunc);

        if (!output)
        {
            return false;
        }

        output.write(
            serializedModel.data(),
            static_cast<std::streamsize>(serializedModel.size()));

        return output.good();
    }
}
