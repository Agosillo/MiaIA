#include <iostream>
#include <string>
#include <chrono>
#include <vector>
#include <sstream>

#include "../SDK/Include/MiaIAClient.h"

std::string Trim(const std::string& value)
{
    const std::size_t first =
        value.find_first_not_of(" \t\r\n");

    if (first == std::string::npos)
    {
        return "";
    }

    const std::size_t last =
        value.find_last_not_of(" \t\r\n");

    return value.substr(
        first,
        last - first + 1);
}


void PrintHelp()
{
    std::cout
        << "\nMiaIA Console v0.1\n\n"
        << "Commands:\n\n"

        << "  help\n"
        << "      Show available commands\n\n"

        << "  create <inputs> <neurons-per-hidden-layer> <hidden-layers> <outputs>\n"
        << "      Create a dense neural network\n\n"
        << "      Example:\n"
        << "        create 784 256 3 10\n\n"

        << "  input [values]\n"
        << "      Set the input layer activation values\n\n"
        << "      Example:\n"
        << "        input 0.5 0.2 0.9\n\n"

        << "  predict <values>\n"
        << "      Apply inputs, run forward and print output values\n\n"

        << "      Example:\n"
        << "        predict 1 1\n\n"

        << "  import onnx [path]\n"
        << "      Replace the current network with an ONNX model\n\n"

        << "  export onnx [path]\n"
        << "      Export the current network as an ONNX model\n\n"

        << "  dataset import csv <input-count> <output-count> <path>\n"
        << "      Import a numeric CSV dataset with a header\n"
        << "      The first <input-count> columns are network inputs\n"
        << "      The following <output-count> columns are expected outputs\n\n"

        << "      Add --no-header before the path when needed\n\n"

        << "      Example:\n"
        << "        dataset import csv 2 1 \"C:\\Data\\xor.csv\"\n\n"

        << "  dataset summary\n"
        << "      Show information about the current dataset\n\n"

        << "  dataset inspect [index]\n"
        << "      Show one dataset sample\n\n"

        << "  dataset apply [index]\n"
        << "      Apply one sample to the network input\n\n"

        << "  dataset evaluate <index> mse\n"
        << "      Apply and evaluate one sample using mean squared error\n\n"

        << "  dataset evaluate all mse\n"
        << "      Evaluate every sample against one fixed network\n\n"

        << "  dataset gradients [index] mse\n"
        << "      Calculate gradients without changing weights or biases\n\n"

        << "  dataset clear\n"
        << "      Clear the current dataset\n\n"

        << "  train step <sample-index> <learning-rate> mse\n"
        << "      Apply one atomic SGD training step\n\n"

        << "  train epoch <learning-rate> mse\n"
        << "      Train all dataset samples in order as one atomic epoch\n\n"

        << "  train session start <epochs> <learning-rate> mse\n"
        << "      Start a manually controlled training session\n\n"

        << "  train session status\n"
        << "      Show controlled training progress\n\n"

        << "  train session next\n"
        << "      Execute exactly one sample training step\n\n"

        << "  train session cancel\n"
        << "      Stop the session without reverting completed steps\n\n"

        << "  summary\n"
        << "      Show network overview\n\n"

        << "  inspect\n"
        << "      Show layers, neurons and connections\n\n"

        << "  forward\n"
        << "      Execute forward propagation\n\n"

        << "  benchmark\n"
        << "      Measure forward performance\n\n"

        << "  exit\n"
        << "      Close console\n\n";
}


void CreateNetwork(const std::string& command)
{
    using MiaIA::SDK::MiaIAClient;


    int inputCount = 10;
    int hiddenCount = 32;
    int hiddenLayers = 2;
    int outputCount = 3;


    std::stringstream ss(command);

    std::string token;

    ss >> token; // Remove "create".


    if (ss >> inputCount
        >> hiddenCount
        >> hiddenLayers
        >> outputCount)
    {
        // Parameters received.
    }

    if (MiaIAClient::CreateDenseNetwork(
        inputCount,
        hiddenCount,
        hiddenLayers,
        outputCount))
    {
        std::cout
            << "Dense network created.\n";
    }
    else
    {
        std::cout
            << "Network creation failed.\n";
    }
}

