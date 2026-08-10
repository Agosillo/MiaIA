#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <thread>
#include <vector>
#include <onnx/checker.h>
#include <onnx/onnx_pb.h>
#include "TestHarness.h"
#include "../CLI/Include/MiaIACommandProcessor.h"
#include "../IDE/StudioCore/Include/StudioController.h"
#include "../SDK/Include/MiaIAClient.h"
#include "../Core/Execution/Activation.h"
#include "../Engine/Validation/NetworkValidator.h"

// Keep the existing checks active in every build configuration.
#ifdef assert
#undef assert
#endif
#define assert(expression) MIAIA_CHECK(expression)

namespace
{
    const onnx::TensorProto* FindInitializer(
        const onnx::GraphProto& graph,
        const std::string& name)
    {
        for (const onnx::TensorProto& tensor : graph.initializer())
        {
            if (tensor.name() == name)
            {
                return &tensor;
            }
        }

        return nullptr;
    }
}

int main()
{
    using MiaIA::SDK::MiaIAClient;

    MiaIA::Tests::TestRunner runner;

    runner.Run("Studio topology scenes", [&]()
    {
        using namespace MiaIA::Studio;

        MiaIA::Core::NetworkSnapshot network;
        network.Layers = {
            { 1, "Input", 0, { { 1001 }, { 1002 } } },
            { 2, "Output", 1, { { 1003 } } }
        };
        network.Connections = {
            { 1, 1001, 1003, 0.25 },
            { 2, 1002, 1003, -0.5 }
        };

        const auto twoDimensional =
            StudioTopologyBuilder::BuildDetailed(
                network,
                StudioViewMode::TwoDimensional);
        assert(twoDimensional.Detail ==
            StudioTopologyDetail::Detailed);
        assert(twoDimensional.Nodes.size() == 3);
        assert(twoDimensional.Links.size() == 2);
        assert(twoDimensional.Nodes[0].Position.X == 0.0);
        assert(twoDimensional.Nodes[0].Position.Y == 0.0);
        assert(twoDimensional.Nodes[1].Position.Y == 1.0);
        assert(twoDimensional.Nodes[2].Position.X == 1.0);
        assert(twoDimensional.Nodes[2].Position.Z == 0.0);

        const auto threeDimensional =
            StudioTopologyBuilder::BuildDetailed(
                network,
                StudioViewMode::ThreeDimensional);
        assert(threeDimensional.ViewMode ==
            StudioViewMode::ThreeDimensional);
        assert(threeDimensional.Nodes[0].Position.Z == 0.0);
        assert(threeDimensional.Nodes[2].Position.Z == 1.0);
        assert(threeDimensional.Nodes[0].Position.X == 0.0);
        assert(threeDimensional.Nodes[1].Position.X == 1.0);

        MiaIA::Core::NetworkOverviewSnapshot overview;
        overview.NeuronCount =
            StudioTopologyBuilder::DetailedNeuronLimit + 1;
        overview.ConnectionCount = 100;
        overview.Layers = {
            { 1, "Input", 0, 1000 },
            { 2, "Hidden", 1, 1000 },
            { 3, "Output", 2, 1 }
        };

        assert(StudioTopologyBuilder::ChooseDetail(overview) ==
            StudioTopologyDetail::Compact);
        assert(StudioTopologyBuilder::ChooseDetail(
            overview,
            overview.NeuronCount,
            overview.ConnectionCount) ==
            StudioTopologyDetail::Detailed);
        assert(StudioTopologyBuilder::RequiresCompactMode(
            10,
            101,
            10,
            100));

        const auto compact = StudioTopologyBuilder::BuildCompact(
            overview,
            StudioViewMode::ThreeDimensional);
        assert(compact.Detail == StudioTopologyDetail::Compact);
        assert(compact.Nodes.size() == 3);
        assert(compact.Links.size() == 2);
        assert(compact.Nodes[0].Kind == StudioNodeKind::Layer);
        assert(compact.Nodes[0].NeuronCount == 1000);
        assert(compact.Nodes[0].Position.Z == 0.0);
        assert(compact.Nodes[2].Position.Z == 1.0);
        assert(compact.Links[0].Aggregate);
    });

    runner.Run("Studio application controller", [&]()
    {
        using namespace MiaIA::Studio;

        MiaIAClient::ClearNetwork();
        StudioController controller;
        controller.SetViewMode(StudioViewMode::ThreeDimensional);

        const StudioCommandResult result =
            controller.ExecuteCommand("create 2 2 1 1");
        assert(result.Output.find("Dense network created") !=
            std::string::npos);
        assert(!result.ExitRequested);
        assert(controller.State().Overview.Layers.size() == 3);
        assert(controller.State().Topology.Nodes.size() == 5);
        assert(controller.State().Topology.Links.size() == 6);
        assert(controller.State().Topology.ViewMode ==
            StudioViewMode::ThreeDimensional);
        assert(controller.SelectNeuron(1001));
        assert(controller.State().Selection.Kind ==
            StudioSelectionKind::Neuron);
        assert(!controller.SelectNeuron(999999));
        assert(controller.SelectConnection(1));

        const auto suggestions = controller.GetSuggestions("pred");
        assert(suggestions.size() == 1);
        assert(suggestions[0].Completion == "predict");

        MiaIAClient::ClearNetwork();
        controller.Refresh();
        assert(controller.State().Topology.Nodes.empty());
        assert(controller.State().Selection.Kind ==
            StudioSelectionKind::None);
    });

    runner.Run("Shared CLI command processor", [&]()
    {
    using MiaIA::CLI::MiaIACommandProcessor;

    const auto blank = MiaIACommandProcessor::Execute("   ");
    assert(blank.Output.empty());
    assert(!blank.ExitRequested);

    MiaIAClient::ClearNetwork();

    const auto create = MiaIACommandProcessor::Execute(
        "  create 2 2 1 1  ");
    assert(create.Output.find("Dense network created") !=
        std::string::npos);

    const auto network = MiaIAClient::GetSnapshot();
    assert(network.Layers.size() == 3);
    assert(network.Layers[0].Neurons.size() == 2);
    assert(network.Layers[1].Neurons.size() == 2);
    assert(network.Layers[2].Neurons.size() == 1);

    const auto abbreviated = MiaIACommandProcessor::Execute("sum");
    assert(abbreviated.Output.find("Network Summary") !=
        std::string::npos);

    const auto testDirectory =
        std::filesystem::temp_directory_path() /
        "miaia_cli_command_tests";
    const auto datasetPath = testDirectory / "samples.csv";
    std::filesystem::create_directories(testDirectory);

    {
        std::ofstream dataset(datasetPath);
        dataset << "x1,x2,y\n0,0,0\n1,1,1\n";
    }

    const auto datasetImport = MiaIACommandProcessor::Execute(
        "dataset import csv 2 1 samples.csv",
        testDirectory.string());
    assert(datasetImport.Output.find("CSV dataset imported") !=
        std::string::npos);
    assert(MiaIAClient::GetDatasetSummary().SampleCount == 2);

    const auto unknown = MiaIACommandProcessor::Execute("not-a-command");
    assert(unknown.Output == "Unknown command\n");

    const auto exit = MiaIACommandProcessor::Execute("exit");
    assert(exit.ExitRequested);
    assert(exit.Output.empty());

    const auto help = MiaIACommandProcessor::Execute("help");
    assert(help.Output.find("Commands:") != std::string::npos);

    const auto trainRoot =
        MiaIACommandProcessor::GetSuggestions("tr");
    assert(trainRoot.size() == 1);
    assert(trainRoot[0].Completion == "train");

    const auto trainActions =
        MiaIACommandProcessor::GetSuggestions("train s");
    assert(trainActions.size() == 2);
    assert(trainActions[0].Completion == "train step");
    assert(trainActions[1].Completion == "train session");

    const auto sessionActions =
        MiaIACommandProcessor::GetSuggestions("train session r");
    assert(sessionActions.size() == 2);
    assert(sessionActions[0].Completion == "train session run");
    assert(sessionActions[1].Completion == "train session resume");

    const auto breakpointAdd = MiaIACommandProcessor::GetSuggestions(
        "train breakpoint add ");
    assert(breakpointAdd.size() == 5);

    assert(MiaIACommandProcessor::Execute(
        "train breakpoint clear").Output.find("cleared") !=
        std::string::npos);
    const auto breakpointCreated = MiaIACommandProcessor::Execute(
        "train breakpoint add phase commit");
    assert(breakpointCreated.Output.find("added") !=
        std::string::npos);
    assert(MiaIACommandProcessor::Execute(
        "train breakpoint list").Output.find("Committed") !=
        std::string::npos);
    assert(MiaIACommandProcessor::Execute(
        "train breakpoint enable 1 off").Output.find("updated") !=
        std::string::npos);
    assert(MiaIACommandProcessor::Execute(
        "train breakpoint remove 1").Output.find("removed") !=
        std::string::npos);

    const auto datasetFormat =
        MiaIACommandProcessor::GetSuggestions("dataset import ");
    assert(datasetFormat.size() == 1);
    assert(datasetFormat[0].Completion == "dataset import csv");

    const auto createArguments =
        MiaIACommandProcessor::GetSuggestions("create 2");
    assert(createArguments.size() == 1);
    assert(createArguments[0].Syntax.find("<inputs>") !=
        std::string::npos);

    const auto limitedSuggestions =
        MiaIACommandProcessor::GetSuggestions("", 3);
    assert(limitedSuggestions.size() == 3);

    assert(MiaIACommandProcessor::GetSuggestions(
        "unknown command").empty());

    std::filesystem::remove(datasetPath);
    std::filesystem::remove(testDirectory);

    });

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
    assert(neuron.Bias == 0.75);
    assert(neuron.Activation == 0.25);

    assert(MiaIAClient::TryGetConnection(1, connection));
    assert(connection.Id == 1);
    assert(connection.FromNeuron == 1001);
    assert(connection.ToNeuron == 2001);
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

    assert(neuron.Id == 1001);
    assert(connection.Id == 1);
    assert(layer.Id == 1);

    const auto afterInspection = MiaIAClient::GetSnapshot();

    assert(afterInspection.Layers.size() == 2);
    assert(afterInspection.Connections.size() == 1);

    assert(MiaIAClient::RemoveConnection(1));
    assert(!MiaIAClient::RemoveConnection(1));

    const auto afterConnectionRemoval = MiaIAClient::GetSnapshot();

    assert(afterConnectionRemoval.Connections.empty());

    });

    runner.Run("CSV dataset pipeline", [&]()
    {
    const std::filesystem::path headerPath =
        std::filesystem::temp_directory_path() /
        "miaia_dataset_header_test.csv";
    const std::filesystem::path noHeaderPath =
        std::filesystem::temp_directory_path() /
        "miaia_dataset_no_header_test.csv";
    const std::filesystem::path invalidPath =
        std::filesystem::temp_directory_path() /
        "miaia_dataset_invalid_test.csv";
    const std::filesystem::path nonFinitePath =
        std::filesystem::temp_directory_path() /
        "miaia_dataset_non_finite_test.csv";

    {
        std::ofstream output(headerPath);
        assert(output.good());
        output
            << "x1,x2,target\n"
            << "0,0,0\n"
            << "0,1,1\n"
            << "1,0,1\n"
            << "1,1,0\n";
    }

    {
        std::ofstream output(noHeaderPath);
        assert(output.good());
        output
            << "0.25,-0.5,1\n"
            << "0.75,0.5,0\n";
    }

    {
        std::ofstream output(invalidPath);
        assert(output.good());
        output
            << "x1,x2,target\n"
            << "0,1\n";
    }

    {
        std::ofstream output(nonFinitePath);
        assert(output.good());
        output
            << "x1,x2,target\n"
            << "0,nan,1\n";
    }

    MiaIAClient::ClearDataset();
    MiaIAClient::ClearNetwork();

    assert(MiaIAClient::ImportCsvDataset(
        headerPath.string(),
        2,
        1));

    const auto summary = MiaIAClient::GetDatasetSummary();

    assert(summary.Name == "miaia_dataset_header_test");
    assert(summary.Source == headerPath.string());
    assert(summary.SampleCount == 4);
    assert(summary.InputCount == 2);
    assert(summary.TargetCount == 1);

    MiaIA::Core::SampleSnapshot sample;

    assert(MiaIAClient::TryGetDatasetSample(2, sample));
    assert(sample.Index == 2);
    assert(sample.Inputs.size() == 2);
    assert(sample.Targets.size() == 1);
    assert(sample.Inputs[0] == 1.0);
    assert(sample.Inputs[1] == 0.0);
    assert(sample.Targets[0] == 1.0);
    assert(!MiaIAClient::TryGetDatasetSample(4, sample));

    assert(MiaIAClient::CreateDenseNetwork(2, 2, 1, 1));
    assert(MiaIAClient::ApplyDatasetSample(2));

    const auto appliedSnapshot = MiaIAClient::GetSnapshot();

    assert(appliedSnapshot.Layers[0].Neurons[0].Activation == 1.0);
    assert(appliedSnapshot.Layers[0].Neurons[1].Activation == 0.0);

    assert(!MiaIAClient::ImportCsvDataset(
        invalidPath.string(),
        2,
        1));
    assert(!MiaIAClient::ImportCsvDataset(
        nonFinitePath.string(),
        2,
        1));
    assert(!MiaIAClient::ImportCsvDataset("", 2, 1));
    assert(!MiaIAClient::ImportCsvDataset(
        headerPath.string(),
        0,
        1));

    const auto afterFailedImport = MiaIAClient::GetDatasetSummary();

    assert(afterFailedImport.Source == summary.Source);
    assert(afterFailedImport.SampleCount == summary.SampleCount);

    assert(MiaIAClient::ImportCsvDataset(
        noHeaderPath.string(),
        2,
        1,
        false));

    const auto noHeaderSummary = MiaIAClient::GetDatasetSummary();

    assert(noHeaderSummary.SampleCount == 2);

    assert(MiaIAClient::CreateDenseNetwork(3, 2, 1, 1));
    assert(MiaIAClient::SetInputValues({ 0.1, 0.2, 0.3 }));
    assert(!MiaIAClient::ApplyDatasetSample(0));

    const auto afterFailedApply = MiaIAClient::GetSnapshot();

    assert(afterFailedApply.Layers[0].Neurons[0].Activation == 0.1);
    assert(afterFailedApply.Layers[0].Neurons[1].Activation == 0.2);
    assert(afterFailedApply.Layers[0].Neurons[2].Activation == 0.3);

    MiaIAClient::ClearDataset();

    const auto clearedSummary = MiaIAClient::GetDatasetSummary();

    assert(clearedSummary.SampleCount == 0);
    assert(!MiaIAClient::TryGetDatasetSample(0, sample));
    assert(!MiaIAClient::ApplyDatasetSample(0));

    std::filesystem::remove(headerPath);
    std::filesystem::remove(noHeaderPath);
    std::filesystem::remove(invalidPath);
    std::filesystem::remove(nonFinitePath);

    });

    runner.Run("Dataset sample evaluation", [&]()
    {
    const std::filesystem::path evaluationPath =
        std::filesystem::temp_directory_path() /
        "miaia_dataset_evaluation_test.csv";
    const std::filesystem::path incompatiblePath =
        std::filesystem::temp_directory_path() /
        "miaia_dataset_evaluation_incompatible_test.csv";

    {
        std::ofstream output(evaluationPath);
        assert(output.good());
        output
            << "x1,x2,target1,target2\n"
            << "0.5,-1,1,2\n";
    }

    {
        std::ofstream output(incompatiblePath);
        assert(output.good());
        output
            << "x1,x2,target\n"
            << "0.5,-1,1\n";
    }

    MiaIAClient::ClearDataset();
    MiaIAClient::ClearNetwork();

    assert(MiaIAClient::ImportCsvDataset(
        evaluationPath.string(),
        2,
        2));
    assert(MiaIAClient::CreateDenseNetwork(2, 2, 0, 2));
    assert(MiaIAClient::SetLayerActivation(
        1,
        MiaIA::Core::ActivationType::Linear));

    assert(MiaIAClient::SetConnectionWeight(1, 2.0));
    assert(MiaIAClient::SetConnectionWeight(2, -1.0));
    assert(MiaIAClient::SetConnectionWeight(3, 3.0));
    assert(MiaIAClient::SetConnectionWeight(4, 0.5));
    assert(MiaIAClient::SetNeuronBias(1003, 0.25));
    assert(MiaIAClient::SetNeuronBias(1004, -0.5));

    const auto beforeEvaluation = MiaIAClient::GetSnapshot();

    MiaIA::Core::SampleEvaluationSnapshot evaluation;

    assert(MiaIAClient::EvaluateDatasetSample(
        0,
        MiaIA::Core::LossType::MeanSquaredError,
        evaluation));

    assert(evaluation.SampleIndex == 0);
    assert(evaluation.Type == MiaIA::Core::LossType::MeanSquaredError);
    assert(evaluation.Targets.size() == 2);
    assert(evaluation.Predictions.size() == 2);
    assert(evaluation.Errors.size() == 2);
    assert(std::abs(evaluation.Targets[0] - 1.0) < 1e-12);
    assert(std::abs(evaluation.Targets[1] - 2.0) < 1e-12);
    assert(std::abs(evaluation.Predictions[0] - (-1.75)) < 1e-12);
    assert(std::abs(evaluation.Predictions[1] - (-1.5)) < 1e-12);
    assert(std::abs(evaluation.Errors[0] - (-2.75)) < 1e-12);
    assert(std::abs(evaluation.Errors[1] - (-3.5)) < 1e-12);
    assert(std::abs(evaluation.Loss - 9.90625) < 1e-12);

    const auto afterEvaluation = MiaIAClient::GetSnapshot();

    assert(afterEvaluation.Connections.size() ==
        beforeEvaluation.Connections.size());

    for (std::size_t index = 0;
        index < afterEvaluation.Connections.size();
        ++index)
    {
        assert(afterEvaluation.Connections[index].Weight ==
            beforeEvaluation.Connections[index].Weight);
    }

    assert(afterEvaluation.Layers[1].Neurons[0].Bias ==
        beforeEvaluation.Layers[1].Neurons[0].Bias);
    assert(afterEvaluation.Layers[1].Neurons[1].Bias ==
        beforeEvaluation.Layers[1].Neurons[1].Bias);

    const double successfulLoss = evaluation.Loss;

    assert(!MiaIAClient::EvaluateDatasetSample(
        1,
        MiaIA::Core::LossType::MeanSquaredError,
        evaluation));
    assert(evaluation.Loss == successfulLoss);

    assert(!MiaIAClient::EvaluateDatasetSample(
        0,
        static_cast<MiaIA::Core::LossType>(999),
        evaluation));
    assert(evaluation.Loss == successfulLoss);

    assert(MiaIAClient::ImportCsvDataset(
        incompatiblePath.string(),
        2,
        1));
    assert(MiaIAClient::SetInputValues({ 0.25, 0.75 }));

    const auto beforeIncompatibleEvaluation =
        MiaIAClient::GetSnapshot();

    assert(!MiaIAClient::EvaluateDatasetSample(
        0,
        MiaIA::Core::LossType::MeanSquaredError,
        evaluation));
    assert(evaluation.Loss == successfulLoss);

    const auto afterIncompatibleEvaluation =
        MiaIAClient::GetSnapshot();

    assert(afterIncompatibleEvaluation.Layers[0].Neurons[0].Activation ==
        beforeIncompatibleEvaluation.Layers[0].Neurons[0].Activation);
    assert(afterIncompatibleEvaluation.Layers[0].Neurons[1].Activation ==
        beforeIncompatibleEvaluation.Layers[0].Neurons[1].Activation);

    MiaIAClient::ClearDataset();
    MiaIAClient::ClearNetwork();

    std::filesystem::remove(evaluationPath);
    std::filesystem::remove(incompatiblePath);

    });

    runner.Run("Fixed-model dataset evaluation", [&]()
    {
    const std::filesystem::path datasetEvaluationPath =
        std::filesystem::temp_directory_path() /
        "miaia_full_dataset_evaluation_test.csv";
    const std::filesystem::path failingEvaluationPath =
        std::filesystem::temp_directory_path() /
        "miaia_full_dataset_evaluation_failure_test.csv";

    {
        std::ofstream output(datasetEvaluationPath);
        assert(output.good());
        output
            << "x,target\n"
            << "1,1\n"
            << "2,0\n";
    }

    {
        std::ofstream output(failingEvaluationPath);
        assert(output.good());
        output
            << "x,target\n"
            << "1,1\n"
            << "1e308,-1e308\n";
    }

    MiaIAClient::ClearDataset();
    MiaIAClient::ClearNetwork();

    assert(MiaIAClient::ImportCsvDataset(
        datasetEvaluationPath.string(),
        1,
        1));
    assert(MiaIAClient::CreateDenseNetwork(1, 1, 0, 1));
    assert(MiaIAClient::SetLayerActivation(
        1,
        MiaIA::Core::ActivationType::Linear));
    assert(MiaIAClient::SetConnectionWeight(1, 0.5));
    assert(MiaIAClient::SetNeuronBias(1002, 0.25));
    assert(MiaIAClient::SetInputValues({ 0.33 }));
    assert(MiaIAClient::Forward());

    const auto beforeDatasetEvaluation = MiaIAClient::GetSnapshot();
    MiaIA::Core::DatasetEvaluationSnapshot evaluation;

    assert(MiaIAClient::EvaluateDataset(
        MiaIA::Core::LossType::MeanSquaredError,
        evaluation));

    assert(evaluation.SampleCount == 2);
    assert(evaluation.Type == MiaIA::Core::LossType::MeanSquaredError);
    assert(evaluation.Evaluations.size() == 2);
    assert(std::abs(evaluation.MeanLoss - 0.8125) < 1e-12);

    assert(evaluation.Evaluations[0].SampleIndex == 0);
    assert(std::abs(evaluation.Evaluations[0].Predictions[0] - 0.75) <
        1e-12);
    assert(std::abs(evaluation.Evaluations[0].Loss - 0.0625) < 1e-12);

    assert(evaluation.Evaluations[1].SampleIndex == 1);
    assert(std::abs(evaluation.Evaluations[1].Predictions[0] - 1.25) <
        1e-12);
    assert(std::abs(evaluation.Evaluations[1].Loss - 1.5625) < 1e-12);

    const auto afterDatasetEvaluation = MiaIAClient::GetSnapshot();

    assert(afterDatasetEvaluation.Connections[0].Weight ==
        beforeDatasetEvaluation.Connections[0].Weight);
    assert(afterDatasetEvaluation.Layers[0].Neurons[0].Bias ==
        beforeDatasetEvaluation.Layers[0].Neurons[0].Bias);
    assert(afterDatasetEvaluation.Layers[0].Neurons[0].Activation ==
        beforeDatasetEvaluation.Layers[0].Neurons[0].Activation);
    assert(afterDatasetEvaluation.Layers[1].Neurons[0].Bias ==
        beforeDatasetEvaluation.Layers[1].Neurons[0].Bias);
    assert(afterDatasetEvaluation.Layers[1].Neurons[0].Activation ==
        beforeDatasetEvaluation.Layers[1].Neurons[0].Activation);

    MiaIA::Core::DatasetEvaluationSnapshot rejectedEvaluation;
    rejectedEvaluation.SampleCount = 999;
    rejectedEvaluation.MeanLoss = 42.0;

    const auto assertRejectedEvaluationPreserved = [&]()
    {
        assert(rejectedEvaluation.SampleCount == 999);
        assert(rejectedEvaluation.MeanLoss == 42.0);
        assert(rejectedEvaluation.Evaluations.empty());
    };

    assert(!MiaIAClient::EvaluateDataset(
        static_cast<MiaIA::Core::LossType>(999),
        rejectedEvaluation));
    assertRejectedEvaluationPreserved();

    MiaIAClient::ClearDataset();

    assert(!MiaIAClient::EvaluateDataset(
        MiaIA::Core::LossType::MeanSquaredError,
        rejectedEvaluation));
    assertRejectedEvaluationPreserved();

    assert(MiaIAClient::ImportCsvDataset(
        failingEvaluationPath.string(),
        1,
        1));

    const auto beforeRejectedEvaluation = MiaIAClient::GetSnapshot();

    assert(!MiaIAClient::EvaluateDataset(
        MiaIA::Core::LossType::MeanSquaredError,
        rejectedEvaluation));
    assertRejectedEvaluationPreserved();

    const auto afterRejectedEvaluation = MiaIAClient::GetSnapshot();

    assert(afterRejectedEvaluation.Connections[0].Weight ==
        beforeRejectedEvaluation.Connections[0].Weight);
    assert(afterRejectedEvaluation.Layers[0].Neurons[0].Bias ==
        beforeRejectedEvaluation.Layers[0].Neurons[0].Bias);
    assert(afterRejectedEvaluation.Layers[0].Neurons[0].Activation ==
        beforeRejectedEvaluation.Layers[0].Neurons[0].Activation);
    assert(afterRejectedEvaluation.Layers[1].Neurons[0].Bias ==
        beforeRejectedEvaluation.Layers[1].Neurons[0].Bias);
    assert(afterRejectedEvaluation.Layers[1].Neurons[0].Activation ==
        beforeRejectedEvaluation.Layers[1].Neurons[0].Activation);

    MiaIAClient::ClearDataset();
    MiaIAClient::ClearNetwork();
    std::filesystem::remove(datasetEvaluationPath);
    std::filesystem::remove(failingEvaluationPath);

    });

    runner.Run("Dataset sample gradients", [&]()
    {
    const std::filesystem::path analyticalPath =
        std::filesystem::temp_directory_path() /
        "miaia_dataset_gradient_analytical_test.csv";
    const std::filesystem::path numericalPath =
        std::filesystem::temp_directory_path() /
        "miaia_dataset_gradient_numerical_test.csv";

    {
        std::ofstream output(analyticalPath);
        assert(output.good());
        output
            << "x1,x2,target1,target2\n"
            << "0.5,-1,1,2\n";
    }

    {
        std::ofstream output(numericalPath);
        assert(output.good());
        output
            << "x1,x2,target\n"
            << "0.3,-0.2,0.7\n";
    }

    MiaIAClient::ClearDataset();
    MiaIAClient::ClearNetwork();

    assert(MiaIAClient::ImportCsvDataset(
        analyticalPath.string(),
        2,
        2));
    assert(MiaIAClient::CreateDenseNetwork(2, 2, 0, 2));
    assert(MiaIAClient::SetLayerActivation(
        1,
        MiaIA::Core::ActivationType::Linear));
    assert(MiaIAClient::SetConnectionWeight(1, 2.0));
    assert(MiaIAClient::SetConnectionWeight(2, -1.0));
    assert(MiaIAClient::SetConnectionWeight(3, 3.0));
    assert(MiaIAClient::SetConnectionWeight(4, 0.5));
    assert(MiaIAClient::SetNeuronBias(1003, 0.25));
    assert(MiaIAClient::SetNeuronBias(1004, -0.5));

    const auto beforeAnalyticalGradients =
        MiaIAClient::GetSnapshot();

    MiaIA::Core::SampleGradientSnapshot analyticalGradients;

    assert(MiaIAClient::EvaluateDatasetSampleGradients(
        0,
        MiaIA::Core::LossType::MeanSquaredError,
        analyticalGradients));

    assert(analyticalGradients.Neurons.size() == 4);
    assert(analyticalGradients.Connections.size() == 4);
    assert(std::abs(
        analyticalGradients.Evaluation.Loss - 9.90625) < 1e-12);

    assert(analyticalGradients.Neurons[0].Id == 1001);
    assert(analyticalGradients.Neurons[0].LayerOrder == 0);
    assert(std::abs(
        analyticalGradients.Neurons[0].ActivationGradient - (-2.0)) <
        1e-12);
    assert(std::abs(
        analyticalGradients.Neurons[1].ActivationGradient - (-10.0)) <
        1e-12);
    assert(std::abs(
        analyticalGradients.Neurons[2].BiasGradient - (-2.75)) <
        1e-12);
    assert(std::abs(
        analyticalGradients.Neurons[3].BiasGradient - (-3.5)) <
        1e-12);

    assert(analyticalGradients.Connections[0].Id == 1);
    assert(std::abs(
        analyticalGradients.Connections[0].WeightGradient - (-1.375)) <
        1e-12);
    assert(std::abs(
        analyticalGradients.Connections[1].WeightGradient - (-1.75)) <
        1e-12);
    assert(std::abs(
        analyticalGradients.Connections[2].WeightGradient - 2.75) <
        1e-12);
    assert(std::abs(
        analyticalGradients.Connections[3].WeightGradient - 3.5) <
        1e-12);

    const auto afterAnalyticalGradients =
        MiaIAClient::GetSnapshot();

    for (std::size_t index = 0;
        index < beforeAnalyticalGradients.Connections.size();
        ++index)
    {
        assert(beforeAnalyticalGradients.Connections[index].Weight ==
            afterAnalyticalGradients.Connections[index].Weight);
    }

    assert(beforeAnalyticalGradients.Layers[1].Neurons[0].Bias ==
        afterAnalyticalGradients.Layers[1].Neurons[0].Bias);
    assert(beforeAnalyticalGradients.Layers[1].Neurons[1].Bias ==
        afterAnalyticalGradients.Layers[1].Neurons[1].Bias);

    const double analyticalLoss =
        analyticalGradients.Evaluation.Loss;

    assert(!MiaIAClient::EvaluateDatasetSampleGradients(
        1,
        MiaIA::Core::LossType::MeanSquaredError,
        analyticalGradients));
    assert(analyticalGradients.Evaluation.Loss == analyticalLoss);

    assert(!MiaIAClient::EvaluateDatasetSampleGradients(
        0,
        static_cast<MiaIA::Core::LossType>(999),
        analyticalGradients));
    assert(analyticalGradients.Evaluation.Loss == analyticalLoss);

    assert(MiaIAClient::ImportCsvDataset(
        numericalPath.string(),
        2,
        1));
    assert(MiaIAClient::CreateDenseNetwork(2, 2, 1, 1));
    assert(MiaIAClient::SetLayerActivation(
        1,
        MiaIA::Core::ActivationType::Tanh));
    assert(MiaIAClient::SetLayerActivation(
        2,
        MiaIA::Core::ActivationType::Sigmoid));

    assert(MiaIAClient::SetConnectionWeight(1, 0.4));
    assert(MiaIAClient::SetConnectionWeight(2, -0.3));
    assert(MiaIAClient::SetConnectionWeight(3, 0.2));
    assert(MiaIAClient::SetConnectionWeight(4, 0.1));
    assert(MiaIAClient::SetConnectionWeight(5, -0.5));
    assert(MiaIAClient::SetConnectionWeight(6, 0.6));
    assert(MiaIAClient::SetNeuronBias(1003, 0.05));
    assert(MiaIAClient::SetNeuronBias(1004, -0.02));
    assert(MiaIAClient::SetNeuronBias(1005, 0.1));

    const auto beforeNumericalGradients =
        MiaIAClient::GetSnapshot();

    MiaIA::Core::SampleGradientSnapshot gradients;

    assert(MiaIAClient::EvaluateDatasetSampleGradients(
        0,
        MiaIA::Core::LossType::MeanSquaredError,
        gradients));

    const auto afterNumericalGradients =
        MiaIAClient::GetSnapshot();

    for (std::size_t index = 0;
        index < beforeNumericalGradients.Connections.size();
        ++index)
    {
        assert(beforeNumericalGradients.Connections[index].Weight ==
            afterNumericalGradients.Connections[index].Weight);
    }

    for (std::size_t layerIndex = 1;
        layerIndex < beforeNumericalGradients.Layers.size();
        ++layerIndex)
    {
        for (std::size_t neuronIndex = 0;
            neuronIndex <
                beforeNumericalGradients.Layers[layerIndex].Neurons.size();
            ++neuronIndex)
        {
            assert(
                beforeNumericalGradients.Layers[layerIndex]
                    .Neurons[neuronIndex].Bias ==
                afterNumericalGradients.Layers[layerIndex]
                    .Neurons[neuronIndex].Bias);
        }
    }

    const auto findConnectionGradient =
        [&](std::uint64_t id) -> const MiaIA::Core::ConnectionGradientSnapshot*
        {
            for (const auto& connection : gradients.Connections)
            {
                if (connection.Id == id)
                {
                    return &connection;
                }
            }

            return nullptr;
        };

    const auto findNeuronGradient =
        [&](std::uint64_t id) -> const MiaIA::Core::NeuronGradientSnapshot*
        {
            for (const auto& neuron : gradients.Neurons)
            {
                if (neuron.Id == id)
                {
                    return &neuron;
                }
            }

            return nullptr;
        };

    constexpr double Epsilon = 1e-6;
    constexpr double GradientTolerance = 1e-6;

    for (std::uint64_t connectionId = 1;
        connectionId <= 6;
        ++connectionId)
    {
        MiaIA::Core::ConnectionSnapshot connection;
        assert(MiaIAClient::TryGetConnection(connectionId, connection));

        MiaIA::Core::SampleEvaluationSnapshot plusEvaluation;
        MiaIA::Core::SampleEvaluationSnapshot minusEvaluation;

        assert(MiaIAClient::SetConnectionWeight(
            connectionId,
            connection.Weight + Epsilon));
        assert(MiaIAClient::EvaluateDatasetSample(
            0,
            MiaIA::Core::LossType::MeanSquaredError,
            plusEvaluation));

        assert(MiaIAClient::SetConnectionWeight(
            connectionId,
            connection.Weight - Epsilon));
        assert(MiaIAClient::EvaluateDatasetSample(
            0,
            MiaIA::Core::LossType::MeanSquaredError,
            minusEvaluation));

        assert(MiaIAClient::SetConnectionWeight(
            connectionId,
            connection.Weight));

        const double numericalGradient =
            (plusEvaluation.Loss - minusEvaluation.Loss) /
            (2.0 * Epsilon);

        const auto* analyticalGradient =
            findConnectionGradient(connectionId);

        assert(analyticalGradient != nullptr);
        assert(std::abs(
            analyticalGradient->WeightGradient - numericalGradient) <
            GradientTolerance);
    }

    for (const std::uint64_t neuronId :
        std::vector<std::uint64_t>{ 1003, 1004, 1005 })
    {
        MiaIA::Core::NeuronSnapshot neuron;
        assert(MiaIAClient::TryGetNeuron(neuronId, neuron));

        MiaIA::Core::SampleEvaluationSnapshot plusEvaluation;
        MiaIA::Core::SampleEvaluationSnapshot minusEvaluation;

        assert(MiaIAClient::SetNeuronBias(
            neuronId,
            neuron.Bias + Epsilon));
        assert(MiaIAClient::EvaluateDatasetSample(
            0,
            MiaIA::Core::LossType::MeanSquaredError,
            plusEvaluation));

        assert(MiaIAClient::SetNeuronBias(
            neuronId,
            neuron.Bias - Epsilon));
        assert(MiaIAClient::EvaluateDatasetSample(
            0,
            MiaIA::Core::LossType::MeanSquaredError,
            minusEvaluation));

        assert(MiaIAClient::SetNeuronBias(
            neuronId,
            neuron.Bias));

        const double numericalGradient =
            (plusEvaluation.Loss - minusEvaluation.Loss) /
            (2.0 * Epsilon);

        const auto* analyticalGradient =
            findNeuronGradient(neuronId);

        assert(analyticalGradient != nullptr);
        assert(std::abs(
            analyticalGradient->BiasGradient - numericalGradient) <
            GradientTolerance);
    }

    MiaIAClient::ClearDataset();
    MiaIAClient::ClearNetwork();

    std::filesystem::remove(analyticalPath);
    std::filesystem::remove(numericalPath);

    });

    runner.Run("Atomic SGD training step", [&]()
    {
    const std::filesystem::path trainingPath =
        std::filesystem::temp_directory_path() /
        "miaia_dataset_training_step_test.csv";

    {
        std::ofstream output(trainingPath);
        assert(output.good());
        output
            << "x1,x2,target1,target2\n"
            << "0.5,-1,1,2\n";
    }

    MiaIAClient::ClearDataset();
    MiaIAClient::ClearNetwork();

    assert(MiaIAClient::ImportCsvDataset(
        trainingPath.string(),
        2,
        2));
    assert(MiaIAClient::CreateDenseNetwork(2, 2, 0, 2));
    assert(MiaIAClient::SetLayerActivation(
        1,
        MiaIA::Core::ActivationType::Linear));
    assert(MiaIAClient::SetConnectionWeight(1, 2.0));
    assert(MiaIAClient::SetConnectionWeight(2, -1.0));
    assert(MiaIAClient::SetConnectionWeight(3, 3.0));
    assert(MiaIAClient::SetConnectionWeight(4, 0.5));
    assert(MiaIAClient::SetNeuronBias(1003, 0.25));
    assert(MiaIAClient::SetNeuronBias(1004, -0.5));

    MiaIA::Core::TrainingStepSnapshot step;

    assert(MiaIAClient::TrainDatasetSample(
        0,
        0.1,
        MiaIA::Core::LossType::MeanSquaredError,
        MiaIA::Core::OptimizerType::StochasticGradientDescent,
        step));

    assert(step.SampleIndex == 0);
    assert(std::abs(step.LearningRate - 0.1) < 1e-12);
    assert(step.Optimizer ==
        MiaIA::Core::OptimizerType::StochasticGradientDescent);
    assert(std::abs(step.Before.Evaluation.Loss - 9.90625) < 1e-12);
    assert(step.After.Loss < step.Before.Evaluation.Loss);
    assert(step.ConnectionUpdates.size() == 4);
    assert(step.NeuronUpdates.size() == 2);

    const std::vector<double> expectedWeightGradients{
        -1.375,
        -1.75,
        2.75,
        3.5
    };

    const std::vector<double> expectedWeightDeltas{
        0.1375,
        0.175,
        -0.275,
        -0.35
    };

    const std::vector<double> expectedUpdatedWeights{
        2.1375,
        -0.825,
        2.725,
        0.15
    };

    for (std::size_t index = 0;
        index < step.ConnectionUpdates.size();
        ++index)
    {
        const auto& update = step.ConnectionUpdates[index];

        assert(update.Id == index + 1);
        assert(std::abs(
            update.Gradient - expectedWeightGradients[index]) < 1e-12);
        assert(std::abs(
            update.Delta - expectedWeightDeltas[index]) < 1e-12);
        assert(std::abs(
            update.UpdatedWeight - expectedUpdatedWeights[index]) < 1e-12);
        assert(std::abs(
            update.UpdatedWeight -
            (update.PreviousWeight + update.Delta)) < 1e-12);
    }

    assert(step.NeuronUpdates[0].Id == 1003);
    assert(std::abs(step.NeuronUpdates[0].PreviousBias - 0.25) < 1e-12);
    assert(std::abs(step.NeuronUpdates[0].Gradient - (-2.75)) < 1e-12);
    assert(std::abs(step.NeuronUpdates[0].Delta - 0.275) < 1e-12);
    assert(std::abs(step.NeuronUpdates[0].UpdatedBias - 0.525) < 1e-12);

    assert(step.NeuronUpdates[1].Id == 1004);
    assert(std::abs(step.NeuronUpdates[1].PreviousBias - (-0.5)) < 1e-12);
    assert(std::abs(step.NeuronUpdates[1].Gradient - (-3.5)) < 1e-12);
    assert(std::abs(step.NeuronUpdates[1].Delta - 0.35) < 1e-12);
    assert(std::abs(step.NeuronUpdates[1].UpdatedBias - (-0.15)) < 1e-12);

    assert(std::abs(step.After.Predictions[0] - (-1.13125)) < 1e-12);
    assert(std::abs(step.After.Predictions[1] - (-0.7125)) < 1e-12);

    const auto afterStep = MiaIAClient::GetSnapshot();

    assert(afterStep.Layers[0].Neurons[0].Bias == 0.0);
    assert(afterStep.Layers[0].Neurons[1].Bias == 0.0);

    for (std::size_t index = 0;
        index < afterStep.Connections.size();
        ++index)
    {
        assert(std::abs(
            afterStep.Connections[index].Weight -
            expectedUpdatedWeights[index]) < 1e-12);
    }

    assert(std::abs(
        afterStep.Layers[1].Neurons[0].Bias - 0.525) < 1e-12);
    assert(std::abs(
        afterStep.Layers[1].Neurons[1].Bias - (-0.15)) < 1e-12);

    MiaIA::Core::TrainingStepSnapshot rejectedStep;
    rejectedStep.SampleIndex = 999;
    rejectedStep.LearningRate = 42.0;

    const auto assertRejectedStepPreserved = [&]()
    {
        assert(rejectedStep.SampleIndex == 999);
        assert(rejectedStep.LearningRate == 42.0);
    };

    assert(!MiaIAClient::TrainDatasetSample(
        1,
        0.1,
        MiaIA::Core::LossType::MeanSquaredError,
        MiaIA::Core::OptimizerType::StochasticGradientDescent,
        rejectedStep));
    assertRejectedStepPreserved();

    for (const double invalidLearningRate :
        std::vector<double>{
            0.0,
            -0.1,
            std::numeric_limits<double>::quiet_NaN(),
            std::numeric_limits<double>::infinity(),
            (std::numeric_limits<double>::max)()
        })
    {
        assert(!MiaIAClient::TrainDatasetSample(
            0,
            invalidLearningRate,
            MiaIA::Core::LossType::MeanSquaredError,
            MiaIA::Core::OptimizerType::StochasticGradientDescent,
            rejectedStep));
        assertRejectedStepPreserved();
    }

    assert(!MiaIAClient::TrainDatasetSample(
        0,
        0.1,
        static_cast<MiaIA::Core::LossType>(999),
        MiaIA::Core::OptimizerType::StochasticGradientDescent,
        rejectedStep));
    assertRejectedStepPreserved();

    assert(!MiaIAClient::TrainDatasetSample(
        0,
        0.1,
        MiaIA::Core::LossType::MeanSquaredError,
        static_cast<MiaIA::Core::OptimizerType>(999),
        rejectedStep));
    assertRejectedStepPreserved();

    const auto afterRejectedSteps = MiaIAClient::GetSnapshot();

    for (std::size_t index = 0;
        index < afterStep.Connections.size();
        ++index)
    {
        assert(afterRejectedSteps.Connections[index].Weight ==
            afterStep.Connections[index].Weight);
    }

    for (std::size_t layerIndex = 0;
        layerIndex < afterStep.Layers.size();
        ++layerIndex)
    {
        for (std::size_t neuronIndex = 0;
            neuronIndex < afterStep.Layers[layerIndex].Neurons.size();
            ++neuronIndex)
        {
            assert(
                afterRejectedSteps.Layers[layerIndex]
                    .Neurons[neuronIndex].Bias ==
                afterStep.Layers[layerIndex]
                    .Neurons[neuronIndex].Bias);
        }
    }

    MiaIAClient::ClearDataset();
    MiaIAClient::ClearNetwork();
    std::filesystem::remove(trainingPath);

    });

    runner.Run("Atomic dataset training epoch", [&]()
    {
    const std::filesystem::path epochPath =
        std::filesystem::temp_directory_path() /
        "miaia_dataset_training_epoch_test.csv";

    {
        std::ofstream output(epochPath);
        assert(output.good());
        output
            << "x,target\n"
            << "1,1\n"
            << "2,0\n";
    }

    MiaIAClient::ClearDataset();
    MiaIAClient::ClearNetwork();

    assert(MiaIAClient::ImportCsvDataset(
        epochPath.string(),
        1,
        1));
    assert(MiaIAClient::CreateDenseNetwork(1, 1, 0, 1));
    assert(MiaIAClient::SetLayerActivation(
        1,
        MiaIA::Core::ActivationType::Linear));
    assert(MiaIAClient::SetConnectionWeight(1, 0.0));
    assert(MiaIAClient::SetNeuronBias(1002, 0.0));

    MiaIA::Core::TrainingEpochSnapshot epoch;

    assert(MiaIAClient::TrainDatasetEpoch(
        0.1,
        MiaIA::Core::LossType::MeanSquaredError,
        MiaIA::Core::OptimizerType::StochasticGradientDescent,
        epoch));

    assert(epoch.SampleCount == 2);
    assert(std::abs(epoch.LearningRate - 0.1) < 1e-12);
    assert(epoch.Loss == MiaIA::Core::LossType::MeanSquaredError);
    assert(epoch.Optimizer ==
        MiaIA::Core::OptimizerType::StochasticGradientDescent);
    assert(epoch.Steps.size() == 2);
    assert(std::abs(epoch.MeanLossBeforeUpdate - 0.68) < 1e-12);
    assert(std::abs(epoch.MeanLossAfterUpdate - 0.18) < 1e-12);

    assert(epoch.Steps[0].SampleIndex == 0);
    assert(std::abs(
        epoch.Steps[0].Before.Evaluation.Loss - 1.0) < 1e-12);
    assert(std::abs(epoch.Steps[0].After.Loss - 0.36) < 1e-12);
    assert(std::abs(
        epoch.Steps[0].ConnectionUpdates[0].UpdatedWeight - 0.2) <
        1e-12);
    assert(std::abs(
        epoch.Steps[0].NeuronUpdates[0].UpdatedBias - 0.2) <
        1e-12);

    assert(epoch.Steps[1].SampleIndex == 1);
    assert(std::abs(
        epoch.Steps[1].Before.Evaluation.Loss - 0.36) < 1e-12);
    assert(std::abs(epoch.Steps[1].After.Loss) < 1e-12);
    assert(std::abs(
        epoch.Steps[1].ConnectionUpdates[0].UpdatedWeight - (-0.04)) <
        1e-12);
    assert(std::abs(
        epoch.Steps[1].NeuronUpdates[0].UpdatedBias - 0.08) <
        1e-12);

    const auto afterEpoch = MiaIAClient::GetSnapshot();

    assert(afterEpoch.Layers[0].Neurons[0].Bias == 0.0);
    assert(std::abs(afterEpoch.Connections[0].Weight - (-0.04)) < 1e-12);
    assert(std::abs(afterEpoch.Layers[1].Neurons[0].Bias - 0.08) < 1e-12);

    MiaIA::Core::TrainingEpochSnapshot rejectedEpoch;
    rejectedEpoch.SampleCount = 999;
    rejectedEpoch.LearningRate = 42.0;
    rejectedEpoch.MeanLossBeforeUpdate = 12.0;
    rejectedEpoch.MeanLossAfterUpdate = 10.0;

    const auto assertRejectedEpochPreserved = [&]()
    {
        assert(rejectedEpoch.SampleCount == 999);
        assert(rejectedEpoch.LearningRate == 42.0);
        assert(rejectedEpoch.MeanLossBeforeUpdate == 12.0);
        assert(rejectedEpoch.MeanLossAfterUpdate == 10.0);
        assert(rejectedEpoch.Steps.empty());
    };

    for (const double invalidLearningRate :
        std::vector<double>{
            0.0,
            -0.1,
            std::numeric_limits<double>::quiet_NaN(),
            std::numeric_limits<double>::infinity(),
            (std::numeric_limits<double>::max)()
        })
    {
        assert(!MiaIAClient::TrainDatasetEpoch(
            invalidLearningRate,
            MiaIA::Core::LossType::MeanSquaredError,
            MiaIA::Core::OptimizerType::StochasticGradientDescent,
            rejectedEpoch));
        assertRejectedEpochPreserved();
    }

    assert(!MiaIAClient::TrainDatasetEpoch(
        0.1,
        static_cast<MiaIA::Core::LossType>(999),
        MiaIA::Core::OptimizerType::StochasticGradientDescent,
        rejectedEpoch));
    assertRejectedEpochPreserved();

    assert(!MiaIAClient::TrainDatasetEpoch(
        0.1,
        MiaIA::Core::LossType::MeanSquaredError,
        static_cast<MiaIA::Core::OptimizerType>(999),
        rejectedEpoch));
    assertRejectedEpochPreserved();

    MiaIAClient::ClearDataset();

    assert(!MiaIAClient::TrainDatasetEpoch(
        0.1,
        MiaIA::Core::LossType::MeanSquaredError,
        MiaIA::Core::OptimizerType::StochasticGradientDescent,
        rejectedEpoch));
    assertRejectedEpochPreserved();

    const std::filesystem::path failingEpochPath =
        std::filesystem::temp_directory_path() /
        "miaia_dataset_training_epoch_rollback_test.csv";

    {
        std::ofstream output(failingEpochPath);
        assert(output.good());
        output
            << "x,target\n"
            << "1,1\n"
            << "1e308,-1e308\n";
    }

    assert(MiaIAClient::ImportCsvDataset(
        failingEpochPath.string(),
        1,
        1));
    assert(MiaIAClient::SetConnectionWeight(1, 0.0));
    assert(MiaIAClient::SetNeuronBias(1002, 0.0));

    const auto beforeRejectedEpoch = MiaIAClient::GetSnapshot();

    assert(!MiaIAClient::TrainDatasetEpoch(
        0.1,
        MiaIA::Core::LossType::MeanSquaredError,
        MiaIA::Core::OptimizerType::StochasticGradientDescent,
        rejectedEpoch));
    assertRejectedEpochPreserved();

    const auto afterRejectedEpoch = MiaIAClient::GetSnapshot();

    assert(afterRejectedEpoch.Connections[0].Weight ==
        beforeRejectedEpoch.Connections[0].Weight);
    assert(afterRejectedEpoch.Layers[0].Neurons[0].Bias ==
        beforeRejectedEpoch.Layers[0].Neurons[0].Bias);
    assert(afterRejectedEpoch.Layers[0].Neurons[0].Activation ==
        beforeRejectedEpoch.Layers[0].Neurons[0].Activation);
    assert(afterRejectedEpoch.Layers[1].Neurons[0].Bias ==
        beforeRejectedEpoch.Layers[1].Neurons[0].Bias);
    assert(afterRejectedEpoch.Layers[1].Neurons[0].Activation ==
        beforeRejectedEpoch.Layers[1].Neurons[0].Activation);

    MiaIAClient::ClearDataset();
    MiaIAClient::ClearNetwork();
    std::filesystem::remove(epochPath);
    std::filesystem::remove(failingEpochPath);

    });

    runner.Run("Controlled training session", [&]()
    {
    const std::filesystem::path sessionPath =
        std::filesystem::temp_directory_path() /
        "miaia_controlled_training_session_test.csv";

    {
        std::ofstream output(sessionPath);
        assert(output.good());
        output
            << "x,target\n"
            << "1,1\n"
            << "2,0\n";
    }

    MiaIAClient::ClearDataset();
    MiaIAClient::ClearNetwork();

    assert(MiaIAClient::ImportCsvDataset(
        sessionPath.string(),
        1,
        1));
    assert(MiaIAClient::CreateDenseNetwork(1, 1, 0, 1));
    assert(MiaIAClient::SetLayerActivation(
        1,
        MiaIA::Core::ActivationType::Linear));
    assert(MiaIAClient::SetConnectionWeight(1, 0.0));
    assert(MiaIAClient::SetNeuronBias(1002, 0.0));

    MiaIA::Core::TrainingSessionSnapshot session;

    assert(MiaIAClient::StartTrainingSession(
        2,
        0.1,
        MiaIA::Core::LossType::MeanSquaredError,
        MiaIA::Core::OptimizerType::StochasticGradientDescent,
        session));

    assert(session.Status == MiaIA::Core::TrainingSessionStatus::Active);
    assert(session.EpochCount == 2);
    assert(session.CurrentEpoch == 0);
    assert(session.NextSampleIndex == 0);
    assert(session.SampleCount == 2);
    assert(session.CompletedSteps == 0);
    assert(session.TotalSteps == 4);
    assert(std::abs(session.LearningRate - 0.1) < 1e-12);
    assert(session.Steps.empty());

    MiaIA::Core::TrainingSessionSnapshot rejectedSession;
    rejectedSession.EpochCount = 999;
    rejectedSession.LearningRate = 42.0;

    assert(!MiaIAClient::StartTrainingSession(
        1,
        0.1,
        MiaIA::Core::LossType::MeanSquaredError,
        MiaIA::Core::OptimizerType::StochasticGradientDescent,
        rejectedSession));
    assert(rejectedSession.EpochCount == 999);
    assert(rejectedSession.LearningRate == 42.0);

    MiaIA::Core::TrainingStepSnapshot step;

    assert(MiaIAClient::AdvanceTrainingSession(step));
    assert(step.SampleIndex == 0);
    assert(std::abs(step.Before.Evaluation.Loss - 1.0) < 1e-12);
    assert(std::abs(step.After.Loss - 0.36) < 1e-12);

    session = MiaIAClient::GetTrainingSession();
    assert(session.Status == MiaIA::Core::TrainingSessionStatus::Active);
    assert(session.CurrentEpoch == 0);
    assert(session.NextSampleIndex == 1);
    assert(session.CompletedSteps == 1);
    assert(session.Steps.size() == 1);

    assert(MiaIAClient::AdvanceTrainingSession(step));
    assert(step.SampleIndex == 1);
    assert(std::abs(step.Before.Evaluation.Loss - 0.36) < 1e-12);
    assert(std::abs(step.After.Loss) < 1e-12);

    session = MiaIAClient::GetTrainingSession();
    assert(session.Status == MiaIA::Core::TrainingSessionStatus::Active);
    assert(session.CurrentEpoch == 1);
    assert(session.NextSampleIndex == 0);
    assert(session.CompletedSteps == 2);

    assert(MiaIAClient::AdvanceTrainingSession(step));
    assert(step.SampleIndex == 0);
    assert(std::abs(step.Before.Evaluation.Loss - 0.9216) < 1e-12);
    assert(std::abs(step.After.Loss - 0.331776) < 1e-12);

    assert(MiaIAClient::AdvanceTrainingSession(step));
    assert(step.SampleIndex == 1);
    assert(std::abs(step.Before.Evaluation.Loss - 0.331776) < 1e-12);
    assert(std::abs(step.After.Loss) < 1e-12);

    session = MiaIAClient::GetTrainingSession();
    assert(session.Status ==
        MiaIA::Core::TrainingSessionStatus::Completed);
    assert(session.CurrentEpoch == 2);
    assert(session.NextSampleIndex == 0);
    assert(session.CompletedSteps == 4);
    assert(session.TotalSteps == 4);
    assert(session.Steps.size() == 4);
    assert(session.Steps[0].SampleIndex == 0);
    assert(session.Steps[1].SampleIndex == 1);
    assert(session.Steps[2].SampleIndex == 0);
    assert(session.Steps[3].SampleIndex == 1);

    const auto afterCompletedSession = MiaIAClient::GetSnapshot();
    assert(std::abs(
        afterCompletedSession.Connections[0].Weight - (-0.0784)) <
        1e-12);
    assert(std::abs(
        afterCompletedSession.Layers[1].Neurons[0].Bias - 0.1568) <
        1e-12);

    MiaIA::Core::TrainingStepSnapshot rejectedStep;
    rejectedStep.SampleIndex = 999;
    rejectedStep.LearningRate = 42.0;

    assert(!MiaIAClient::AdvanceTrainingSession(rejectedStep));
    assert(rejectedStep.SampleIndex == 999);
    assert(rejectedStep.LearningRate == 42.0);
    assert(!MiaIAClient::CancelTrainingSession());

    assert(MiaIAClient::StartTrainingSession(
        2,
        0.1,
        MiaIA::Core::LossType::MeanSquaredError,
        MiaIA::Core::OptimizerType::StochasticGradientDescent,
        session));
    assert(MiaIAClient::AdvanceTrainingSession(step));

    const auto beforeCancellation = MiaIAClient::GetSnapshot();
    assert(MiaIAClient::CancelTrainingSession());

    session = MiaIAClient::GetTrainingSession();
    assert(session.Status ==
        MiaIA::Core::TrainingSessionStatus::Cancelled);
    assert(session.CompletedSteps == 1);
    assert(session.CurrentEpoch == 0);
    assert(session.NextSampleIndex == 1);
    assert(!MiaIAClient::AdvanceTrainingSession(rejectedStep));

    const auto afterCancellation = MiaIAClient::GetSnapshot();
    assert(afterCancellation.Connections[0].Weight ==
        beforeCancellation.Connections[0].Weight);
    assert(afterCancellation.Layers[1].Neurons[0].Bias ==
        beforeCancellation.Layers[1].Neurons[0].Bias);

    assert(MiaIAClient::StartTrainingSession(
        1,
        0.1,
        MiaIA::Core::LossType::MeanSquaredError,
        MiaIA::Core::OptimizerType::StochasticGradientDescent,
        session));

    MiaIAClient::ClearNetwork();
    const auto beforeIncompatibleStep = MiaIAClient::GetTrainingSession();

    assert(!MiaIAClient::AdvanceTrainingSession(rejectedStep));
    assert(rejectedStep.SampleIndex == 999);
    assert(rejectedStep.LearningRate == 42.0);

    const auto afterIncompatibleStep = MiaIAClient::GetTrainingSession();
    assert(afterIncompatibleStep.Status ==
        MiaIA::Core::TrainingSessionStatus::Active);
    assert(afterIncompatibleStep.CurrentEpoch ==
        beforeIncompatibleStep.CurrentEpoch);
    assert(afterIncompatibleStep.NextSampleIndex ==
        beforeIncompatibleStep.NextSampleIndex);
    assert(afterIncompatibleStep.CompletedSteps ==
        beforeIncompatibleStep.CompletedSteps);
    assert(MiaIAClient::CancelTrainingSession());

    assert(MiaIAClient::CreateDenseNetwork(1, 1, 0, 1));
    assert(MiaIAClient::SetLayerActivation(
        1,
        MiaIA::Core::ActivationType::Linear));

    for (const double invalidLearningRate :
        std::vector<double>{
            0.0,
            -0.1,
            std::numeric_limits<double>::quiet_NaN(),
            std::numeric_limits<double>::infinity()
        })
    {
        assert(!MiaIAClient::StartTrainingSession(
            1,
            invalidLearningRate,
            MiaIA::Core::LossType::MeanSquaredError,
            MiaIA::Core::OptimizerType::StochasticGradientDescent,
            rejectedSession));
        assert(rejectedSession.EpochCount == 999);
        assert(rejectedSession.LearningRate == 42.0);
    }

    assert(!MiaIAClient::StartTrainingSession(
        0,
        0.1,
        MiaIA::Core::LossType::MeanSquaredError,
        MiaIA::Core::OptimizerType::StochasticGradientDescent,
        rejectedSession));
    assert(!MiaIAClient::StartTrainingSession(
        1,
        0.1,
        static_cast<MiaIA::Core::LossType>(999),
        MiaIA::Core::OptimizerType::StochasticGradientDescent,
        rejectedSession));
    assert(!MiaIAClient::StartTrainingSession(
        1,
        0.1,
        MiaIA::Core::LossType::MeanSquaredError,
        static_cast<MiaIA::Core::OptimizerType>(999),
        rejectedSession));
    assert(rejectedSession.EpochCount == 999);
    assert(rejectedSession.LearningRate == 42.0);

    MiaIAClient::ClearDataset();

    assert(!MiaIAClient::StartTrainingSession(
        1,
        0.1,
        MiaIA::Core::LossType::MeanSquaredError,
        MiaIA::Core::OptimizerType::StochasticGradientDescent,
        rejectedSession));

    MiaIAClient::ClearNetwork();
    std::filesystem::remove(sessionPath);

    });

    runner.Run("Bounded training session run", [&]()
    {
    const std::filesystem::path runPath =
        std::filesystem::temp_directory_path() /
        "miaia_bounded_training_session_run_test.csv";
    const std::filesystem::path failingRunPath =
        std::filesystem::temp_directory_path() /
        "miaia_bounded_training_session_run_failure_test.csv";

    {
        std::ofstream output(runPath);
        assert(output.good());
        output
            << "x,target\n"
            << "1,1\n"
            << "2,0\n";
    }

    {
        std::ofstream output(failingRunPath);
        assert(output.good());
        output
            << "x,target\n"
            << "1,1\n"
            << "1e308,-1e308\n";
    }

    MiaIAClient::ClearDataset();
    MiaIAClient::ClearNetwork();

    assert(MiaIAClient::ImportCsvDataset(
        runPath.string(),
        1,
        1));
    assert(MiaIAClient::CreateDenseNetwork(1, 1, 0, 1));
    assert(MiaIAClient::SetLayerActivation(
        1,
        MiaIA::Core::ActivationType::Linear));
    assert(MiaIAClient::SetConnectionWeight(1, 0.0));
    assert(MiaIAClient::SetNeuronBias(1002, 0.0));

    MiaIA::Core::TrainingSessionSnapshot session;
    assert(MiaIAClient::StartTrainingSession(
        2,
        0.1,
        MiaIA::Core::LossType::MeanSquaredError,
        MiaIA::Core::OptimizerType::StochasticGradientDescent,
        session));

    MiaIA::Core::TrainingRunSnapshot run;
    assert(MiaIAClient::RunTrainingSession(3, run));

    assert(run.RequestedSteps == 3);
    assert(run.ExecutedSteps == 3);
    assert(run.StartEpoch == 0);
    assert(run.StartSampleIndex == 0);
    assert(run.EndEpoch == 1);
    assert(run.EndSampleIndex == 1);
    assert(run.StopReason ==
        MiaIA::Core::TrainingRunStopReason::StepLimitReached);
    assert(run.Steps.size() == 3);
    assert(run.Steps[0].SampleIndex == 0);
    assert(run.Steps[1].SampleIndex == 1);
    assert(run.Steps[2].SampleIndex == 0);
    assert(std::abs(
        run.MeanLossBeforeUpdate -
        ((1.0 + 0.36 + 0.9216) / 3.0)) < 1e-12);
    assert(std::abs(
        run.MeanLossAfterUpdate -
        ((0.36 + 0.0 + 0.331776) / 3.0)) < 1e-12);

    session = MiaIAClient::GetTrainingSession();
    assert(session.Status == MiaIA::Core::TrainingSessionStatus::Active);
    assert(session.CurrentEpoch == 1);
    assert(session.NextSampleIndex == 1);
    assert(session.CompletedSteps == 3);

    assert(MiaIAClient::RunTrainingSession(10, run));
    assert(run.RequestedSteps == 10);
    assert(run.ExecutedSteps == 1);
    assert(run.StartEpoch == 1);
    assert(run.StartSampleIndex == 1);
    assert(run.EndEpoch == 2);
    assert(run.EndSampleIndex == 0);
    assert(run.StopReason ==
        MiaIA::Core::TrainingRunStopReason::SessionCompleted);
    assert(run.Steps.size() == 1);
    assert(run.Steps[0].SampleIndex == 1);
    assert(std::abs(run.MeanLossBeforeUpdate - 0.331776) < 1e-12);
    assert(std::abs(run.MeanLossAfterUpdate) < 1e-12);

    session = MiaIAClient::GetTrainingSession();
    assert(session.Status ==
        MiaIA::Core::TrainingSessionStatus::Completed);
    assert(session.CompletedSteps == 4);

    MiaIA::Core::TrainingRunSnapshot rejectedRun;
    rejectedRun.RequestedSteps = 999;
    rejectedRun.ExecutedSteps = 888;
    rejectedRun.MeanLossBeforeUpdate = 42.0;

    assert(!MiaIAClient::RunTrainingSession(1, rejectedRun));
    assert(rejectedRun.RequestedSteps == 999);
    assert(rejectedRun.ExecutedSteps == 888);
    assert(rejectedRun.MeanLossBeforeUpdate == 42.0);

    assert(MiaIAClient::ImportCsvDataset(
        failingRunPath.string(),
        1,
        1));
    assert(MiaIAClient::SetConnectionWeight(1, 0.0));
    assert(MiaIAClient::SetNeuronBias(1002, 0.0));
    assert(MiaIAClient::StartTrainingSession(
        1,
        0.1,
        MiaIA::Core::LossType::MeanSquaredError,
        MiaIA::Core::OptimizerType::StochasticGradientDescent,
        session));

    assert(MiaIAClient::RunTrainingSession(2, run));
    assert(run.RequestedSteps == 2);
    assert(run.ExecutedSteps == 1);
    assert(run.StopReason ==
        MiaIA::Core::TrainingRunStopReason::StepFailed);
    assert(run.StartEpoch == 0);
    assert(run.StartSampleIndex == 0);
    assert(run.EndEpoch == 0);
    assert(run.EndSampleIndex == 1);
    assert(run.Steps.size() == 1);
    assert(run.Steps[0].SampleIndex == 0);

    session = MiaIAClient::GetTrainingSession();
    assert(session.Status == MiaIA::Core::TrainingSessionStatus::Active);
    assert(session.CurrentEpoch == 0);
    assert(session.NextSampleIndex == 1);
    assert(session.CompletedSteps == 1);

    const auto afterPartialFailure = MiaIAClient::GetSnapshot();
    assert(std::abs(afterPartialFailure.Connections[0].Weight - 0.2) <
        1e-12);
    assert(std::abs(
        afterPartialFailure.Layers[1].Neurons[0].Bias - 0.2) <
        1e-12);
    assert(MiaIAClient::CancelTrainingSession());

    assert(MiaIAClient::ImportCsvDataset(
        runPath.string(),
        1,
        1));
    assert(MiaIAClient::StartTrainingSession(
        1,
        0.1,
        MiaIA::Core::LossType::MeanSquaredError,
        MiaIA::Core::OptimizerType::StochasticGradientDescent,
        session));

    assert(!MiaIAClient::RunTrainingSession(0, rejectedRun));
    assert(rejectedRun.RequestedSteps == 999);
    assert(rejectedRun.ExecutedSteps == 888);
    assert(rejectedRun.MeanLossBeforeUpdate == 42.0);
    assert(MiaIAClient::CancelTrainingSession());

    MiaIAClient::ClearDataset();
    MiaIAClient::ClearNetwork();
    std::filesystem::remove(runPath);
    std::filesystem::remove(failingRunPath);

    });

    runner.Run("Background training pause and resume", [&]()
    {
    const std::filesystem::path backgroundPath =
        std::filesystem::temp_directory_path() /
        "miaia_background_training_test.csv";
    const std::filesystem::path failingBackgroundPath =
        std::filesystem::temp_directory_path() /
        "miaia_background_training_failure_test.csv";

    {
        std::ofstream output(backgroundPath);
        assert(output.good());
        output
            << "x,target\n"
            << "1,1\n"
            << "2,0\n";
    }

    {
        std::ofstream output(failingBackgroundPath);
        assert(output.good());
        output
            << "x,target\n"
            << "1e308,-1e308\n";
    }

    assert(MiaIAClient::ClearDataset());
    assert(MiaIAClient::ClearNetwork());
    assert(MiaIAClient::ImportCsvDataset(
        backgroundPath.string(),
        1,
        1));
    assert(MiaIAClient::CreateDenseNetwork(1, 1, 0, 1));
    assert(MiaIAClient::SetLayerActivation(
        1,
        MiaIA::Core::ActivationType::Linear));
    assert(MiaIAClient::SetConnectionWeight(1, 0.0));
    assert(MiaIAClient::SetNeuronBias(1002, 0.0));

    MiaIA::Core::TrainingSessionSnapshot session;
    assert(MiaIAClient::StartTrainingSession(
        1,
        0.1,
        MiaIA::Core::LossType::MeanSquaredError,
        MiaIA::Core::OptimizerType::StochasticGradientDescent,
        session));
    assert(MiaIAClient::ResumeTrainingSession());

    for (std::size_t attempt = 0; attempt < 100000; ++attempt)
    {
        session = MiaIAClient::GetTrainingSession();

        if (session.Status !=
            MiaIA::Core::TrainingSessionStatus::Running)
        {
            break;
        }

        std::this_thread::yield();
    }

    assert(session.Status ==
        MiaIA::Core::TrainingSessionStatus::Completed);
    assert(session.CompletedSteps == 2);
    assert(session.WorkerStopReason ==
        MiaIA::Core::TrainingWorkerStopReason::None);
    assert(!MiaIAClient::PauseTrainingSession());

    assert(MiaIAClient::SetConnectionWeight(1, 0.0));
    assert(MiaIAClient::SetNeuronBias(1002, 0.0));
    assert(MiaIAClient::StartTrainingSession(
        1000000,
        0.001,
        MiaIA::Core::LossType::MeanSquaredError,
        MiaIA::Core::OptimizerType::StochasticGradientDescent,
        session));
    assert(MiaIAClient::ResumeTrainingSession());

    session = MiaIAClient::GetTrainingSession();
    assert(session.Status ==
        MiaIA::Core::TrainingSessionStatus::Running);

    const auto runningNetwork = MiaIAClient::GetSnapshot();
    const auto runningDataset = MiaIAClient::GetDatasetSummary();
    MiaIA::Core::SampleSnapshot runningSample;
    MiaIA::Core::PredictionSnapshot runningPrediction;

    assert(!runningNetwork.Layers.empty());
    assert(runningDataset.SampleCount == 2);
    assert(MiaIAClient::TryGetDatasetSample(0, runningSample));
    assert(!MiaIAClient::SetConnectionWeight(1, 0.5));
    assert(!MiaIAClient::ClearNetwork());
    assert(!MiaIAClient::ClearDataset());
    assert(!MiaIAClient::Predict(
        { 1.0 },
        runningPrediction));

    assert(MiaIAClient::PauseTrainingSession());

    session = MiaIAClient::GetTrainingSession();
    assert(session.Status == MiaIA::Core::TrainingSessionStatus::Active);
    assert(session.CompletedSteps < session.TotalSteps);
    assert(session.WorkerStopReason ==
        MiaIA::Core::TrainingWorkerStopReason::PauseRequested);
    assert(MiaIAClient::SetConnectionWeight(1, 0.5));

    assert(MiaIAClient::ResumeTrainingSession());
    assert(MiaIAClient::CancelTrainingSession());

    session = MiaIAClient::GetTrainingSession();
    assert(session.Status ==
        MiaIA::Core::TrainingSessionStatus::Cancelled);
    assert(session.WorkerStopReason ==
        MiaIA::Core::TrainingWorkerStopReason::CancelRequested);

    assert(MiaIAClient::ImportCsvDataset(
        failingBackgroundPath.string(),
        1,
        1));
    assert(MiaIAClient::SetConnectionWeight(1, 0.5));
    assert(MiaIAClient::SetNeuronBias(1002, 0.0));
    assert(MiaIAClient::StartTrainingSession(
        1,
        0.1,
        MiaIA::Core::LossType::MeanSquaredError,
        MiaIA::Core::OptimizerType::StochasticGradientDescent,
        session));
    assert(MiaIAClient::ResumeTrainingSession());

    for (std::size_t attempt = 0; attempt < 100000; ++attempt)
    {
        session = MiaIAClient::GetTrainingSession();

        if (session.Status !=
            MiaIA::Core::TrainingSessionStatus::Running)
        {
            break;
        }

        std::this_thread::yield();
    }

    assert(session.Status == MiaIA::Core::TrainingSessionStatus::Active);
    assert(session.CompletedSteps == 0);
    assert(session.WorkerStopReason ==
        MiaIA::Core::TrainingWorkerStopReason::StepFailed);
    assert(MiaIAClient::CancelTrainingSession());

    assert(MiaIAClient::ClearDataset());
    assert(MiaIAClient::ClearNetwork());
    std::filesystem::remove(backgroundPath);
    std::filesystem::remove(failingBackgroundPath);

    });

    runner.Run("Training breakpoints", [&]()
    {
    assert(MiaIAClient::ClearTrainingBreakpoints());
    MiaIAClient::ClearDataset();
    MiaIAClient::ClearNetwork();
    assert(MiaIAClient::CreateDenseNetwork(2, 2, 1, 1));

    const auto testDirectory =
        std::filesystem::temp_directory_path() /
        "miaia_training_breakpoint_tests";
    const auto datasetPath = testDirectory / "samples.csv";
    std::filesystem::create_directories(testDirectory);

    {
        std::ofstream dataset(datasetPath);
        dataset << "x1,x2,y\n0,0,0\n1,1,1\n";
    }

    assert(MiaIAClient::ImportCsvDataset(
        datasetPath.string(),
        2,
        1));

    MiaIA::Core::TrainingBreakpointSpec invalidGradient;
    invalidGradient.Kind = MiaIA::Core::TrainingBreakpointKind::
        NeuronGradientMagnitudeAbove;
    invalidGradient.Threshold = 0.1;
    MiaIA::Core::TrainingBreakpointSnapshot breakpoint;
    assert(!MiaIAClient::AddTrainingBreakpoint(
        invalidGradient,
        breakpoint));

    MiaIA::Core::TrainingBreakpointSpec committedPhase;
    committedPhase.Kind = MiaIA::Core::TrainingBreakpointKind::Phase;
    committedPhase.Phase =
        MiaIA::Core::TrainingDebugPhase::Committed;
    assert(MiaIAClient::AddTrainingBreakpoint(
        committedPhase,
        breakpoint));
    const std::uint64_t committedBreakpointId = breakpoint.Id;

    MiaIA::Core::TrainingBreakpointSpec activation;
    activation.Kind = MiaIA::Core::TrainingBreakpointKind::
        NeuronActivationAbove;
    activation.TargetId = 1005;
    activation.Threshold = -1.0;
    MiaIA::Core::TrainingBreakpointSnapshot activationBreakpoint;
    assert(MiaIAClient::AddTrainingBreakpoint(
        activation,
        activationBreakpoint));
    assert(MiaIAClient::SetTrainingBreakpointEnabled(
        activationBreakpoint.Id,
        false));

    MiaIA::Core::TrainingBreakpointSpec gradient;
    gradient.Kind = MiaIA::Core::TrainingBreakpointKind::
        NeuronGradientMagnitudeAbove;
    gradient.TargetId = 1005;
    gradient.Threshold = 0.0;
    MiaIA::Core::TrainingBreakpointSnapshot gradientBreakpoint;
    assert(MiaIAClient::AddTrainingBreakpoint(
        gradient,
        gradientBreakpoint));
    assert(MiaIAClient::SetTrainingBreakpointEnabled(
        gradientBreakpoint.Id,
        false));

    MiaIA::Core::TrainingBreakpointSpec update;
    update.Kind = MiaIA::Core::TrainingBreakpointKind::
        ConnectionUpdateMagnitudeAbove;
    update.TargetId = 5;
    update.Threshold = 0.0;
    MiaIA::Core::TrainingBreakpointSnapshot updateBreakpoint;
    assert(MiaIAClient::AddTrainingBreakpoint(
        update,
        updateBreakpoint));
    assert(MiaIAClient::SetTrainingBreakpointEnabled(
        updateBreakpoint.Id,
        false));

    MiaIA::Core::TrainingSessionSnapshot session;
    assert(MiaIAClient::StartTrainingSession(
        2,
        0.1,
        MiaIA::Core::LossType::MeanSquaredError,
        MiaIA::Core::OptimizerType::StochasticGradientDescent,
        session));
    assert(session.Breakpoints.size() == 4);

    MiaIA::Core::TrainingRunSnapshot run;
    assert(MiaIAClient::RunTrainingSession(4, run));
    assert(run.ExecutedSteps == 1);
    assert(run.StopReason ==
        MiaIA::Core::TrainingRunStopReason::BreakpointHit);

    session = MiaIAClient::GetTrainingSession();
    assert(session.Status == MiaIA::Core::TrainingSessionStatus::Active);
    assert(session.WorkerStopReason ==
        MiaIA::Core::TrainingWorkerStopReason::BreakpointHit);
    assert(session.HasBreakpointHit);
    assert(session.LastBreakpointHit.BreakpointId ==
        committedBreakpointId);
    assert(session.LastBreakpointHit.SampleIndex == 0);
    assert(session.Breakpoints[0].HitCount == 1);

    MiaIA::Core::TrainingBreakpointHitSnapshot hit;
    assert(MiaIAClient::TryGetLastTrainingBreakpointHit(hit));
    assert(hit.BreakpointId == committedBreakpointId);

    assert(MiaIAClient::SetTrainingBreakpointEnabled(
        committedBreakpointId,
        false));
    assert(MiaIAClient::SetTrainingBreakpointEnabled(
        activationBreakpoint.Id,
        true));

    assert(MiaIAClient::ResumeTrainingSession());

    for (int attempt = 0; attempt < 200; ++attempt)
    {
        session = MiaIAClient::GetTrainingSession();

        if (session.Status !=
            MiaIA::Core::TrainingSessionStatus::Running)
        {
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    session = MiaIAClient::GetTrainingSession();
    assert(session.Status == MiaIA::Core::TrainingSessionStatus::Active);
    assert(session.WorkerStopReason ==
        MiaIA::Core::TrainingWorkerStopReason::BreakpointHit);
    assert(session.LastBreakpointHit.BreakpointId ==
        activationBreakpoint.Id);
    assert(session.CompletedSteps == 2);

    assert(MiaIAClient::SetTrainingBreakpointEnabled(
        activationBreakpoint.Id,
        false));
    assert(MiaIAClient::SetTrainingBreakpointEnabled(
        gradientBreakpoint.Id,
        true));
    assert(MiaIAClient::SetTrainingBreakpointEnabled(
        updateBreakpoint.Id,
        true));

    MiaIA::Core::TrainingBreakpointSpec forwardPhase;
    forwardPhase.Kind = MiaIA::Core::TrainingBreakpointKind::Phase;
    forwardPhase.Phase =
        MiaIA::Core::TrainingDebugPhase::ForwardComplete;
    MiaIA::Core::TrainingBreakpointSnapshot forwardBreakpoint;
    assert(MiaIAClient::AddTrainingBreakpoint(
        forwardPhase,
        forwardBreakpoint));

    MiaIA::Core::TrainingDebugSnapshot debug;
    assert(MiaIAClient::StartTrainingSessionDebug(debug));
    assert(MiaIAClient::AdvanceTrainingDebug(debug));
    assert(debug.Phase ==
        MiaIA::Core::TrainingDebugPhase::ForwardComplete);
    assert(MiaIAClient::TryGetLastTrainingBreakpointHit(hit));
    assert(hit.BreakpointId == forwardBreakpoint.Id);
    assert(hit.Phase ==
        MiaIA::Core::TrainingDebugPhase::ForwardComplete);

    assert(MiaIAClient::AdvanceTrainingDebug(debug));
    assert(debug.Phase ==
        MiaIA::Core::TrainingDebugPhase::BackwardComplete);
    assert(MiaIAClient::TryGetLastTrainingBreakpointHit(hit));
    assert(hit.BreakpointId == gradientBreakpoint.Id);

    assert(MiaIAClient::AdvanceTrainingDebug(debug));
    assert(debug.Phase ==
        MiaIA::Core::TrainingDebugPhase::UpdateComplete);
    assert(MiaIAClient::TryGetLastTrainingBreakpointHit(hit));
    assert(hit.BreakpointId == updateBreakpoint.Id);
    assert(MiaIAClient::CancelTrainingDebug());
    assert(MiaIAClient::CancelTrainingSession());

    assert(MiaIAClient::RemoveTrainingBreakpoint(
        forwardBreakpoint.Id));
    assert(MiaIAClient::ClearTrainingBreakpoints());
    assert(MiaIAClient::GetTrainingBreakpoints().empty());
    assert(MiaIAClient::ClearDataset());
    assert(MiaIAClient::ClearNetwork());

    std::filesystem::remove(datasetPath);
    std::filesystem::remove(testDirectory);

    });

    runner.Run("Training session history inspection", [&]()
    {
    const std::filesystem::path historyPath =
        std::filesystem::temp_directory_path() /
        "miaia_training_session_history_test.csv";

    {
        std::ofstream output(historyPath);
        assert(output.good());
        output
            << "x,target\n"
            << "1,1\n"
            << "2,0\n";
    }

    assert(MiaIAClient::ClearDataset());
    assert(MiaIAClient::ClearNetwork());
    assert(MiaIAClient::ImportCsvDataset(
        historyPath.string(),
        1,
        1));
    assert(MiaIAClient::CreateDenseNetwork(1, 1, 0, 1));
    assert(MiaIAClient::SetLayerActivation(
        1,
        MiaIA::Core::ActivationType::Linear));
    assert(MiaIAClient::SetConnectionWeight(1, 0.0));
    assert(MiaIAClient::SetNeuronBias(1002, 0.0));

    MiaIA::Core::TrainingSessionSnapshot session;
    assert(MiaIAClient::StartTrainingSession(
        2,
        0.1,
        MiaIA::Core::LossType::MeanSquaredError,
        MiaIA::Core::OptimizerType::StochasticGradientDescent,
        session));
    assert(MiaIAClient::GetTrainingSessionHistory().empty());

    MiaIA::Core::TrainingRunSnapshot run;
    assert(MiaIAClient::RunTrainingSession(3, run));

    const auto history = MiaIAClient::GetTrainingSessionHistory();
    assert(history.size() == 3);
    assert(history[0].StepIndex == 0);
    assert(history[0].EpochIndex == 0);
    assert(history[0].SampleIndex == 0);
    assert(std::abs(history[0].LossBefore - 1.0) < 1e-12);
    assert(std::abs(history[0].LossAfter - 0.36) < 1e-12);
    assert(history[0].WeightUpdateCount == 1);
    assert(history[0].BiasUpdateCount == 1);
    assert(history[1].StepIndex == 1);
    assert(history[1].EpochIndex == 0);
    assert(history[1].SampleIndex == 1);
    assert(std::abs(history[1].LossBefore - 0.36) < 1e-12);
    assert(std::abs(history[1].LossAfter) < 1e-12);
    assert(history[2].StepIndex == 2);
    assert(history[2].EpochIndex == 1);
    assert(history[2].SampleIndex == 0);

    MiaIA::Core::TrainingStepSnapshot inspectedStep;
    assert(MiaIAClient::TryGetTrainingSessionStep(
        1,
        inspectedStep));
    assert(inspectedStep.SampleIndex == 1);
    assert(std::abs(
        inspectedStep.Before.Evaluation.Loss - 0.36) < 1e-12);
    assert(std::abs(inspectedStep.After.Loss) < 1e-12);
    assert(inspectedStep.ConnectionUpdates.size() == 1);
    assert(inspectedStep.NeuronUpdates.size() == 1);

    MiaIA::Core::TrainingStepSnapshot rejectedStep;
    rejectedStep.SampleIndex = 999;
    rejectedStep.LearningRate = 42.0;

    assert(!MiaIAClient::TryGetTrainingSessionStep(
        3,
        rejectedStep));
    assert(rejectedStep.SampleIndex == 999);
    assert(rejectedStep.LearningRate == 42.0);
    assert(MiaIAClient::CancelTrainingSession());

    assert(MiaIAClient::ClearDataset());
    assert(MiaIAClient::ClearNetwork());
    std::filesystem::remove(historyPath);

    });

    runner.Run("Phase-by-phase training debug", [&]()
    {
    const std::filesystem::path debugPath =
        std::filesystem::temp_directory_path() /
        "miaia_training_debug_test.csv";

    {
        std::ofstream output(debugPath);
        assert(output.good());
        output
            << "x,target\n"
            << "1,1\n";
    }

    assert(MiaIAClient::ClearDataset());
    assert(MiaIAClient::ClearNetwork());
    assert(MiaIAClient::ImportCsvDataset(
        debugPath.string(),
        1,
        1));
    assert(MiaIAClient::CreateDenseNetwork(1, 1, 0, 1));
    assert(MiaIAClient::SetLayerActivation(
        1,
        MiaIA::Core::ActivationType::Linear));
    assert(MiaIAClient::SetConnectionWeight(1, 0.0));
    assert(MiaIAClient::SetNeuronBias(1002, 0.0));

    const auto original = MiaIAClient::GetSnapshot();
    MiaIA::Core::TrainingDebugSnapshot debug;
    MiaIA::Core::TrainingDebugNeuronSnapshot debugNeuron;
    MiaIA::Core::TrainingDebugConnectionSnapshot debugConnection;

    debugNeuron.Id = 999;
    debugNeuron.PublicBias = 42.0;
    debugConnection.Id = 999;
    debugConnection.PublicWeight = 42.0;

    assert(!MiaIAClient::TryGetTrainingDebugNeuron(
        1002,
        debugNeuron));
    assert(debugNeuron.Id == 999);
    assert(debugNeuron.PublicBias == 42.0);
    assert(!MiaIAClient::TryGetTrainingDebugConnection(
        1,
        debugConnection));
    assert(debugConnection.Id == 999);
    assert(debugConnection.PublicWeight == 42.0);

    assert(MiaIAClient::StartTrainingDebug(
        0,
        0.1,
        MiaIA::Core::LossType::MeanSquaredError,
        MiaIA::Core::OptimizerType::StochasticGradientDescent,
        debug));
    assert(debug.Phase ==
        MiaIA::Core::TrainingDebugPhase::BeforeForward);
    assert(debug.SampleIndex == 0);
    assert(debug.CandidateNetwork.Connections[0].Weight == 0.0);
    assert(!MiaIAClient::SetConnectionWeight(1, 42.0));
    assert(MiaIAClient::TryGetTrainingDebugNeuron(
        1002,
        debugNeuron));
    assert(debugNeuron.Phase ==
        MiaIA::Core::TrainingDebugPhase::BeforeForward);
    assert(debugNeuron.LayerOrder == 1);
    assert(debugNeuron.PublicBias == 0.0);
    assert(debugNeuron.CandidateBias == 0.0);
    assert(!debugNeuron.HasGradients);
    assert(!debugNeuron.HasUpdate);
    assert(MiaIAClient::TryGetTrainingDebugConnection(
        1,
        debugConnection));
    assert(debugConnection.PublicWeight == 0.0);
    assert(debugConnection.CandidateWeight == 0.0);
    assert(!debugConnection.HasGradient);
    assert(!debugConnection.HasUpdate);

    MiaIA::Core::TrainingSessionSnapshot rejectedSession;
    assert(!MiaIAClient::StartTrainingSession(
        1,
        0.1,
        MiaIA::Core::LossType::MeanSquaredError,
        MiaIA::Core::OptimizerType::StochasticGradientDescent,
        rejectedSession));

    assert(MiaIAClient::AdvanceTrainingDebug(debug));
    assert(debug.Phase ==
        MiaIA::Core::TrainingDebugPhase::ForwardComplete);
    assert(std::abs(debug.Step.Before.Evaluation.Loss - 1.0) <
        1e-12);
    assert(debug.Step.Before.Evaluation.Predictions.size() == 1);
    assert(debug.Step.Before.Evaluation.Predictions[0] == 0.0);
    assert(debug.Step.Before.Neurons.empty());
    assert(MiaIAClient::TryGetTrainingDebugNeuron(
        1002,
        debugNeuron));
    assert(debugNeuron.Phase ==
        MiaIA::Core::TrainingDebugPhase::ForwardComplete);
    assert(debugNeuron.PublicActivation == 0.0);
    assert(debugNeuron.CandidateActivation == 0.0);
    assert(!debugNeuron.HasGradients);

    assert(MiaIAClient::AdvanceTrainingDebug(debug));
    assert(debug.Phase ==
        MiaIA::Core::TrainingDebugPhase::BackwardComplete);
    assert(debug.Step.Before.Neurons.size() == 2);
    assert(debug.Step.Before.Connections.size() == 1);
    assert(std::abs(
        debug.Step.Before.Connections[0].WeightGradient + 2.0) <
        1e-12);
    assert(MiaIAClient::TryGetTrainingDebugNeuron(
        1002,
        debugNeuron));
    assert(debugNeuron.HasGradients);
    assert(std::abs(debugNeuron.BiasGradient + 2.0) < 1e-12);
    assert(!debugNeuron.HasUpdate);
    assert(MiaIAClient::TryGetTrainingDebugConnection(
        1,
        debugConnection));
    assert(debugConnection.HasGradient);
    assert(std::abs(debugConnection.WeightGradient + 2.0) <
        1e-12);
    assert(!debugConnection.HasUpdate);

    assert(MiaIAClient::AdvanceTrainingDebug(debug));
    assert(debug.Phase ==
        MiaIA::Core::TrainingDebugPhase::UpdateComplete);
    assert(debug.Step.ConnectionUpdates.size() == 1);
    assert(debug.Step.NeuronUpdates.size() == 1);
    assert(std::abs(
        debug.CandidateNetwork.Connections[0].Weight - 0.2) <
        1e-12);
    assert(MiaIAClient::TryGetTrainingDebugNeuron(
        1002,
        debugNeuron));
    assert(debugNeuron.HasUpdate);
    assert(debugNeuron.PublicBias == 0.0);
    assert(std::abs(debugNeuron.CandidateBias - 0.2) < 1e-12);
    assert(std::abs(debugNeuron.Delta - 0.2) < 1e-12);
    assert(MiaIAClient::TryGetTrainingDebugConnection(
        1,
        debugConnection));
    assert(debugConnection.HasUpdate);
    assert(debugConnection.PublicWeight == 0.0);
    assert(std::abs(debugConnection.CandidateWeight - 0.2) <
        1e-12);
    assert(std::abs(debugConnection.Delta - 0.2) < 1e-12);

    debugNeuron.Id = 999;
    debugNeuron.PublicBias = 42.0;
    assert(!MiaIAClient::TryGetTrainingDebugNeuron(
        9999,
        debugNeuron));
    assert(debugNeuron.Id == 999);
    assert(debugNeuron.PublicBias == 42.0);

    debugConnection.Id = 999;
    debugConnection.PublicWeight = 42.0;
    assert(!MiaIAClient::TryGetTrainingDebugConnection(
        9999,
        debugConnection));
    assert(debugConnection.Id == 999);
    assert(debugConnection.PublicWeight == 42.0);

    auto publicNetwork = MiaIAClient::GetSnapshot();
    assert(publicNetwork.Connections[0].Weight ==
        original.Connections[0].Weight);
    assert(publicNetwork.Layers[1].Neurons[0].Bias ==
        original.Layers[1].Neurons[0].Bias);

    assert(MiaIAClient::AdvanceTrainingDebug(debug));
    assert(debug.Phase == MiaIA::Core::TrainingDebugPhase::Verified);
    assert(std::abs(debug.Step.After.Loss - 0.36) < 1e-12);
    assert(MiaIAClient::TryGetTrainingDebugNeuron(
        1002,
        debugNeuron));
    assert(std::abs(debugNeuron.CandidateActivation - 0.4) <
        1e-12);
    assert(debugNeuron.PublicActivation == 0.0);

    publicNetwork = MiaIAClient::GetSnapshot();
    assert(publicNetwork.Connections[0].Weight == 0.0);
    assert(publicNetwork.Layers[1].Neurons[0].Bias == 0.0);

    assert(MiaIAClient::AdvanceTrainingDebug(debug));
    assert(debug.Phase == MiaIA::Core::TrainingDebugPhase::Committed);

    publicNetwork = MiaIAClient::GetSnapshot();
    assert(std::abs(publicNetwork.Connections[0].Weight - 0.2) <
        1e-12);
    assert(std::abs(
        publicNetwork.Layers[1].Neurons[0].Bias - 0.2) < 1e-12);
    assert(MiaIAClient::TryGetTrainingDebugConnection(
        1,
        debugConnection));
    assert(debugConnection.Phase ==
        MiaIA::Core::TrainingDebugPhase::Committed);
    assert(std::abs(debugConnection.PublicWeight - 0.2) <
        1e-12);
    assert(std::abs(debugConnection.CandidateWeight - 0.2) <
        1e-12);

    MiaIA::Core::TrainingDebugSnapshot rejectedDebug;
    rejectedDebug.SampleIndex = 999;
    rejectedDebug.LearningRate = 42.0;
    assert(!MiaIAClient::AdvanceTrainingDebug(rejectedDebug));
    assert(rejectedDebug.SampleIndex == 999);
    assert(rejectedDebug.LearningRate == 42.0);
    assert(!MiaIAClient::CancelTrainingDebug());

    assert(MiaIAClient::SetConnectionWeight(1, 0.0));
    assert(MiaIAClient::SetNeuronBias(1002, 0.0));
    assert(MiaIAClient::StartTrainingDebug(
        0,
        0.1,
        MiaIA::Core::LossType::MeanSquaredError,
        MiaIA::Core::OptimizerType::StochasticGradientDescent,
        debug));
    assert(MiaIAClient::AdvanceTrainingDebug(debug));
    assert(MiaIAClient::AdvanceTrainingDebug(debug));
    assert(MiaIAClient::AdvanceTrainingDebug(debug));
    assert(MiaIAClient::CancelTrainingDebug());
    assert(MiaIAClient::GetTrainingDebug().Phase ==
        MiaIA::Core::TrainingDebugPhase::Idle);
    assert(!MiaIAClient::TryGetTrainingDebugNeuron(
        1002,
        debugNeuron));
    assert(!MiaIAClient::TryGetTrainingDebugConnection(
        1,
        debugConnection));

    publicNetwork = MiaIAClient::GetSnapshot();
    assert(publicNetwork.Connections[0].Weight == 0.0);
    assert(publicNetwork.Layers[1].Neurons[0].Bias == 0.0);

    assert(!MiaIAClient::StartTrainingDebug(
        9,
        0.1,
        MiaIA::Core::LossType::MeanSquaredError,
        MiaIA::Core::OptimizerType::StochasticGradientDescent,
        rejectedDebug));
    assert(rejectedDebug.SampleIndex == 999);
    assert(rejectedDebug.LearningRate == 42.0);

    assert(MiaIAClient::ClearDataset());
    assert(MiaIAClient::ClearNetwork());
    std::filesystem::remove(debugPath);

    });

    runner.Run("Training session phase debug", [&]()
    {
    const std::filesystem::path sessionDebugPath =
        std::filesystem::temp_directory_path() /
        "miaia_training_session_debug_test.csv";

    {
        std::ofstream output(sessionDebugPath);
        assert(output.good());
        output
            << "x,target\n"
            << "1,1\n"
            << "2,0\n";
    }

    assert(MiaIAClient::ClearDataset());
    assert(MiaIAClient::ClearNetwork());
    assert(MiaIAClient::ImportCsvDataset(
        sessionDebugPath.string(),
        1,
        1));
    assert(MiaIAClient::CreateDenseNetwork(1, 1, 0, 1));
    assert(MiaIAClient::SetLayerActivation(
        1,
        MiaIA::Core::ActivationType::Linear));
    assert(MiaIAClient::SetConnectionWeight(1, 0.0));
    assert(MiaIAClient::SetNeuronBias(1002, 0.0));

    MiaIA::Core::TrainingSessionSnapshot session;
    assert(MiaIAClient::StartTrainingSession(
        1,
        0.1,
        MiaIA::Core::LossType::MeanSquaredError,
        MiaIA::Core::OptimizerType::StochasticGradientDescent,
        session));

    MiaIA::Core::TrainingDebugSnapshot debug;
    assert(MiaIAClient::StartTrainingSessionDebug(debug));
    assert(debug.Phase ==
        MiaIA::Core::TrainingDebugPhase::BeforeForward);
    assert(debug.SampleIndex == 0);

    session = MiaIAClient::GetTrainingSession();
    assert(session.Status == MiaIA::Core::TrainingSessionStatus::Active);
    assert(session.CompletedSteps == 0);
    assert(session.CurrentEpoch == 0);
    assert(session.NextSampleIndex == 0);

    MiaIA::Core::TrainingStepSnapshot rejectedStep;
    MiaIA::Core::TrainingRunSnapshot rejectedRun;
    assert(!MiaIAClient::AdvanceTrainingSession(rejectedStep));
    assert(!MiaIAClient::RunTrainingSession(1, rejectedRun));
    assert(!MiaIAClient::ResumeTrainingSession());
    assert(!MiaIAClient::CancelTrainingSession());

    assert(MiaIAClient::AdvanceTrainingDebug(debug));
    assert(MiaIAClient::AdvanceTrainingDebug(debug));
    assert(MiaIAClient::AdvanceTrainingDebug(debug));
    assert(debug.Phase ==
        MiaIA::Core::TrainingDebugPhase::UpdateComplete);

    auto network = MiaIAClient::GetSnapshot();
    assert(network.Connections[0].Weight == 0.0);
    assert(network.Layers[1].Neurons[0].Bias == 0.0);
    assert(MiaIAClient::CancelTrainingDebug());

    session = MiaIAClient::GetTrainingSession();
    assert(session.CompletedSteps == 0);
    assert(session.CurrentEpoch == 0);
    assert(session.NextSampleIndex == 0);
    assert(session.Steps.empty());

    network = MiaIAClient::GetSnapshot();
    assert(network.Connections[0].Weight == 0.0);
    assert(network.Layers[1].Neurons[0].Bias == 0.0);

    assert(MiaIAClient::StartTrainingSessionDebug(debug));

    for (int phase = 0; phase < 5; ++phase)
    {
        assert(MiaIAClient::AdvanceTrainingDebug(debug));
    }

    assert(debug.Phase == MiaIA::Core::TrainingDebugPhase::Committed);

    session = MiaIAClient::GetTrainingSession();
    assert(session.Status == MiaIA::Core::TrainingSessionStatus::Active);
    assert(session.CompletedSteps == 1);
    assert(session.CurrentEpoch == 0);
    assert(session.NextSampleIndex == 1);
    assert(session.Steps.size() == 1);
    assert(session.Steps[0].SampleIndex == 0);
    assert(std::abs(
        session.Steps[0].Before.Evaluation.Loss - 1.0) < 1e-12);
    assert(std::abs(session.Steps[0].After.Loss - 0.36) < 1e-12);

    assert(!MiaIAClient::AdvanceTrainingDebug(debug));
    session = MiaIAClient::GetTrainingSession();
    assert(session.CompletedSteps == 1);
    assert(session.NextSampleIndex == 1);

    assert(MiaIAClient::StartTrainingSessionDebug(debug));
    assert(debug.SampleIndex == 1);

    for (int phase = 0; phase < 5; ++phase)
    {
        assert(MiaIAClient::AdvanceTrainingDebug(debug));
    }

    session = MiaIAClient::GetTrainingSession();
    assert(session.Status ==
        MiaIA::Core::TrainingSessionStatus::Completed);
    assert(session.CompletedSteps == 2);
    assert(session.CurrentEpoch == 1);
    assert(session.NextSampleIndex == 0);
    assert(session.Steps.size() == 2);
    assert(session.Steps[1].SampleIndex == 1);
    assert(!MiaIAClient::StartTrainingSessionDebug(debug));

    assert(MiaIAClient::GetTrainingSessionHistory().size() == 2);
    assert(MiaIAClient::ClearDataset());
    assert(MiaIAClient::ClearNetwork());
    std::filesystem::remove(sessionDebugPath);

    });

    runner.Run("Prediction pipeline", [&]()
    {
    MiaIAClient::ClearNetwork();

    assert(MiaIAClient::CreateDenseNetwork(2, 2, 0, 1));
    assert(MiaIAClient::SetLayerActivation(
        1,
        MiaIA::Core::ActivationType::Linear));
    assert(MiaIAClient::SetConnectionWeight(1, 2.0));
    assert(MiaIAClient::SetConnectionWeight(2, -1.0));
    assert(MiaIAClient::SetNeuronBias(1003, 0.5));

    MiaIA::Core::PredictionSnapshot prediction;

    assert(MiaIAClient::Predict({ 1.0, 1.0 }, prediction));
    assert(prediction.Inputs.size() == 2);
    assert(prediction.Outputs.size() == 1);
    assert(prediction.Inputs[0] == 1.0);
    assert(prediction.Inputs[1] == 1.0);
    assert(std::abs(prediction.Outputs[0] - 1.5) < 1e-12);

    const auto afterSingleOutputPrediction =
        MiaIAClient::GetSnapshot();

    assert(afterSingleOutputPrediction.Layers[0].Neurons[0].Activation == 1.0);
    assert(afterSingleOutputPrediction.Layers[0].Neurons[1].Activation == 1.0);
    assert(std::abs(
        afterSingleOutputPrediction.Layers[1].Neurons[0].Activation - 1.5) <
        1e-12);

    assert(MiaIAClient::SetInputValues({ 2.0, 1.0 }));
    assert(MiaIAClient::Forward());

    const auto manualForward = MiaIAClient::GetSnapshot();

    assert(MiaIAClient::Predict({ 2.0, 1.0 }, prediction));
    assert(std::abs(
        prediction.Outputs[0] -
        manualForward.Layers[1].Neurons[0].Activation) < 1e-12);

    assert(MiaIAClient::CreateDenseNetwork(2, 2, 0, 2));
    assert(MiaIAClient::SetLayerActivation(
        1,
        MiaIA::Core::ActivationType::Linear));
    assert(MiaIAClient::SetConnectionWeight(1, 1.0));
    assert(MiaIAClient::SetConnectionWeight(2, 2.0));
    assert(MiaIAClient::SetConnectionWeight(3, -1.0));
    assert(MiaIAClient::SetConnectionWeight(4, 0.5));
    assert(MiaIAClient::SetNeuronBias(1003, 0.25));
    assert(MiaIAClient::SetNeuronBias(1004, -0.5));

    assert(MiaIAClient::Predict({ 0.5, 2.0 }, prediction));
    assert(prediction.Outputs.size() == 2);
    assert(std::abs(prediction.Outputs[0] - (-1.25)) < 1e-12);
    assert(std::abs(prediction.Outputs[1] - 1.5) < 1e-12);

    MiaIA::Core::PredictionSnapshot rejectedPrediction;
    rejectedPrediction.Inputs = { 9.0 };
    rejectedPrediction.Outputs = { 8.0 };

    const auto beforeRejectedPredictions =
        MiaIAClient::GetSnapshot();

    const auto assertRejectedPredictionPreserved = [&]()
    {
        assert(rejectedPrediction.Inputs.size() == 1);
        assert(rejectedPrediction.Outputs.size() == 1);
        assert(rejectedPrediction.Inputs[0] == 9.0);
        assert(rejectedPrediction.Outputs[0] == 8.0);
    };

    assert(!MiaIAClient::Predict({}, rejectedPrediction));
    assertRejectedPredictionPreserved();
    assert(!MiaIAClient::Predict({ 1.0 }, rejectedPrediction));
    assertRejectedPredictionPreserved();
    assert(!MiaIAClient::Predict({ 1.0, 2.0, 3.0 }, rejectedPrediction));
    assertRejectedPredictionPreserved();
    assert(!MiaIAClient::Predict({
        std::numeric_limits<double>::quiet_NaN(),
        1.0
        }, rejectedPrediction));
    assertRejectedPredictionPreserved();
    assert(!MiaIAClient::Predict({
        std::numeric_limits<double>::infinity(),
        1.0
        }, rejectedPrediction));
    assertRejectedPredictionPreserved();

    const auto afterRejectedPredictions =
        MiaIAClient::GetSnapshot();

    for (std::size_t layerIndex = 0;
        layerIndex < beforeRejectedPredictions.Layers.size();
        ++layerIndex)
    {
        for (std::size_t neuronIndex = 0;
            neuronIndex <
                beforeRejectedPredictions.Layers[layerIndex].Neurons.size();
            ++neuronIndex)
        {
            assert(
                afterRejectedPredictions.Layers[layerIndex]
                    .Neurons[neuronIndex].Activation ==
                beforeRejectedPredictions.Layers[layerIndex]
                    .Neurons[neuronIndex].Activation);
        }
    }

    MiaIAClient::ClearNetwork();

    assert(!MiaIAClient::Predict({ 1.0, 1.0 }, rejectedPrediction));
    assertRejectedPredictionPreserved();

    assert(MiaIAClient::CreateDenseNetwork(1, 1, 0, 1));
    assert(MiaIAClient::SetLayerActivation(
        1,
        MiaIA::Core::ActivationType::Linear));
    assert(MiaIAClient::SetConnectionWeight(
        1,
        (std::numeric_limits<double>::max)()));
    assert(MiaIAClient::SetNeuronActivation(1001, 0.25));
    assert(MiaIAClient::SetNeuronActivation(1002, 0.75));

    assert(!MiaIAClient::Predict({ 2.0 }, rejectedPrediction));
    assertRejectedPredictionPreserved();

    const auto afterOverflowPrediction =
        MiaIAClient::GetSnapshot();

    assert(afterOverflowPrediction.Layers[0].Neurons[0].Activation == 0.25);
    assert(afterOverflowPrediction.Layers[1].Neurons[0].Activation == 0.75);

    MiaIAClient::ClearNetwork();

    });

    runner.Run("Network input", [&]()
    {
    MiaIAClient::ClearNetwork();

    assert(MiaIAClient::CreateDenseNetwork(3, 2, 1, 1));

    const std::vector<double> inputValues{
        0.25,
        -0.5,
        1.0
    };

    assert(MiaIAClient::SetInputValues(inputValues));

    const auto inputSnapshot = MiaIAClient::GetSnapshot();

    assert(inputSnapshot.Layers[0].Neurons.size() == 3);
    assert(inputSnapshot.Layers[0].Neurons[0].Activation == 0.25);
    assert(inputSnapshot.Layers[0].Neurons[1].Activation == -0.5);
    assert(inputSnapshot.Layers[0].Neurons[2].Activation == 1.0);

    assert(!MiaIAClient::SetInputValues({ 0.1, 0.2 }));
    assert(!MiaIAClient::SetInputValues({ 0.1, 0.2, 0.3, 0.4 }));
    assert(!MiaIAClient::SetInputValues({
        0.1,
        std::numeric_limits<double>::quiet_NaN(),
        0.3
        }));
    assert(!MiaIAClient::SetInputValues({
        0.1,
        std::numeric_limits<double>::infinity(),
        0.3
        }));

    const auto afterInvalidInput = MiaIAClient::GetSnapshot();

    assert(afterInvalidInput.Layers[0].Neurons[0].Activation == 0.25);
    assert(afterInvalidInput.Layers[0].Neurons[1].Activation == -0.5);
    assert(afterInvalidInput.Layers[0].Neurons[2].Activation == 1.0);

    assert(MiaIAClient::Forward());

    MiaIAClient::ClearNetwork();

    assert(!MiaIAClient::SetInputValues({ 1.0 }));

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

    const auto directOverview = MiaIAClient::GetNetworkOverview();
    assert(directOverview.Layers.size() == 2);
    assert(directOverview.NeuronCount == 3);
    assert(directOverview.ConnectionCount == 2);

    assert(MiaIAClient::CreateDenseNetwork(64, 64, 16, 2));

    const auto largeOverview = MiaIAClient::GetNetworkOverview();
    assert(largeOverview.Layers.size() == 18);
    assert(largeOverview.NeuronCount == 1090);
    assert(largeOverview.ConnectionCount == 65664);
    assert(largeOverview.Layers.front().NeuronCount == 64);
    assert(largeOverview.Layers.back().NeuronCount == 2);

    MiaIA::Core::ConnectionSnapshot finalConnection;
    assert(MiaIAClient::TryGetConnection(65664, finalConnection));
    assert(finalConnection.FromNeuron == 2088);
    assert(finalConnection.ToNeuron == 2090);

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

    runner.Run("ONNX export", [&]()
    {
    const std::filesystem::path exportPath =
        std::filesystem::temp_directory_path() /
        "miaia_export_test.onnx";
    const std::filesystem::path invalidExportPath =
        std::filesystem::temp_directory_path() /
        "miaia_invalid_export_test.onnx";

    std::filesystem::remove(exportPath);
    std::filesystem::remove(invalidExportPath);

    MiaIAClient::ClearNetwork();

    assert(MiaIAClient::CreateDenseNetwork(2, 2, 1, 1));
    assert(MiaIAClient::SetLayerActivation(
        1,
        MiaIA::Core::ActivationType::ReLU));
    assert(MiaIAClient::SetLayerActivation(
        2,
        MiaIA::Core::ActivationType::Tanh));

    assert(MiaIAClient::SetNeuronBias(1003, 0.3));
    assert(MiaIAClient::SetNeuronBias(1004, -0.2));
    assert(MiaIAClient::SetNeuronBias(1005, 0.7));

    assert(MiaIAClient::SetConnectionWeight(1, 0.1));
    assert(MiaIAClient::SetConnectionWeight(2, 0.2));
    assert(MiaIAClient::SetConnectionWeight(3, 0.3));
    assert(MiaIAClient::SetConnectionWeight(4, 0.4));
    assert(MiaIAClient::SetConnectionWeight(5, 0.5));
    assert(MiaIAClient::SetConnectionWeight(6, 0.6));

    assert(MiaIAClient::ExportOnnx(exportPath.string()));
    assert(std::filesystem::exists(exportPath));
    assert(std::filesystem::file_size(exportPath) > 0);

    onnx::ModelProto model;
    std::ifstream input(exportPath, std::ios::binary);

    assert(input.good());
    assert(model.ParseFromIstream(&input));

    onnx::checker::check_model(model);

    assert(model.ir_version() == 8);
    assert(model.producer_name() == "MiaIA");
    assert(model.opset_import_size() == 1);
    assert(model.opset_import(0).version() == 18);

    const onnx::GraphProto& graph = model.graph();

    assert(graph.input_size() == 1);
    assert(graph.output_size() == 1);
    assert(graph.node_size() == 4);
    assert(graph.initializer_size() == 4);
    assert(graph.node(0).op_type() == "Gemm");
    assert(graph.node(1).op_type() == "Relu");
    assert(graph.node(2).op_type() == "Gemm");
    assert(graph.node(3).op_type() == "Tanh");

    const onnx::TensorProto* hiddenWeights =
        FindInitializer(graph, "layer_1_weights");
    const onnx::TensorProto* hiddenBiases =
        FindInitializer(graph, "layer_1_biases");
    const onnx::TensorProto* outputWeights =
        FindInitializer(graph, "layer_2_weights");
    const onnx::TensorProto* outputBiases =
        FindInitializer(graph, "layer_2_biases");

    assert(hiddenWeights != nullptr);
    assert(hiddenBiases != nullptr);
    assert(outputWeights != nullptr);
    assert(outputBiases != nullptr);

    assert(hiddenWeights->dims_size() == 2);
    assert(hiddenWeights->dims(0) == 2);
    assert(hiddenWeights->dims(1) == 2);
    assert(hiddenWeights->double_data_size() == 4);
    assert(hiddenWeights->double_data(0) == 0.1);
    assert(hiddenWeights->double_data(1) == 0.2);
    assert(hiddenWeights->double_data(2) == 0.3);
    assert(hiddenWeights->double_data(3) == 0.4);

    assert(hiddenBiases->double_data_size() == 2);
    assert(hiddenBiases->double_data(0) == 0.3);
    assert(hiddenBiases->double_data(1) == -0.2);
    assert(outputWeights->double_data_size() == 2);
    assert(outputWeights->double_data(0) == 0.5);
    assert(outputWeights->double_data(1) == 0.6);
    assert(outputBiases->double_data_size() == 1);
    assert(outputBiases->double_data(0) == 0.7);

    assert(model.metadata_props_size() == 10);

    input.close();

    MiaIAClient::ClearNetwork();

    assert(MiaIAClient::AddLayer(0, "Input", 0));
    assert(MiaIAClient::AddLayer(1, "Hidden", 1));
    assert(MiaIAClient::AddLayer(2, "Output", 2));
    assert(MiaIAClient::AddNeuron(0, 1001, 0.0, 0.0));
    assert(MiaIAClient::AddNeuron(1, 2001, 0.0, 0.0));
    assert(MiaIAClient::AddNeuron(2, 3001, 0.0, 0.0));
    assert(MiaIAClient::AddConnection(1, 1001, 2001, 0.5));
    assert(MiaIAClient::AddConnection(2, 2001, 3001, 0.5));
    assert(MiaIAClient::AddConnection(3, 1001, 3001, 0.5));

    assert(!MiaIAClient::ExportOnnx(invalidExportPath.string()));
    assert(!std::filesystem::exists(invalidExportPath));
    assert(!MiaIAClient::ExportOnnx(""));

    std::filesystem::remove(exportPath);
    std::filesystem::remove(invalidExportPath);

    });

    runner.Run("ONNX import and round trip", [&]()
    {
    const std::filesystem::path roundTripPath =
        std::filesystem::temp_directory_path() /
        "miaia_round_trip_test.onnx";
    const std::filesystem::path floatPath =
        std::filesystem::temp_directory_path() /
        "miaia_float_import_test.onnx";
    const std::filesystem::path unsupportedPath =
        std::filesystem::temp_directory_path() /
        "miaia_unsupported_import_test.onnx";

    std::filesystem::remove(roundTripPath);
    std::filesystem::remove(floatPath);
    std::filesystem::remove(unsupportedPath);

    MiaIAClient::ClearNetwork();

    assert(MiaIAClient::CreateDenseNetwork(2, 2, 1, 1));
    assert(MiaIAClient::SetLayerActivation(
        1,
        MiaIA::Core::ActivationType::ReLU));
    assert(MiaIAClient::SetLayerActivation(
        2,
        MiaIA::Core::ActivationType::Tanh));
    assert(MiaIAClient::SetNeuronBias(1003, 0.3));
    assert(MiaIAClient::SetNeuronBias(1004, -0.2));
    assert(MiaIAClient::SetNeuronBias(1005, 0.7));
    assert(MiaIAClient::SetConnectionWeight(1, 0.1));
    assert(MiaIAClient::SetConnectionWeight(2, 0.2));
    assert(MiaIAClient::SetConnectionWeight(3, 0.3));
    assert(MiaIAClient::SetConnectionWeight(4, 0.4));
    assert(MiaIAClient::SetConnectionWeight(5, 0.5));
    assert(MiaIAClient::SetConnectionWeight(6, 0.6));

    const auto original = MiaIAClient::GetSnapshot();

    assert(MiaIAClient::SetInputValues({ 0.25, -0.5 }));
    assert(MiaIAClient::Forward());

    const auto originalForward = MiaIAClient::GetSnapshot();
    const double expectedOutput =
        originalForward.Layers.back().Neurons.front().Activation;

    assert(MiaIAClient::ExportOnnx(roundTripPath.string()));

    MiaIAClient::ClearNetwork();
    assert(MiaIAClient::ImportOnnx(roundTripPath.string()));

    const auto imported = MiaIAClient::GetSnapshot();

    assert(imported.Layers.size() == original.Layers.size());
    assert(imported.Connections.size() == original.Connections.size());

    for (std::size_t layerIndex = 0;
        layerIndex < original.Layers.size();
        ++layerIndex)
    {
        const auto& expectedLayer = original.Layers[layerIndex];
        const auto& actualLayer = imported.Layers[layerIndex];

        assert(actualLayer.Id == expectedLayer.Id);
        assert(actualLayer.Name == expectedLayer.Name);
        assert(actualLayer.Order == expectedLayer.Order);
        assert(actualLayer.Activation == expectedLayer.Activation);
        assert(actualLayer.Neurons.size() ==
            expectedLayer.Neurons.size());

        for (std::size_t neuronIndex = 0;
            neuronIndex < expectedLayer.Neurons.size();
            ++neuronIndex)
        {
            assert(actualLayer.Neurons[neuronIndex].Id ==
                expectedLayer.Neurons[neuronIndex].Id);
            assert(actualLayer.Neurons[neuronIndex].Bias ==
                expectedLayer.Neurons[neuronIndex].Bias);
        }
    }

    for (std::size_t connectionIndex = 0;
        connectionIndex < original.Connections.size();
        ++connectionIndex)
    {
        const auto& expectedConnection =
            original.Connections[connectionIndex];
        const auto& actualConnection =
            imported.Connections[connectionIndex];

        assert(actualConnection.Id == expectedConnection.Id);
        assert(actualConnection.FromNeuron ==
            expectedConnection.FromNeuron);
        assert(actualConnection.ToNeuron ==
            expectedConnection.ToNeuron);
        assert(actualConnection.Weight == expectedConnection.Weight);
    }

    assert(MiaIAClient::SetInputValues({ 0.25, -0.5 }));
    assert(MiaIAClient::Forward());

    const auto importedForward = MiaIAClient::GetSnapshot();
    const double actualOutput =
        importedForward.Layers.back().Neurons.front().Activation;

    assert(std::abs(actualOutput - expectedOutput) < 1e-12);

    onnx::ModelProto floatModel;
    floatModel.set_ir_version(8);

    auto* floatOpset = floatModel.add_opset_import();
    floatOpset->set_domain("");
    floatOpset->set_version(18);

    auto* floatGraph = floatModel.mutable_graph();
    floatGraph->set_name("Float dense model");

    auto setFloatValueInfo = [](
        onnx::ValueInfoProto& value,
        const std::string& name,
        std::int64_t features)
    {
        value.set_name(name);
        auto* tensorType = value.mutable_type()->mutable_tensor_type();
        tensorType->set_elem_type(onnx::TensorProto_DataType_FLOAT);
        auto* shape = tensorType->mutable_shape();
        shape->add_dim()->set_dim_param("batch");
        shape->add_dim()->set_dim_value(features);
    };

    setFloatValueInfo(*floatGraph->add_input(), "input", 2);
    setFloatValueInfo(*floatGraph->add_output(), "output", 1);

    auto* floatWeights = floatGraph->add_initializer();
    floatWeights->set_name("weights");
    floatWeights->set_data_type(onnx::TensorProto_DataType_FLOAT);
    floatWeights->add_dims(2);
    floatWeights->add_dims(1);
    floatWeights->add_float_data(0.25f);
    floatWeights->add_float_data(-0.5f);

    auto* floatBiases = floatGraph->add_initializer();
    floatBiases->set_name("biases");
    floatBiases->set_data_type(onnx::TensorProto_DataType_FLOAT);
    floatBiases->add_dims(1);
    floatBiases->add_float_data(0.1f);

    auto* floatGemm = floatGraph->add_node();
    floatGemm->set_name("dense");
    floatGemm->set_op_type("Gemm");
    floatGemm->add_input("input");
    floatGemm->add_input("weights");
    floatGemm->add_input("biases");
    floatGemm->add_output("linear");

    auto* floatRelu = floatGraph->add_node();
    floatRelu->set_name("activation");
    floatRelu->set_op_type("Relu");
    floatRelu->add_input("linear");
    floatRelu->add_output("output");

    onnx::checker::check_model(floatModel);

    {
        std::ofstream output(floatPath, std::ios::binary);
        assert(output.good());
        assert(floatModel.SerializeToOstream(&output));
    }

    assert(MiaIAClient::ImportOnnx(floatPath.string()));

    const auto floatSnapshot = MiaIAClient::GetSnapshot();

    assert(floatSnapshot.Layers.size() == 2);
    assert(floatSnapshot.Layers[0].Name == "Input");
    assert(floatSnapshot.Layers[1].Name == "Output");
    assert(floatSnapshot.Layers[0].Neurons.size() == 2);
    assert(floatSnapshot.Layers[1].Neurons.size() == 1);
    assert(floatSnapshot.Layers[1].Activation ==
        MiaIA::Core::ActivationType::ReLU);
    assert(std::abs(
        floatSnapshot.Layers[1].Neurons[0].Bias - 0.1) < 1e-6);
    assert(floatSnapshot.Connections.size() == 2);
    assert(std::abs(
        floatSnapshot.Connections[0].Weight - 0.25) < 1e-6);
    assert(std::abs(
        floatSnapshot.Connections[1].Weight + 0.5) < 1e-6);

    assert(MiaIAClient::SetInputValues({ 2.0, 1.0 }));
    assert(MiaIAClient::Forward());

    const auto floatForward = MiaIAClient::GetSnapshot();

    assert(std::abs(
        floatForward.Layers[1].Neurons[0].Activation - 0.1) < 1e-6);

    const auto beforeFailedImport = MiaIAClient::GetSnapshot();

    onnx::ModelProto unsupportedModel;
    unsupportedModel.set_ir_version(8);

    auto* unsupportedOpset = unsupportedModel.add_opset_import();
    unsupportedOpset->set_domain("");
    unsupportedOpset->set_version(18);

    auto* unsupportedGraph = unsupportedModel.mutable_graph();
    unsupportedGraph->set_name("Unsupported model");
    setFloatValueInfo(*unsupportedGraph->add_input(), "input", 2);
    setFloatValueInfo(*unsupportedGraph->add_output(), "output", 2);

    auto* identity = unsupportedGraph->add_node();
    identity->set_name("identity");
    identity->set_op_type("Identity");
    identity->add_input("input");
    identity->add_output("output");

    onnx::checker::check_model(unsupportedModel);

    {
        std::ofstream output(unsupportedPath, std::ios::binary);
        assert(output.good());
        assert(unsupportedModel.SerializeToOstream(&output));
    }

    assert(!MiaIAClient::ImportOnnx(unsupportedPath.string()));
    assert(!MiaIAClient::ImportOnnx(""));

    const auto afterFailedImport = MiaIAClient::GetSnapshot();

    assert(afterFailedImport.Layers.size() ==
        beforeFailedImport.Layers.size());
    assert(afterFailedImport.Connections.size() ==
        beforeFailedImport.Connections.size());
    assert(afterFailedImport.Layers[0].Neurons[0].Id ==
        beforeFailedImport.Layers[0].Neurons[0].Id);
    assert(afterFailedImport.Connections[0].Weight ==
        beforeFailedImport.Connections[0].Weight);

    std::filesystem::remove(roundTripPath);
    std::filesystem::remove(floatPath);
    std::filesystem::remove(unsupportedPath);

    });

    return runner.Finish();
}