void SetInput(const std::string& command)
{
    using MiaIA::SDK::MiaIAClient;

    std::stringstream stream(command);
    std::string token;
    std::vector<double> values;
    double value{};

    stream >> token;

    while (stream >> value)
    {
        values.push_back(value);
    }

    if (!stream.eof())
    {
        std::cout
            << "Input contains an invalid numeric value.\n";

        return;
    }

    if (values.empty())
    {
        std::cout
            << "Input requires at least one value.\n";

        return;
    }

    if (!MiaIAClient::SetInputValues(values))
    {
        std::cout
            << "Input values do not match the current network.\n";

        return;
    }

    std::cout
        << "Input values applied.\n";
}

bool ReadOnnxPath(
    const std::string& command,
    const std::string& expectedAction,
    std::string& path)
{
    std::stringstream stream(command);
    std::string action;
    std::string format;

    stream >> action >> format;

    if (action != expectedAction || format != "onnx")
    {
        return false;
    }

    std::getline(stream, path);
    path = Trim(path);

    if (path.size() >= 2 &&
        path.front() == '"' &&
        path.back() == '"')
    {
        path = path.substr(1, path.size() - 2);
    }

    return !path.empty();
}

void ImportOnnx(const std::string& command)
{
    using MiaIA::SDK::MiaIAClient;

    std::string path;

    if (!ReadOnnxPath(command, "import", path))
    {
        std::cout
            << "Usage: import onnx <path>\n";

        return;
    }

    if (!MiaIAClient::ImportOnnx(path))
    {
        std::cout
            << "ONNX import failed. The current network was not changed.\n";

        return;
    }

    std::cout
        << "ONNX model imported.\n";
}

void ExportOnnx(const std::string& command)
{
    using MiaIA::SDK::MiaIAClient;

    std::string path;

    if (!ReadOnnxPath(command, "export", path))
    {
        std::cout
            << "Usage: export onnx <path>\n";

        return;
    }

    if (!MiaIAClient::ExportOnnx(path))
    {
        std::cout
            << "ONNX export failed.\n";

        return;
    }

    std::cout
        << "ONNX model exported.\n";
}

std::string UnquotePath(const std::string& value)
{
    std::string path = Trim(value);

    if (path.size() >= 2 &&
        path.front() == '"' &&
        path.back() == '"')
    {
        path = path.substr(1, path.size() - 2);
    }

    return path;
}

bool ReadDatasetImport(
    const std::string& command,
    std::size_t& inputCount,
    std::size_t& targetCount,
    bool& hasHeader,
    std::string& path)
{
    std::stringstream stream(command);
    std::string datasetToken;
    std::string action;
    std::string format;

    if (!(stream >> datasetToken
        >> action
        >> format
        >> inputCount
        >> targetCount) ||
        datasetToken != "dataset" ||
        action != "import" ||
        format != "csv")
    {
        return false;
    }

    std::string remainder;
    std::getline(stream, remainder);
    remainder = Trim(remainder);
    hasHeader = true;

    constexpr const char* NoHeaderOption = "--no-header";

    if (remainder.rfind(NoHeaderOption, 0) == 0)
    {
        const std::size_t optionLength =
            std::char_traits<char>::length(NoHeaderOption);

        if (remainder.size() > optionLength &&
            remainder[optionLength] != ' ' &&
            remainder[optionLength] != '\t')
        {
            return false;
        }

        hasHeader = false;
        remainder = Trim(remainder.substr(optionLength));
    }

    path = UnquotePath(remainder);
    return inputCount > 0 && targetCount > 0 && !path.empty();
}

void PrintValues(const std::vector<double>& values)
{
    for (std::size_t index = 0; index < values.size(); ++index)
    {
        if (index > 0)
        {
            std::cout << ", ";
        }

        std::cout << values[index];
    }
}

void Predict(const std::string& command)
{
    using MiaIA::SDK::MiaIAClient;

    std::stringstream stream(command);
    std::string token;
    std::vector<double> inputs;
    double input{};

    stream >> token;

    while (stream >> input)
    {
        inputs.push_back(input);
    }

    if (!stream.eof())
    {
        std::cout
            << "Prediction input contains an invalid numeric value.\n";

        return;
    }

    if (inputs.empty())
    {
        std::cout
            << "Prediction requires at least one input value.\n";

        return;
    }

    MiaIA::Core::PredictionSnapshot prediction;

    if (!MiaIAClient::Predict(inputs, prediction))
    {
        std::cout
            << "Prediction failed. "
            << "Check the network and input dimensions.\n";

        return;
    }

    std::cout << "Prediction: ";
    PrintValues(prediction.Outputs);
    std::cout << "\n";
}

void HandleDatasetCommand(const std::string& command)
{
    using MiaIA::SDK::MiaIAClient;

    if (command.rfind("dataset import", 0) == 0)
    {
        std::size_t inputCount{};
        std::size_t targetCount{};
        bool hasHeader{};
        std::string path;

        if (!ReadDatasetImport(
            command,
            inputCount,
            targetCount,
            hasHeader,
            path))
        {
            std::cout
                << "Usage: dataset import csv <input-count> <output-count> "
                << "[--no-header] <path>\n";

            return;
        }

        if (!MiaIAClient::ImportCsvDataset(
            path,
            inputCount,
            targetCount,
            hasHeader))
        {
            std::cout
                << "CSV dataset import failed. "
                << "The current dataset was not changed.\n";

            return;
        }

        std::cout
            << "CSV dataset imported.\n";

        return;
    }

    if (command == "dataset summary")
    {
        const auto summary = MiaIAClient::GetDatasetSummary();

        if (summary.SampleCount == 0)
        {
            std::cout
                << "No dataset loaded.\n";

            return;
        }

        std::cout
            << "\nDataset Summary\n\n"
            << "Name: " << summary.Name << "\n"
            << "Source: " << summary.Source << "\n"
            << "Samples: " << summary.SampleCount << "\n"
            << "Inputs: " << summary.InputCount << "\n"
            << "Targets: " << summary.TargetCount << "\n";

        return;
    }

    if (command == "dataset clear")
    {
        MiaIAClient::ClearDataset();
        std::cout
            << "Dataset cleared.\n";

        return;
    }

    if (command.rfind("dataset evaluate", 0) == 0)
    {
        std::stringstream stream(command);
        std::string datasetToken;
        std::string action;
        std::string selector;
        std::string lossName;

        if (!(stream >> datasetToken >> action >> selector >> lossName) ||
            datasetToken != "dataset" ||
            action != "evaluate" ||
            lossName != "mse")
        {
            std::cout
                << "Usage: dataset evaluate <index|all> mse\n";

            return;
        }

        stream >> std::ws;

        if (!stream.eof())
        {
            std::cout
                << "Usage: dataset evaluate <index|all> mse\n";

            return;
        }

        if (selector == "all")
        {
            MiaIA::Core::DatasetEvaluationSnapshot evaluation;

            if (!MiaIAClient::EvaluateDataset(
                MiaIA::Core::LossType::MeanSquaredError,
                evaluation))
            {
                std::cout
                    << "Dataset evaluation failed. "
                    << "Check the dataset, network and dimensions.\n";

                return;
            }

            std::cout
                << "\nDataset Evaluation"
                << "\nSamples: " << evaluation.SampleCount
                << "\nMean squared error: " << evaluation.MeanLoss
                << "\n\nSample Losses\n";

            for (const auto& sample : evaluation.Evaluations)
            {
                std::cout
                    << "Sample " << sample.SampleIndex
                    << " Loss " << sample.Loss
                    << " Predictions ";
                PrintValues(sample.Predictions);
                std::cout << "\n";
            }

            std::cout << "\n";
            return;
        }

        std::stringstream indexStream(selector);
        std::size_t index{};

        if (!(indexStream >> index))
        {
            std::cout
                << "Usage: dataset evaluate <index|all> mse\n";

            return;
        }

        indexStream >> std::ws;

        if (!indexStream.eof())
        {
            std::cout
                << "Usage: dataset evaluate <index|all> mse\n";

            return;
        }

        MiaIA::Core::SampleEvaluationSnapshot evaluation;

        if (!MiaIAClient::EvaluateDatasetSample(
            index,
            MiaIA::Core::LossType::MeanSquaredError,
            evaluation))
        {
            std::cout
                << "Dataset sample evaluation failed. "
                << "Check the sample, network and output dimensions.\n";

            return;
        }

        std::cout
            << "\nSample Evaluation " << evaluation.SampleIndex << "\n"
            << "Targets: ";
        PrintValues(evaluation.Targets);
        std::cout << "\nPredictions: ";
        PrintValues(evaluation.Predictions);
        std::cout << "\nErrors (prediction - target): ";
        PrintValues(evaluation.Errors);
        std::cout
            << "\nMean squared error: "
            << evaluation.Loss
            << "\n";

        return;
    }

    if (command.rfind("dataset gradients", 0) == 0)
    {
        std::stringstream stream(command);
        std::string datasetToken;
        std::string action;
        std::size_t index{};
        std::string lossName;

        if (!(stream >> datasetToken >> action >> index >> lossName) ||
            datasetToken != "dataset" ||
            action != "gradients" ||
            lossName != "mse")
        {
            std::cout
                << "Usage: dataset gradients <index> mse\n";

            return;
        }

        stream >> std::ws;

        if (!stream.eof())
        {
            std::cout
                << "Usage: dataset gradients <index> mse\n";

            return;
        }

        MiaIA::Core::SampleGradientSnapshot gradients;

        if (!MiaIAClient::EvaluateDatasetSampleGradients(
            index,
            MiaIA::Core::LossType::MeanSquaredError,
            gradients))
        {
            std::cout
                << "Dataset sample gradient evaluation failed. "
                << "Check the sample, network and output dimensions.\n";

            return;
        }

        std::cout
            << "\nSample Gradients "
            << gradients.Evaluation.SampleIndex
            << "\nLoss: "
            << gradients.Evaluation.Loss
            << "\n\nNeuron Gradients\n";

        for (const auto& neuron : gradients.Neurons)
        {
            std::cout
                << "Neuron " << neuron.Id
                << " Layer " << neuron.LayerOrder
                << " dLoss/dActivation " << neuron.ActivationGradient
                << " dLoss/dPreActivation "
                << neuron.PreActivationGradient
                << " dLoss/dBias " << neuron.BiasGradient
                << "\n";
        }

        std::cout << "\nConnection Gradients\n";

        for (const auto& connection : gradients.Connections)
        {
            std::cout
                << "Connection " << connection.Id
                << " " << connection.FromNeuron
                << " -> " << connection.ToNeuron
                << " dLoss/dWeight " << connection.WeightGradient
                << "\n";
        }

        return;
    }

    std::stringstream stream(command);
    std::string datasetToken;
    std::string action;
    std::size_t index{};

    if (!(stream >> datasetToken >> action >> index) ||
        datasetToken != "dataset")
    {
        std::cout
            << "Unknown dataset command.\n";

        return;
    }

    stream >> std::ws;

    if (!stream.eof())
    {
        std::cout
            << "Dataset sample index is invalid.\n";

        return;
    }

    if (action == "inspect")
    {
        MiaIA::Core::SampleSnapshot sample;

        if (!MiaIAClient::TryGetDatasetSample(index, sample))
        {
            std::cout
                << "Dataset sample was not found.\n";

            return;
        }

        std::cout
            << "\nSample " << sample.Index << "\n"
            << "Inputs: ";
        PrintValues(sample.Inputs);
        std::cout << "\nTargets: ";
        PrintValues(sample.Targets);
        std::cout << "\n";

        return;
    }

    if (action == "apply")
    {
        if (!MiaIAClient::ApplyDatasetSample(index))
        {
            std::cout
                << "Dataset sample could not be applied to the network.\n";

            return;
        }

        std::cout
            << "Dataset sample applied to the network input.\n";

        return;
    }

    std::cout
        << "Unknown dataset command.\n";
}

const char* TrainingSessionStatusName(
    MiaIA::Core::TrainingSessionStatus status)
{
    switch (status)
    {
    case MiaIA::Core::TrainingSessionStatus::Idle:
        return "Idle";
    case MiaIA::Core::TrainingSessionStatus::Active:
        return "Active";
    case MiaIA::Core::TrainingSessionStatus::Completed:
        return "Completed";
    case MiaIA::Core::TrainingSessionStatus::Cancelled:
        return "Cancelled";
    }

    return "Unknown";
}

void PrintTrainingSession(
    const MiaIA::Core::TrainingSessionSnapshot& session)
{
    std::cout
        << "\nTraining Session"
        << "\nStatus: " << TrainingSessionStatusName(session.Status)
        << "\nEpochs: " << session.CurrentEpoch
        << " / " << session.EpochCount
        << "\nSteps: " << session.CompletedSteps
        << " / " << session.TotalSteps;

    if (session.Status == MiaIA::Core::TrainingSessionStatus::Active)
    {
        std::cout
            << "\nCurrent epoch: " << session.CurrentEpoch + 1
            << "\nNext sample: " << session.NextSampleIndex
            << "\nLearning rate: " << session.LearningRate
            << "\nOptimizer: SGD";
    }

    std::cout << "\n";
}

void PrintTrainingSessionUsage()
{
    std::cout
        << "Usage: train session start <epochs> <learning-rate> mse\n"
        << "       train session status\n"
        << "       train session next\n"
        << "       train session cancel\n";
}

void HandleTrainCommand(const std::string& command)
{
    using MiaIA::SDK::MiaIAClient;

    std::stringstream stream(command);
    std::string trainToken;
    std::string action;
    if (!(stream >> trainToken >> action) || trainToken != "train")
    {
        std::cout
            << "Usage: train step <sample-index> <learning-rate> mse\n"
            << "       train epoch <learning-rate> mse\n"
            << "       train session <start|status|next|cancel> ...\n";

        return;
    }

    if (action == "session")
    {
        std::string sessionAction;

        if (!(stream >> sessionAction))
        {
            PrintTrainingSessionUsage();
            return;
        }

        if (sessionAction == "start")
        {
            std::size_t epochCount{};
            double learningRate{};
            std::string lossName;

            if (!(stream >> epochCount >> learningRate >> lossName) ||
                lossName != "mse")
            {
                PrintTrainingSessionUsage();
                return;
            }

            stream >> std::ws;

            if (!stream.eof())
            {
                PrintTrainingSessionUsage();
                return;
            }

            MiaIA::Core::TrainingSessionSnapshot session;

            if (!MiaIAClient::StartTrainingSession(
                epochCount,
                learningRate,
                MiaIA::Core::LossType::MeanSquaredError,
                MiaIA::Core::OptimizerType::StochasticGradientDescent,
                session))
            {
                std::cout
                    << "Training session could not be started. "
                    << "Check its configuration, dataset, network, "
                    << "or active session state.\n";

                return;
            }

            PrintTrainingSession(session);
            return;
        }

        stream >> std::ws;

        if (!stream.eof())
        {
            PrintTrainingSessionUsage();
            return;
        }

        if (sessionAction == "status")
        {
            PrintTrainingSession(MiaIAClient::GetTrainingSession());
            return;
        }

        if (sessionAction == "next")
        {
            MiaIA::Core::TrainingStepSnapshot step;

            if (!MiaIAClient::AdvanceTrainingSession(step))
            {
                std::cout
                    << "Training session step failed. "
                    << "The network and session position were not changed.\n";

                return;
            }

            std::cout
                << "\nSession Step"
                << "\nSample: " << step.SampleIndex
                << "\nLoss before: " << step.Before.Evaluation.Loss
                << "\nLoss after: " << step.After.Loss
                << "\n";
            PrintTrainingSession(MiaIAClient::GetTrainingSession());
            return;
        }

        if (sessionAction == "cancel")
        {
            if (!MiaIAClient::CancelTrainingSession())
            {
                std::cout << "No active training session to cancel.\n";
                return;
            }

            PrintTrainingSession(MiaIAClient::GetTrainingSession());
            return;
        }

        PrintTrainingSessionUsage();
        return;
    }

    if (action == "epoch")
    {
        double learningRate{};
        std::string lossName;

        if (!(stream >> learningRate >> lossName) || lossName != "mse")
        {
            std::cout
                << "Usage: train epoch <learning-rate> mse\n";

            return;
        }

        stream >> std::ws;

        if (!stream.eof())
        {
            std::cout
                << "Usage: train epoch <learning-rate> mse\n";

            return;
        }

        MiaIA::Core::TrainingEpochSnapshot epoch;

        if (!MiaIAClient::TrainDatasetEpoch(
            learningRate,
            MiaIA::Core::LossType::MeanSquaredError,
            MiaIA::Core::OptimizerType::StochasticGradientDescent,
            epoch))
        {
            std::cout
                << "Training epoch failed. "
                << "The network parameters were not changed.\n";

            return;
        }

        std::cout
            << "\nTraining Epoch"
            << "\nSamples: " << epoch.SampleCount
            << "\nOptimizer: SGD"
            << "\nLearning rate: " << epoch.LearningRate
            << "\nMean loss before update: "
            << epoch.MeanLossBeforeUpdate
            << "\nMean loss after update: "
            << epoch.MeanLossAfterUpdate
            << "\n\nSample Losses\n";

        for (const auto& step : epoch.Steps)
        {
            std::cout
                << "Sample " << step.SampleIndex
                << " Before " << step.Before.Evaluation.Loss
                << " After " << step.After.Loss
                << "\n";
        }

        std::cout << "\n";
        return;
    }

    std::size_t sampleIndex{};
    double learningRate{};
    std::string lossName;

    if (action != "step" ||
        !(stream >> sampleIndex >> learningRate >> lossName) ||
        lossName != "mse")
    {
        std::cout
            << "Usage: train step <sample-index> <learning-rate> mse\n"
            << "       train epoch <learning-rate> mse\n"
            << "       train session <start|status|next|cancel> ...\n";

        return;
    }

    stream >> std::ws;

    if (!stream.eof())
    {
        std::cout
            << "Usage: train step <sample-index> <learning-rate> mse\n";

        return;
    }

    MiaIA::Core::TrainingStepSnapshot step;

    if (!MiaIAClient::TrainDatasetSample(
        sampleIndex,
        learningRate,
        MiaIA::Core::LossType::MeanSquaredError,
        MiaIA::Core::OptimizerType::StochasticGradientDescent,
        step))
    {
        std::cout
            << "Training step failed. "
            << "The network parameters were not changed.\n";

        return;
    }

    std::cout
        << "\nTraining Step " << step.SampleIndex
        << "\nOptimizer: SGD"
        << "\nLearning rate: " << step.LearningRate
        << "\nLoss before: " << step.Before.Evaluation.Loss
        << "\nLoss after: " << step.After.Loss
        << "\n\nConnection Updates\n";

    for (const auto& update : step.ConnectionUpdates)
    {
        std::cout
            << "Connection " << update.Id
            << " Weight " << update.PreviousWeight
            << " Gradient " << update.Gradient
            << " Delta " << update.Delta
            << " Updated " << update.UpdatedWeight
            << "\n";
    }

    std::cout << "\nBias Updates\n";

    for (const auto& update : step.NeuronUpdates)
    {
        std::cout
            << "Neuron " << update.Id
            << " Bias " << update.PreviousBias
            << " Gradient " << update.Gradient
            << " Delta " << update.Delta
            << " Updated " << update.UpdatedBias
            << "\n";
    }
}


void PrintSummary()
{
    using MiaIA::SDK::MiaIAClient;

    const auto snapshot =
        MiaIAClient::GetSnapshot();


    std::cout
        << "\nNetwork Summary\n\n";


    std::cout
        << "Layers: "
        << snapshot.Layers.size()
        << "\n";


    for (const auto& layer : snapshot.Layers)
    {
        std::cout
            << "  "
            << layer.Order
            << " "
            << layer.Name
            << " ("
            << layer.Neurons.size()
            << " neurons)\n";
    }


    std::cout
        << "\nConnections: "
        << snapshot.Connections.size()
        << "\n\n";
}

void InspectNetwork()
{
    using MiaIA::SDK::MiaIAClient;

    const auto snapshot =
        MiaIAClient::GetSnapshot();


    std::cout
        << "\nNetwork Inspection\n\n";


    for (const auto& layer : snapshot.Layers)
    {
        std::cout
            << "Layer ["
            << layer.Order
            << "] "
            << layer.Name
            << "\n";


        for (const auto& neuron : layer.Neurons)
        {
            std::cout
                << "  Neuron "
                << neuron.Id
                << "\n"
                << "    Bias: "
                << neuron.Bias
                << "\n"
                << "    Activation: "
                << neuron.Activation
                << "\n";
        }

        std::cout << "\n";
    }


    std::cout
        << "Connections\n\n";


    for (const auto& connection :
        snapshot.Connections)
    {
        std::cout
            << "  Connection "
            << connection.Id
            << "\n"
            << "    "
            << connection.FromNeuron
            << " -> "
            << connection.ToNeuron
            << "\n"
            << "    Weight: "
            << connection.Weight
            << "\n\n";
    }
}

void RunForward()
{
    using MiaIA::SDK::MiaIAClient;

    const bool result =
        MiaIAClient::Forward();


    if (!result)
    {
        std::cout
            << "Forward failed.\n";

        return;
    }


    std::cout
        << "\nForward completed\n\n";


    const auto snapshot =
        MiaIAClient::GetSnapshot();


    std::cout
        << "Output Layer\n\n";


    for (const auto& layer : snapshot.Layers)
    {
        if (layer.Name == "Output")
        {
            for (const auto& neuron : layer.Neurons)
            {
                std::cout
                    << "  Neuron "
                    << neuron.Id
                    << "\n"
                    << "    Activation: "
                    << neuron.Activation
                    << "\n\n";
            }

            return;
        }
    }


    std::cout
        << "Output layer not found.\n";
}

void RunBenchmark()
{
    using namespace std::chrono;

    using MiaIA::SDK::MiaIAClient;


    const int iterations = 10000;


    std::cout
        << "\nBenchmark\n\n";

    std::cout
        << "Forward iterations: "
        << iterations
        << "\n\n";


    const auto start =
        high_resolution_clock::now();


    for (int i = 0; i < iterations; i++)
    {
        MiaIAClient::Forward();
    }


    const auto end =
        high_resolution_clock::now();


    const auto elapsed =
        duration_cast<microseconds>(
            end - start);


    const double totalMs =
        elapsed.count() / 1000.0;


    const double average =
        totalMs / iterations;


    std::cout
        << "Completed\n\n";

    std::cout
        << "Total time: "
        << totalMs
        << " ms\n";

    std::cout
        << "Average: "
        << average
        << " ms\n";
}

std::string ResolveCommand(
    const std::string& input)
{
    const std::vector<std::string> commands =
    {
        "help",
        "create",
        "input",
        "predict",
        "import",
        "export",
        "dataset",
        "train",
        "summary",
        "inspect",
        "forward",
        "benchmark",
        "exit"
    };


    std::string match;


    for (const auto& command : commands)
    {
        if (command.rfind(input, 0) == 0)
        {
            if (!match.empty())
            {
                return "";
            }

            match = command;
        }
    }


    return match;
}

int main()
{
    std::cout
        << "MiaIA Console v0.1\n";


    std::string command;


    while (true)
    {
        std::cout << "\n> ";

        std::getline(
            std::cin,
            command);

        command = Trim(command);

        if (command.empty())
        {
            continue;
        }

       

        const std::string resolved =
            ResolveCommand(command);

        if (!resolved.empty())
        {
            command = resolved;
        }

        if (command == "help")
        {
            PrintHelp();
        }
        else if (command.rfind("create", 0) == 0)
        {
            CreateNetwork(command);
            continue;
        }
        else if (command.rfind("input", 0) == 0)
        {
            SetInput(command);
            continue;
        }
        else if (command.rfind("predict", 0) == 0)
        {
            Predict(command);
            continue;
        }
        else if (command.rfind("import", 0) == 0)
        {
            ImportOnnx(command);
            continue;
        }
        else if (command.rfind("export", 0) == 0)
        {
            ExportOnnx(command);
            continue;
        }
        else if (command.rfind("dataset", 0) == 0)
        {
            HandleDatasetCommand(command);
            continue;
        }
        else if (command.rfind("train", 0) == 0)
        {
            HandleTrainCommand(command);
            continue;
        }
        else if (command == "summary")
        {
            PrintSummary();
        }
        else if (command == "inspect")
        {
            InspectNetwork();
        }
        else if (command == "forward")
        {
            RunForward();
        }
        else if (command == "benchmark")
        {
            RunBenchmark();
        }
        else if (command == "exit")
        {
            break;
        }
        else
        {
            std::cout
                << "Unknown command\n";
        }
    }


    return 0;
}
