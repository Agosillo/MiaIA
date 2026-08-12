#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <string>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <vector>
#include <sstream>
#include <filesystem>
#include <mutex>

#include "../Include/MiaIACommandProcessor.h"
#include "../../SDK/Include/MiaIAClient.h"

namespace
{
    std::mutex CommandExecutionMutex;
    thread_local std::filesystem::path CommandWorkingDirectory;

    class ScopedCommandOutput final
    {
    public:
        explicit ScopedCommandOutput(std::ostream& output)
            : PreviousBuffer(std::cout.rdbuf(output.rdbuf()))
        {
        }

        ~ScopedCommandOutput()
        {
            std::cout.rdbuf(PreviousBuffer);
        }

        ScopedCommandOutput(const ScopedCommandOutput&) = delete;
        ScopedCommandOutput& operator=(const ScopedCommandOutput&) = delete;

    private:
        std::streambuf* PreviousBuffer;
    };

    class ScopedWorkingDirectory final
    {
    public:
        explicit ScopedWorkingDirectory(const std::string& directory)
            : PreviousDirectory(CommandWorkingDirectory)
        {
            CommandWorkingDirectory = directory;
        }

        ~ScopedWorkingDirectory()
        {
            CommandWorkingDirectory = PreviousDirectory;
        }

        ScopedWorkingDirectory(const ScopedWorkingDirectory&) = delete;
        ScopedWorkingDirectory& operator=(
            const ScopedWorkingDirectory&) = delete;

    private:
        std::filesystem::path PreviousDirectory;
    };

    std::string ResolvePath(const std::string& value)
    {
        std::filesystem::path path(value);

        if (path.is_relative() && !CommandWorkingDirectory.empty())
        {
            path = CommandWorkingDirectory / path;
        }

        return path.lexically_normal().string();
    }
}

namespace
{

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

struct CommandCatalogEntry
{
    const char* Completion;
    const char* Syntax;
    const char* Description;
    bool AcceptsArguments;
};

const std::vector<CommandCatalogEntry>& CommandCatalog()
{
    static const std::vector<CommandCatalogEntry> entries =
    {
        { "help", "help", "Show every available command.", true },
        { "create", "create <inputs> <hidden-width> <hidden-layers> <outputs> [--hidden-activation <type>] [--output-activation <type>] [--weight <value>] [--bias <value>]", "Create a configurable dense feed-forward network.", true },
        { "network", "network <configure|set>", "Modify parameters of the current network.", false },
        { "network configure", "network configure [--hidden-activation <type>] [--output-activation <type>] [--weight <value>] [--bias <value>]", "Atomically update existing network parameters.", true },
        { "network set", "network set <neuron-bias|connection-weight>", "Modify one identified network parameter.", false },
        { "network set neuron-bias", "network set neuron-bias <neuron-id> <value>", "Set one non-input neuron bias.", true },
        { "network set connection-weight", "network set connection-weight <connection-id> <value>", "Set one connection weight.", true },
        { "input", "input <value...>", "Assign values to the input layer.", true },
        { "predict", "predict <value...>", "Run inference for one input vector.", true },
        { "import", "import onnx <path>", "Import a model from a supported format.", false },
        { "import onnx", "import onnx <path>", "Import the supported dense ONNX subset.", true },
        { "export", "export onnx <path>", "Export the current model.", false },
        { "export onnx", "export onnx <path>", "Export the current model to ONNX.", true },
        { "project", "project <new|open|save|info>", "Create, persist, or inspect a MiaIA project.", false },
        { "project new", "project new", "Clear runtime state and begin an unsaved project.", true },
        { "project open", "project open <path.mai>", "Open a versioned MiaIA project archive.", true },
        { "project save", "project save [path.mai]", "Save to a new path or the current project path.", true },
        { "project info", "project info", "Show current project, dataset, training, and breakpoint metadata.", true },
        { "dataset", "dataset <action>", "Import, inspect, evaluate, or clear samples.", false },
        { "dataset import", "dataset import csv <inputs> <targets> [--no-header] <path>", "Import dataset samples.", false },
        { "dataset import csv", "dataset import csv <inputs> <targets> [--no-header] <path>", "Import a numeric CSV dataset.", true },
        { "dataset summary", "dataset summary", "Show the loaded dataset dimensions.", true },
        { "dataset inspect", "dataset inspect <sample-index>", "Inspect one dataset sample.", true },
        { "dataset apply", "dataset apply <sample-index>", "Apply one sample to the network input.", true },
        { "dataset evaluate", "dataset evaluate <sample-index|all> mse", "Evaluate one sample or the fixed model.", true },
        { "dataset gradients", "dataset gradients <sample-index> mse", "Calculate gradients without updating parameters.", true },
        { "dataset clear", "dataset clear", "Clear the current dataset.", true },
        { "train", "train <step|epoch|debug|session|breakpoint>", "Run or inspect training operations.", false },
        { "train breakpoint", "train breakpoint <action>", "Create and manage safe training breakpoints.", false },
        { "train breakpoint add", "train breakpoint add <condition> ...", "Add a phase or threshold breakpoint.", false },
        { "train breakpoint add phase", "train breakpoint add phase <before|forward|backward|update|verify|commit>", "Break when debug reaches a mathematical phase.", true },
        { "train breakpoint add activation-above", "train breakpoint add activation-above <neuron-id> <threshold>", "Break when a neuron activation exceeds a threshold.", true },
        { "train breakpoint add activation-below", "train breakpoint add activation-below <neuron-id> <threshold>", "Break when a neuron activation falls below a threshold.", true },
        { "train breakpoint add gradient-above", "train breakpoint add gradient-above <neuron-id> <magnitude>", "Break when a neuron bias-gradient magnitude exceeds a threshold.", true },
        { "train breakpoint add weight-update-above", "train breakpoint add weight-update-above <connection-id> <magnitude>", "Break when a connection update magnitude exceeds a threshold.", true },
        { "train breakpoint list", "train breakpoint list", "List configured breakpoints and hit counts.", true },
        { "train breakpoint enable", "train breakpoint enable <id> <on|off>", "Enable or disable one breakpoint.", true },
        { "train breakpoint remove", "train breakpoint remove <id>", "Remove one breakpoint.", true },
        { "train breakpoint clear", "train breakpoint clear", "Remove every training breakpoint.", true },
        { "train step", "train step <sample-index> <learning-rate> mse", "Execute one atomic SGD sample update.", true },
        { "train epoch", "train epoch <learning-rate> mse", "Execute one ordered atomic dataset epoch.", true },
        { "train debug", "train debug <action>", "Control a phase-by-phase sample transaction.", false },
        { "train debug start", "train debug start <sample-index> <learning-rate> mse", "Start a standalone debug transaction.", true },
        { "train debug status", "train debug status", "Show the current debug phase.", true },
        { "train debug next", "train debug next", "Advance exactly one mathematical phase.", true },
        { "train debug neuron", "train debug neuron <neuron-id>", "Inspect one neuron in the current phase.", true },
        { "train debug connection", "train debug connection <connection-id>", "Inspect one connection in the current phase.", true },
        { "train debug cancel", "train debug cancel", "Discard the current candidate transaction.", true },
        { "train session", "train session <action>", "Control a multi-epoch training session.", false },
        { "train session start", "train session start <epochs> <learning-rate> mse", "Start a paused controlled session.", true },
        { "train session status", "train session status", "Show session state and progress.", true },
        { "train session next", "train session next", "Execute the next atomic session sample.", true },
        { "train session run", "train session run <steps|all>", "Run a bounded synchronous block.", true },
        { "train session resume", "train session resume", "Start background session execution.", true },
        { "train session pause", "train session pause", "Pause at the next safe sample boundary.", true },
        { "train session debug", "train session debug", "Attach phase debugging to the next sample.", true },
        { "train session history", "train session history", "List completed session steps.", true },
        { "train session inspect", "train session inspect <step-index>", "Inspect one retained training step.", true },
        { "train session compare", "train session compare <first-step-index> <second-step-index> [maximum-items]", "Compare two retained training steps.", true },
        { "train session cancel", "train session cancel", "Cancel future session execution.", true },
        { "summary", "summary", "Show the current network structure.", true },
        { "inspect", "inspect", "Inspect current network values.", true },
        { "inspect neuron", "inspect neuron <neuron-id> [maximum-connections]", "Inspect one neuron and its relationships.", true },
        { "inspect connection", "inspect connection <connection-id>", "Inspect one connection and both endpoint neurons.", true },
        { "forward", "forward", "Run forward propagation with assigned inputs.", true },
        { "benchmark", "benchmark", "Benchmark repeated forward execution.", true },
        { "exit", "exit", "Exit the standalone terminal host.", true }
    };

    return entries;
}

std::vector<std::string> CommandTokens(const std::string& value)
{
    std::stringstream stream(value);
    std::vector<std::string> tokens;
    std::string token;

    while (stream >> token)
    {
        tokens.push_back(token);
    }

    return tokens;
}

bool StartsWith(const std::string& value, const std::string& prefix)
{
    return value.size() >= prefix.size() &&
        value.compare(0, prefix.size(), prefix) == 0;
}


void PrintHelp()
{
    std::cout
        << "\nMiaIA Console v0.1\n\n"
        << "Commands:\n\n"

        << "  help\n"
        << "      Show available commands\n\n"

        << "  create <inputs> <neurons-per-hidden-layer> <hidden-layers> <outputs>\n"
        << "      [--hidden-activation sigmoid|relu|tanh|linear]\n"
        << "      [--output-activation sigmoid|relu|tanh|linear]\n"
        << "      [--weight <initial-weight>] [--bias <initial-bias>]\n"
        << "      Create a dense neural network\n\n"
        << "      Example:\n"
        << "        create 784 256 3 10 --hidden-activation relu "
           "--output-activation sigmoid\n\n"

        << "  network configure [options]\n"
        << "      --hidden-activation sigmoid|relu|tanh|linear\n"
        << "      --output-activation sigmoid|relu|tanh|linear\n"
        << "      --weight <value> --bias <value>\n"
        << "      Atomically update parameters of the current network\n"
        << "      Warning: weight or bias updates replace learned values\n\n"
        << "      Example:\n"
        << "        network configure --hidden-activation tanh "
           "--weight 0.01\n\n"

        << "  network set neuron-bias <neuron-id> <value>\n"
        << "      Set one hidden or output neuron bias\n"
        << "      Input neuron biases are not model parameters\n\n"

        << "  network set connection-weight <connection-id> <value>\n"
        << "      Set one connection weight\n\n"

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

        << "  project new\n"
        << "      Clear the current state and begin an unsaved project\n\n"

        << "  project open <path.mai>\n"
        << "      Open a MiaIA project containing an ONNX model, dataset reference,\n"
        << "      training configuration, and safe breakpoints\n\n"

        << "  project save [path.mai]\n"
        << "      Save atomically to a new path or the current project path\n\n"

        << "  project info\n"
        << "      Show the current project and restored component status\n\n"

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

        << "  train debug start <sample-index> <learning-rate> mse\n"
        << "      Start a phase-by-phase SGD training transaction\n\n"

        << "  train debug status\n"
        << "      Inspect the current mathematical debug phase\n\n"

        << "  train debug next\n"
        << "      Advance exactly one mathematical phase\n\n"

        << "  train debug neuron <neuron-id>\n"
        << "      Inspect one neuron in the current debug phase\n\n"

        << "  train debug connection <connection-id>\n"
        << "      Inspect one connection in the current debug phase\n\n"

        << "  train debug cancel\n"
        << "      Discard the candidate network before commit\n\n"

        << "  train epoch <learning-rate> mse\n"
        << "      Train all dataset samples in order as one atomic epoch\n\n"

        << "  train breakpoint add phase <phase>\n"
        << "  train breakpoint add activation-above <neuron-id> <threshold>\n"
        << "  train breakpoint add activation-below <neuron-id> <threshold>\n"
        << "  train breakpoint add gradient-above <neuron-id> <magnitude>\n"
        << "  train breakpoint add weight-update-above <connection-id> <magnitude>\n"
        << "      Pause controlled training when a condition is reached\n\n"

        << "  train breakpoint list\n"
        << "  train breakpoint enable <id> <on|off>\n"
        << "  train breakpoint remove <id>\n"
        << "  train breakpoint clear\n"
        << "      Inspect or edit the breakpoint collection\n\n"

        << "  train session start <epochs> <learning-rate> mse\n"
        << "      Start a manually controlled training session\n\n"

        << "  train session status\n"
        << "      Show controlled training progress\n\n"

        << "  train session debug\n"
        << "      Debug the session's next sample phase by phase\n\n"

        << "  train session history\n"
        << "      List every completed session step\n\n"

        << "  train session inspect <step-index>\n"
        << "      Show gradients and updates for one completed step\n\n"

        << "  train session compare <first-step-index> "
           "<second-step-index> [maximum-items]\n"
        << "      Compare two completed steps and rank their changes\n\n"

        << "  train session next\n"
        << "      Execute exactly one sample training step\n\n"

        << "  train session run <steps|all>\n"
        << "      Execute a bounded block or all remaining steps\n\n"

        << "  train session resume\n"
        << "      Continue training on a background worker\n\n"

        << "  train session pause\n"
        << "      Stop the worker after its current atomic step\n\n"

        << "  train session cancel\n"
        << "      Stop the session without reverting completed steps\n\n"

        << "  summary\n"
        << "      Show network overview\n\n"

        << "  inspect\n"
        << "      Show layers, neurons and connections\n\n"

        << "  inspect neuron <neuron-id> [maximum-connections]\n"
        << "      Show one neuron and bounded incoming/outgoing relationships\n\n"

        << "  inspect connection <connection-id>\n"
        << "      Show one connection and both endpoint neurons\n\n"

        << "  forward\n"
        << "      Execute forward propagation\n\n"

        << "  benchmark\n"
        << "      Measure forward performance\n\n"

        << "  exit\n"
        << "      Close console\n\n";
}


const char* ActivationTypeName(MiaIA::Core::ActivationType activation);

bool TryParseActivation(
    std::string value,
    MiaIA::Core::ActivationType& result)
{
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](unsigned char character)
        {
            return static_cast<char>(std::tolower(character));
        });

    if (value == "sigmoid")
    {
        result = MiaIA::Core::ActivationType::Sigmoid;
        return true;
    }

    if (value == "relu")
    {
        result = MiaIA::Core::ActivationType::ReLU;
        return true;
    }

    if (value == "tanh")
    {
        result = MiaIA::Core::ActivationType::Tanh;
        return true;
    }

    if (value == "linear")
    {
        result = MiaIA::Core::ActivationType::Linear;
        return true;
    }

    return false;
}

void PrintCreateUsage()
{
    std::cout
        << "Usage: create <inputs> <hidden-width> <hidden-layers> "
           "<outputs> [--hidden-activation <type>] "
           "[--output-activation <type>] [--weight <value>] "
           "[--bias <value>]\n"
        << "Activation types: sigmoid, relu, tanh, linear\n";
}

void CreateNetwork(const std::string& command)
{
    using MiaIA::SDK::MiaIAClient;

    int inputCount = 10;
    int hiddenCount = 32;
    int hiddenLayers = 2;
    int outputCount = 3;
    MiaIA::Core::DenseNetworkConfiguration configuration;
    std::stringstream stream(command);
    std::string token;
    stream >> token;
    stream >> std::ws;

    if (!stream.eof() &&
        !(stream >> inputCount
            >> hiddenCount
            >> hiddenLayers
            >> outputCount))
    {
        PrintCreateUsage();
        return;
    }

    std::string option;

    while (stream >> option)
    {
        if (option == "--hidden-activation")
        {
            std::string activation;

            if (!(stream >> activation) ||
                !TryParseActivation(
                    activation,
                    configuration.HiddenActivation))
            {
                PrintCreateUsage();
                return;
            }
        }
        else if (option == "--output-activation")
        {
            std::string activation;

            if (!(stream >> activation) ||
                !TryParseActivation(
                    activation,
                    configuration.OutputActivation))
            {
                PrintCreateUsage();
                return;
            }
        }
        else if (option == "--weight")
        {
            if (!(stream >> configuration.InitialWeight))
            {
                PrintCreateUsage();
                return;
            }
        }
        else if (option == "--bias")
        {
            if (!(stream >> configuration.InitialBias))
            {
                PrintCreateUsage();
                return;
            }
        }
        else
        {
            PrintCreateUsage();
            return;
        }
    }

    if (MiaIAClient::CreateDenseNetwork(
        inputCount,
        hiddenCount,
        hiddenLayers,
        outputCount,
        configuration))
    {
        std::cout
            << "Dense network created.\n"
            << "Hidden activation: "
            << ActivationTypeName(configuration.HiddenActivation)
            << "\nOutput activation: "
            << ActivationTypeName(configuration.OutputActivation)
            << "\nInitial weight: " << configuration.InitialWeight
            << "\nInitial non-input bias: "
            << configuration.InitialBias << "\n";
    }
    else
    {
        std::cout
            << "Network creation failed.\n";
    }
}

void PrintNetworkConfigureUsage()
{
    std::cout
        << "Usage: network configure "
           "[--hidden-activation <type>] "
           "[--output-activation <type>] [--weight <value>] "
           "[--bias <value>]\n"
        << "Activation types: sigmoid, relu, tanh, linear\n";
}

void PrintNetworkSetUsage()
{
    std::cout
        << "Usage: network set neuron-bias <neuron-id> <value>\n"
        << "   or: network set connection-weight <connection-id> <value>\n";
}

void ConfigureNetwork(const std::string& command);

void SetNetworkParameter(const std::string& command)
{
    using MiaIA::SDK::MiaIAClient;

    std::stringstream stream(command);
    std::string networkToken;
    std::string setToken;
    std::string parameterToken;
    std::uint64_t id{};
    double value{};
    std::string trailingToken;

    if (!(stream >> networkToken >> setToken >> parameterToken >> id >> value) ||
        networkToken != "network" || setToken != "set" ||
        (stream >> trailingToken))
    {
        PrintNetworkSetUsage();
        return;
    }

    bool updated{};

    if (parameterToken == "neuron-bias")
    {
        updated = MiaIAClient::SetNeuronBias(id, value);
    }
    else if (parameterToken == "connection-weight")
    {
        updated = MiaIAClient::SetConnectionWeight(id, value);
    }
    else
    {
        PrintNetworkSetUsage();
        return;
    }

    if (!updated)
    {
        std::cout
            << "Network parameter update failed. Check the element ID, "
               "finite value, non-input bias rule and training/debug state.\n";
        return;
    }

    std::cout
        << (parameterToken == "neuron-bias"
            ? "Neuron bias updated.\n"
            : "Connection weight updated.\n");
}

void HandleNetworkCommand(const std::string& command)
{
    if (command == "network configure" ||
        command.rfind("network configure ", 0) == 0)
    {
        ConfigureNetwork(command);
        return;
    }

    if (command == "network set" ||
        command.rfind("network set ", 0) == 0)
    {
        SetNetworkParameter(command);
        return;
    }

    PrintNetworkSetUsage();
}

void ConfigureNetwork(const std::string& command)
{
    using MiaIA::SDK::MiaIAClient;

    std::stringstream stream(command);
    std::string networkToken;
    std::string configureToken;
    stream >> networkToken >> configureToken;

    if (networkToken != "network" || configureToken != "configure")
    {
        PrintNetworkConfigureUsage();
        return;
    }

    MiaIA::Core::NetworkParameterUpdate update;
    std::string option;

    while (stream >> option)
    {
        if (option == "--hidden-activation")
        {
            std::string activation;
            MiaIA::Core::ActivationType parsedActivation;

            if (!(stream >> activation) ||
                !TryParseActivation(activation, parsedActivation))
            {
                PrintNetworkConfigureUsage();
                return;
            }

            update.HiddenActivation = parsedActivation;
        }
        else if (option == "--output-activation")
        {
            std::string activation;
            MiaIA::Core::ActivationType parsedActivation;

            if (!(stream >> activation) ||
                !TryParseActivation(activation, parsedActivation))
            {
                PrintNetworkConfigureUsage();
                return;
            }

            update.OutputActivation = parsedActivation;
        }
        else if (option == "--weight")
        {
            double value{};

            if (!(stream >> value))
            {
                PrintNetworkConfigureUsage();
                return;
            }

            update.ConnectionWeight = value;
        }
        else if (option == "--bias")
        {
            double value{};

            if (!(stream >> value))
            {
                PrintNetworkConfigureUsage();
                return;
            }

            update.NonInputBias = value;
        }
        else
        {
            PrintNetworkConfigureUsage();
            return;
        }
    }

    if (!update.HasRequestedChanges())
    {
        PrintNetworkConfigureUsage();
        return;
    }

    MiaIA::Core::NetworkParameterUpdateSnapshot result;

    if (!MiaIAClient::ApplyNetworkParameterUpdate(update, result))
    {
        std::cout
            << "Network parameter update failed. Check the current "
               "network, values and training/debug state.\n";
        return;
    }

    std::cout
        << "Network parameters updated atomically.\n"
        << "Hidden layers changed: " << result.HiddenLayersChanged
        << "\nOutput layer changed: "
        << (result.OutputLayerChanged ? "yes" : "no")
        << "\nConnection weights changed: "
        << result.ConnectionWeightsChanged
        << "\nNeuron biases changed: "
        << result.NeuronBiasesChanged << "\n";
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

    if (!MiaIAClient::ImportOnnx(ResolvePath(path)))
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

    if (!MiaIAClient::ExportOnnx(ResolvePath(path)))
    {
        std::cout
            << "ONNX export failed.\n";

        return;
    }

    std::cout
        << "ONNX model exported.\n";
}

std::string UnquotePath(const std::string& value);

void PrintProjectInfo(
    const MiaIA::Core::ProjectInfoSnapshot& info)
{
    std::cout
        << "\nMiaIA Project\n\n"
        << "Path: "
        << (info.Path.empty() ? "Unsaved" : info.Path)
        << "\nFormat: .mai v" << (info.FormatVersion == 0
            ? MiaIA::Core::ProjectFormatVersion
            : info.FormatVersion)
        << "\nModel: " << (info.HasModel ? "Available" : "None")
        << "\nBreakpoints: " << info.BreakpointCount;

    if (!info.HasDatasetReference)
    {
        std::cout << "\nDataset: None";
    }
    else
    {
        std::cout
            << "\nDataset: "
            << (info.DatasetLoaded ? "Loaded" : "Unavailable")
            << "\nDataset source: " << info.DatasetSource
            << "\nDataset inputs: " << info.DatasetInputCount
            << "\nDataset targets: " << info.DatasetTargetCount
            << "\nDataset header: "
            << (info.DatasetHasHeader ? "Yes" : "No");
    }

    if (info.Training.Available)
    {
        std::cout
            << "\nTraining epochs: " << info.Training.EpochCount
            << "\nLearning rate: " << info.Training.LearningRate
            << "\nLoss: MSE"
            << "\nOptimizer: SGD";
    }
    else
    {
        std::cout << "\nTraining configuration: None";
    }

    std::cout << "\n";
}

void HandleProjectCommand(const std::string& command)
{
    using MiaIA::SDK::MiaIAClient;

    std::stringstream stream(command);
    std::string projectToken;
    std::string action;
    stream >> projectToken >> action;

    if (projectToken != "project" || action.empty())
    {
        std::cout
            << "Usage: project <new|open|save|info>\n";
        return;
    }

    if (action == "new")
    {
        std::string extra;

        if (stream >> extra || !MiaIAClient::NewProject())
        {
            std::cout
                << "New project failed. Pause training or cancel phase debugging first.\n";
            return;
        }

        std::cout << "New MiaIA project created.\n";
        return;
    }

    if (action == "info")
    {
        std::string extra;

        if (stream >> extra)
        {
            std::cout << "Usage: project info\n";
            return;
        }

        PrintProjectInfo(MiaIAClient::GetProjectInfo());
        return;
    }

    if (action != "open" && action != "save")
    {
        std::cout
            << "Usage: project <new|open|save|info>\n";
        return;
    }

    std::string path;
    std::getline(stream, path);
    path = UnquotePath(path);

    if (path.empty() && action == "save")
    {
        path = MiaIAClient::GetProjectInfo().Path;
    }

    if (path.empty())
    {
        std::cout << "Usage: project " << action << " <path.mai>\n";
        return;
    }

    path = ResolvePath(path);
    const bool succeeded = action == "open"
        ? MiaIAClient::OpenProject(path)
        : MiaIAClient::SaveProject(path);

    if (!succeeded)
    {
        std::cout
            << "Project " << action
            << " failed. Use a valid .mai path and ensure training is not running.\n";
        return;
    }

    const auto info = MiaIAClient::GetProjectInfo();
    std::cout << "MiaIA project "
        << (action == "open" ? "opened" : "saved") << ".\n";

    if (action == "open" &&
        info.HasDatasetReference &&
        !info.DatasetLoaded)
    {
        std::cout
            << "Dataset reference is unavailable; model and project metadata were still restored.\n";
    }
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
            ResolvePath(path),
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
        if (!MiaIAClient::ClearDataset())
        {
            std::cout
                << "Dataset cannot be cleared while training is running. "
                << "Pause the session first.\n";

            return;
        }

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
    case MiaIA::Core::TrainingSessionStatus::Running:
        return "Running";
    case MiaIA::Core::TrainingSessionStatus::Completed:
        return "Completed";
    case MiaIA::Core::TrainingSessionStatus::Cancelled:
        return "Cancelled";
    }

    return "Unknown";
}

const char* TrainingWorkerStopReasonName(
    MiaIA::Core::TrainingWorkerStopReason reason)
{
    switch (reason)
    {
    case MiaIA::Core::TrainingWorkerStopReason::None:
        return "None";
    case MiaIA::Core::TrainingWorkerStopReason::PauseRequested:
        return "Pause requested";
    case MiaIA::Core::TrainingWorkerStopReason::BreakpointHit:
        return "Breakpoint hit";
    case MiaIA::Core::TrainingWorkerStopReason::CancelRequested:
        return "Cancel requested";
    case MiaIA::Core::TrainingWorkerStopReason::StepFailed:
        return "Step failed";
    }

    return "Unknown";
}

void PrintValueComparison(
    const MiaIA::Core::TrainingValueComparisonSnapshot& comparison)
{
    std::cout
        << comparison.FirstValue
        << " -> " << comparison.SecondValue
        << " Delta " << comparison.Delta
        << " |Delta| " << comparison.AbsoluteDelta;
}

double ComparisonRank(double absoluteDelta)
{
    return std::isfinite(absoluteDelta)
        ? absoluteDelta
        : std::numeric_limits<double>::infinity();
}

const char* TrainingDebugPhaseName(
    MiaIA::Core::TrainingDebugPhase phase);

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

    if (session.WorkerStopReason !=
        MiaIA::Core::TrainingWorkerStopReason::None)
    {
        std::cout
            << "\nWorker stop reason: "
            << TrainingWorkerStopReasonName(session.WorkerStopReason);
    }

    if (session.Status == MiaIA::Core::TrainingSessionStatus::Active ||
        session.Status == MiaIA::Core::TrainingSessionStatus::Running)
    {
        std::cout
            << "\nCurrent epoch: " << session.CurrentEpoch + 1
            << "\nNext sample: " << session.NextSampleIndex
            << "\nLearning rate: " << session.LearningRate
            << "\nOptimizer: SGD";
    }

    if (session.HasBreakpointHit)
    {
        const auto& hit = session.LastBreakpointHit;
        std::cout
            << "\nLast breakpoint: #" << hit.BreakpointId
            << " at " << TrainingDebugPhaseName(hit.Phase)
            << ", sample " << hit.SampleIndex;

        if (hit.TargetId != 0)
        {
            std::cout
                << ", target " << hit.TargetId
                << ", observed " << hit.ObservedValue
                << ", threshold " << hit.Threshold;
        }
    }

    std::cout << "\n";
}

void PrintTrainingSessionUsage()
{
    std::cout
        << "Usage: train session start <epochs> <learning-rate> mse\n"
        << "       train session status\n"
        << "       train session debug\n"
        << "       train session history\n"
        << "       train session inspect <step-index>\n"
        << "       train session compare <first-step-index> "
           "<second-step-index> [maximum-items]\n"
        << "       train session next\n"
        << "       train session run <steps|all>\n"
        << "       train session resume\n"
        << "       train session pause\n"
        << "       train session cancel\n";
}

const char* TrainingRunStopReasonName(
    MiaIA::Core::TrainingRunStopReason reason)
{
    switch (reason)
    {
    case MiaIA::Core::TrainingRunStopReason::StepLimitReached:
        return "Step limit reached";
    case MiaIA::Core::TrainingRunStopReason::BreakpointHit:
        return "Breakpoint hit";
    case MiaIA::Core::TrainingRunStopReason::SessionCompleted:
        return "Session completed";
    case MiaIA::Core::TrainingRunStopReason::StepFailed:
        return "Step failed";
    }

    return "Unknown";
}

const char* TrainingDebugPhaseName(
    MiaIA::Core::TrainingDebugPhase phase)
{
    switch (phase)
    {
    case MiaIA::Core::TrainingDebugPhase::Idle:
        return "Idle";
    case MiaIA::Core::TrainingDebugPhase::BeforeForward:
        return "BeforeForward";
    case MiaIA::Core::TrainingDebugPhase::ForwardComplete:
        return "ForwardComplete";
    case MiaIA::Core::TrainingDebugPhase::BackwardComplete:
        return "BackwardComplete";
    case MiaIA::Core::TrainingDebugPhase::UpdateComplete:
        return "UpdateComplete";
    case MiaIA::Core::TrainingDebugPhase::Verified:
        return "Verified";
    case MiaIA::Core::TrainingDebugPhase::Committed:
        return "Committed";
    }

    return "Unknown";
}

const char* TrainingBreakpointKindName(
    MiaIA::Core::TrainingBreakpointKind kind)
{
    switch (kind)
    {
    case MiaIA::Core::TrainingBreakpointKind::Phase:
        return "phase";
    case MiaIA::Core::TrainingBreakpointKind::NeuronActivationAbove:
        return "activation-above";
    case MiaIA::Core::TrainingBreakpointKind::NeuronActivationBelow:
        return "activation-below";
    case MiaIA::Core::TrainingBreakpointKind::NeuronGradientMagnitudeAbove:
        return "gradient-above";
    case MiaIA::Core::TrainingBreakpointKind::ConnectionUpdateMagnitudeAbove:
        return "weight-update-above";
    }

    return "unknown";
}

bool TryParseTrainingDebugPhase(
    const std::string& value,
    MiaIA::Core::TrainingDebugPhase& result)
{
    if (value == "before")
    {
        result = MiaIA::Core::TrainingDebugPhase::BeforeForward;
    }
    else if (value == "forward")
    {
        result = MiaIA::Core::TrainingDebugPhase::ForwardComplete;
    }
    else if (value == "backward")
    {
        result = MiaIA::Core::TrainingDebugPhase::BackwardComplete;
    }
    else if (value == "update")
    {
        result = MiaIA::Core::TrainingDebugPhase::UpdateComplete;
    }
    else if (value == "verify")
    {
        result = MiaIA::Core::TrainingDebugPhase::Verified;
    }
    else if (value == "commit")
    {
        result = MiaIA::Core::TrainingDebugPhase::Committed;
    }
    else
    {
        return false;
    }

    return true;
}

void PrintTrainingBreakpointUsage()
{
    std::cout
        << "Usage: train breakpoint add phase "
            "<before|forward|backward|update|verify|commit>\n"
        << "       train breakpoint add activation-above "
            "<neuron-id> <threshold>\n"
        << "       train breakpoint add activation-below "
            "<neuron-id> <threshold>\n"
        << "       train breakpoint add gradient-above "
            "<neuron-id> <magnitude>\n"
        << "       train breakpoint add weight-update-above "
            "<connection-id> <magnitude>\n"
        << "       train breakpoint list\n"
        << "       train breakpoint enable <id> <on|off>\n"
        << "       train breakpoint remove <id>\n"
        << "       train breakpoint clear\n";
}

void PrintTrainingBreakpoint(
    const MiaIA::Core::TrainingBreakpointSnapshot& breakpoint)
{
    std::cout
        << "#" << breakpoint.Id
        << " [" << (breakpoint.Enabled ? "enabled" : "disabled") << "] "
        << TrainingBreakpointKindName(breakpoint.Spec.Kind);

    if (breakpoint.Spec.Kind ==
        MiaIA::Core::TrainingBreakpointKind::Phase)
    {
        std::cout << " " << TrainingDebugPhaseName(breakpoint.Spec.Phase);
    }
    else
    {
        std::cout
            << " target " << breakpoint.Spec.TargetId
            << " threshold " << breakpoint.Spec.Threshold;
    }

    std::cout << " hits " << breakpoint.HitCount << "\n";
}

void HandleTrainingBreakpointCommand(std::stringstream& stream)
{
    using MiaIA::SDK::MiaIAClient;

    std::string action;

    if (!(stream >> action))
    {
        PrintTrainingBreakpointUsage();
        return;
    }

    if (action == "list")
    {
        stream >> std::ws;

        if (!stream.eof())
        {
            PrintTrainingBreakpointUsage();
            return;
        }

        const auto breakpoints = MiaIAClient::GetTrainingBreakpoints();

        if (breakpoints.empty())
        {
            std::cout << "No training breakpoints configured.\n";
            return;
        }

        std::cout << "\nTraining Breakpoints\n";

        for (const auto& breakpoint : breakpoints)
        {
            PrintTrainingBreakpoint(breakpoint);
        }

        MiaIA::Core::TrainingBreakpointHitSnapshot hit;

        if (MiaIAClient::TryGetLastTrainingBreakpointHit(hit))
        {
            std::cout
                << "Last hit: #" << hit.BreakpointId
                << " at " << TrainingDebugPhaseName(hit.Phase)
                << ", sample " << hit.SampleIndex << "\n";
        }

        return;
    }

    if (action == "clear")
    {
        stream >> std::ws;

        if (!stream.eof() || !MiaIAClient::ClearTrainingBreakpoints())
        {
            std::cout
                << "Training breakpoints could not be cleared while "
                   "training or debug is active.\n";
            return;
        }

        std::cout << "Training breakpoints cleared.\n";
        return;
    }

    if (action == "enable")
    {
        std::uint64_t breakpointId{};
        std::string enabledName;

        if (!(stream >> breakpointId >> enabledName) ||
            (enabledName != "on" && enabledName != "off"))
        {
            PrintTrainingBreakpointUsage();
            return;
        }

        stream >> std::ws;

        if (!stream.eof() ||
            !MiaIAClient::SetTrainingBreakpointEnabled(
                breakpointId,
                enabledName == "on"))
        {
            std::cout << "Training breakpoint could not be updated.\n";
            return;
        }

        std::cout << "Training breakpoint updated.\n";
        return;
    }

    if (action == "remove")
    {
        std::uint64_t breakpointId{};

        if (!(stream >> breakpointId))
        {
            PrintTrainingBreakpointUsage();
            return;
        }

        stream >> std::ws;

        if (!stream.eof() ||
            !MiaIAClient::RemoveTrainingBreakpoint(breakpointId))
        {
            std::cout << "Training breakpoint could not be removed.\n";
            return;
        }

        std::cout << "Training breakpoint removed.\n";
        return;
    }

    if (action != "add")
    {
        PrintTrainingBreakpointUsage();
        return;
    }

    std::string condition;
    MiaIA::Core::TrainingBreakpointSpec spec;

    if (!(stream >> condition))
    {
        PrintTrainingBreakpointUsage();
        return;
    }

    if (condition == "phase")
    {
        std::string phaseName;

        if (!(stream >> phaseName) ||
            !TryParseTrainingDebugPhase(phaseName, spec.Phase))
        {
            PrintTrainingBreakpointUsage();
            return;
        }

        spec.Kind = MiaIA::Core::TrainingBreakpointKind::Phase;
    }
    else
    {
        if (!(stream >> spec.TargetId >> spec.Threshold))
        {
            PrintTrainingBreakpointUsage();
            return;
        }

        if (condition == "activation-above")
        {
            spec.Kind = MiaIA::Core::TrainingBreakpointKind::
                NeuronActivationAbove;
        }
        else if (condition == "activation-below")
        {
            spec.Kind = MiaIA::Core::TrainingBreakpointKind::
                NeuronActivationBelow;
        }
        else if (condition == "gradient-above")
        {
            spec.Kind = MiaIA::Core::TrainingBreakpointKind::
                NeuronGradientMagnitudeAbove;
        }
        else if (condition == "weight-update-above")
        {
            spec.Kind = MiaIA::Core::TrainingBreakpointKind::
                ConnectionUpdateMagnitudeAbove;
        }
        else
        {
            PrintTrainingBreakpointUsage();
            return;
        }
    }

    stream >> std::ws;

    if (!stream.eof())
    {
        PrintTrainingBreakpointUsage();
        return;
    }

    MiaIA::Core::TrainingBreakpointSnapshot breakpoint;

    if (!MiaIAClient::AddTrainingBreakpoint(spec, breakpoint))
    {
        std::cout
            << "Training breakpoint could not be added. Check its "
               "arguments or active training state.\n";
        return;
    }

    std::cout << "Training breakpoint added: ";
    PrintTrainingBreakpoint(breakpoint);
}

void PrintTrainingDebug(
    const MiaIA::Core::TrainingDebugSnapshot& debug)
{
    std::cout
        << "\nTraining Debug"
        << "\nPhase: " << TrainingDebugPhaseName(debug.Phase);

    if (debug.Phase == MiaIA::Core::TrainingDebugPhase::Idle)
    {
        std::cout << "\n";
        return;
    }

    std::cout
        << "\nSample: " << debug.SampleIndex
        << "\nLearning rate: " << debug.LearningRate
        << "\nCandidate layers: "
        << debug.CandidateNetwork.Layers.size()
        << "\nCandidate connections: "
        << debug.CandidateNetwork.Connections.size();

    if (debug.Phase >=
        MiaIA::Core::TrainingDebugPhase::ForwardComplete)
    {
        std::cout
            << "\nLoss before: " << debug.Step.Before.Evaluation.Loss
            << "\nTargets: ";
        PrintValues(debug.Step.Before.Evaluation.Targets);
        std::cout << "\nPredictions: ";
        PrintValues(debug.Step.Before.Evaluation.Predictions);
        std::cout << "\nErrors: ";
        PrintValues(debug.Step.Before.Evaluation.Errors);
    }

    if (debug.Phase >=
        MiaIA::Core::TrainingDebugPhase::BackwardComplete)
    {
        std::cout << "\n\nNeuron Gradients\n";

        for (const auto& neuron : debug.Step.Before.Neurons)
        {
            std::cout
                << "Neuron " << neuron.Id
                << " dLoss/dActivation "
                << neuron.ActivationGradient
                << " dLoss/dPreActivation "
                << neuron.PreActivationGradient
                << " dLoss/dBias " << neuron.BiasGradient
                << "\n";
        }

        std::cout << "\nConnection Gradients\n";

        for (const auto& connection : debug.Step.Before.Connections)
        {
            std::cout
                << "Connection " << connection.Id
                << " " << connection.FromNeuron
                << " -> " << connection.ToNeuron
                << " dLoss/dWeight "
                << connection.WeightGradient << "\n";
        }
    }

    if (debug.Phase >=
        MiaIA::Core::TrainingDebugPhase::UpdateComplete)
    {
        std::cout << "\nConnection Updates\n";

        for (const auto& update : debug.Step.ConnectionUpdates)
        {
            std::cout
                << "Connection " << update.Id
                << " Weight " << update.PreviousWeight
                << " Delta " << update.Delta
                << " Updated " << update.UpdatedWeight
                << "\n";
        }

        std::cout << "\nBias Updates\n";

        for (const auto& update : debug.Step.NeuronUpdates)
        {
            std::cout
                << "Neuron " << update.Id
                << " Bias " << update.PreviousBias
                << " Delta " << update.Delta
                << " Updated " << update.UpdatedBias
                << "\n";
        }
    }

    if (debug.Phase >= MiaIA::Core::TrainingDebugPhase::Verified)
    {
        std::cout
            << "\nLoss after: " << debug.Step.After.Loss
            << "\nPredictions after: ";
        PrintValues(debug.Step.After.Predictions);
    }

    std::cout << "\n";
}

void PrintTrainingDebugUsage()
{
    std::cout
        << "Usage: train debug start <sample-index> <learning-rate> mse\n"
        << "       train debug status\n"
        << "       train debug next\n"
        << "       train debug neuron <neuron-id>\n"
        << "       train debug connection <connection-id>\n"
        << "       train debug cancel\n";
}

void PrintTrainingDebugNeuron(
    const MiaIA::Core::TrainingDebugNeuronSnapshot& neuron)
{
    std::cout
        << "\nTraining Debug Neuron " << neuron.Id
        << "\nPhase: " << TrainingDebugPhaseName(neuron.Phase)
        << "\nLayer order: " << neuron.LayerOrder
        << "\nPublic activation: " << neuron.PublicActivation
        << "\nCandidate activation: " << neuron.CandidateActivation
        << "\nPublic bias: " << neuron.PublicBias
        << "\nCandidate bias: " << neuron.CandidateBias;

    if (neuron.HasGradients)
    {
        std::cout
            << "\ndLoss/dActivation: "
            << neuron.ActivationGradient
            << "\ndLoss/dPreActivation: "
            << neuron.PreActivationGradient
            << "\ndLoss/dBias: " << neuron.BiasGradient;
    }
    else
    {
        std::cout << "\nGradients: not available in this phase";
    }

    if (neuron.HasUpdate)
    {
        std::cout
            << "\nPrevious bias: " << neuron.PreviousBias
            << "\nUpdate gradient: " << neuron.UpdateGradient
            << "\nBias delta: " << neuron.Delta
            << "\nUpdated bias: " << neuron.UpdatedBias;
    }
    else if (neuron.LayerOrder == 0)
    {
        std::cout << "\nUpdate: input neuron biases are not trainable";
    }
    else
    {
        std::cout << "\nUpdate: not available in this phase";
    }

    std::cout << "\n";
}

void PrintTrainingDebugConnection(
    const MiaIA::Core::TrainingDebugConnectionSnapshot& connection)
{
    std::cout
        << "\nTraining Debug Connection " << connection.Id
        << "\nPhase: " << TrainingDebugPhaseName(connection.Phase)
        << "\nFrom: " << connection.FromNeuron
        << "\nTo: " << connection.ToNeuron
        << "\nPublic weight: " << connection.PublicWeight
        << "\nCandidate weight: " << connection.CandidateWeight;

    if (connection.HasGradient)
    {
        std::cout
            << "\ndLoss/dWeight: " << connection.WeightGradient;
    }
    else
    {
        std::cout << "\nGradient: not available in this phase";
    }

    if (connection.HasUpdate)
    {
        std::cout
            << "\nPrevious weight: " << connection.PreviousWeight
            << "\nUpdate gradient: " << connection.UpdateGradient
            << "\nWeight delta: " << connection.Delta
            << "\nUpdated weight: " << connection.UpdatedWeight;
    }
    else
    {
        std::cout << "\nUpdate: not available in this phase";
    }

    std::cout << "\n";
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

        if (sessionAction == "run")
        {
            std::string limit;

            if (!(stream >> limit))
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

            const auto session = MiaIAClient::GetTrainingSession();
            std::size_t maximumSteps{};

            if (limit == "all")
            {
                if (session.CompletedSteps > session.TotalSteps)
                {
                    std::cout << "Training session state is invalid.\n";
                    return;
                }

                maximumSteps =
                    session.TotalSteps - session.CompletedSteps;
            }
            else
            {
                if (limit.empty() || limit.front() == '-')
                {
                    PrintTrainingSessionUsage();
                    return;
                }

                std::stringstream limitStream(limit);

                if (!(limitStream >> maximumSteps))
                {
                    PrintTrainingSessionUsage();
                    return;
                }

                limitStream >> std::ws;

                if (!limitStream.eof())
                {
                    PrintTrainingSessionUsage();
                    return;
                }
            }

            MiaIA::Core::TrainingRunSnapshot run;

            if (!MiaIAClient::RunTrainingSession(
                maximumSteps,
                run))
            {
                std::cout
                    << "Training session run could not start. "
                    << "Check the session state and step limit.\n";

                return;
            }

            std::cout
                << "\nTraining Session Run"
                << "\nRequested steps: " << run.RequestedSteps
                << "\nExecuted steps: " << run.ExecutedSteps
                << "\nStop reason: "
                << TrainingRunStopReasonName(run.StopReason)
                << "\nStart: epoch " << run.StartEpoch + 1
                << ", sample " << run.StartSampleIndex;

            if (run.StopReason ==
                MiaIA::Core::TrainingRunStopReason::SessionCompleted)
            {
                std::cout
                    << "\nEnd: completed " << run.EndEpoch
                    << " epoch(s)";
            }
            else
            {
                std::cout
                    << "\nEnd: epoch " << run.EndEpoch + 1
                    << ", sample " << run.EndSampleIndex;
            }

            if (run.ExecutedSteps > 0)
            {
                std::cout
                    << "\nMean loss before update: "
                    << run.MeanLossBeforeUpdate
                    << "\nMean loss after update: "
                    << run.MeanLossAfterUpdate;
            }

            std::cout << "\n";
            PrintTrainingSession(MiaIAClient::GetTrainingSession());
            return;
        }

        if (sessionAction == "compare")
        {
            std::size_t firstStepIndex{};
            std::size_t secondStepIndex{};
            std::size_t maximumItems{ 10 };

            if (!(stream >> firstStepIndex >> secondStepIndex))
            {
                PrintTrainingSessionUsage();
                return;
            }

            stream >> std::ws;

            if (!stream.eof())
            {
                if (!(stream >> maximumItems) || maximumItems == 0)
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
            }

            MiaIA::Core::TrainingStepComparisonSnapshot comparison;

            if (!MiaIAClient::TryCompareTrainingSessionSteps(
                firstStepIndex,
                secondStepIndex,
                comparison))
            {
                std::cout
                    << "Training session steps could not be compared. "
                       "Check both history indexes.\n";
                return;
            }

            std::cout
                << "\nTraining Session Comparison "
                << comparison.FirstStepIndex
                << " -> " << comparison.SecondStepIndex
                << "\nSamples: " << comparison.FirstSampleIndex
                << " -> " << comparison.SecondSampleIndex;

            if (comparison.SameSample)
            {
                std::cout << " (same sample)";
            }
            else
            {
                std::cout
                    << " (different samples; loss and prediction "
                       "deltas are contextual)";
            }

            std::cout << "\nLoss before: ";
            PrintValueComparison(comparison.LossBefore);
            std::cout << "\nLoss after: ";
            PrintValueComparison(comparison.LossAfter);
            std::cout << "\n\nOutput Predictions\n";

            for (const auto& output : comparison.Outputs)
            {
                std::cout << "Output " << output.OutputIndex;

                if (!output.HasFirstPrediction ||
                    !output.HasSecondPrediction)
                {
                    std::cout << " unavailable in one step\n";
                    continue;
                }

                std::cout << " Before ";
                PrintValueComparison(output.BeforePrediction);
                std::cout << " After ";
                PrintValueComparison(output.AfterPrediction);
                std::cout << "\n";
            }

            auto neuronGradients = comparison.Neurons;
            neuronGradients.erase(
                std::remove_if(
                    neuronGradients.begin(),
                    neuronGradients.end(),
                    [](const auto& neuron)
                    {
                        return !neuron.HasFirstGradient ||
                            !neuron.HasSecondGradient;
                    }),
                neuronGradients.end());
            std::sort(
                neuronGradients.begin(),
                neuronGradients.end(),
                [](const auto& left, const auto& right)
                {
                    const double leftChange = std::max({
                        ComparisonRank(
                            left.ActivationGradient.AbsoluteDelta),
                        ComparisonRank(
                            left.PreActivationGradient.AbsoluteDelta),
                        ComparisonRank(
                            left.BiasGradient.AbsoluteDelta) });
                    const double rightChange = std::max({
                        ComparisonRank(
                            right.ActivationGradient.AbsoluteDelta),
                        ComparisonRank(
                            right.PreActivationGradient.AbsoluteDelta),
                        ComparisonRank(
                            right.BiasGradient.AbsoluteDelta) });
                    return leftChange != rightChange
                        ? leftChange > rightChange
                        : left.Id < right.Id;
                });

            if (neuronGradients.size() > maximumItems)
            {
                neuronGradients.resize(maximumItems);
            }

            std::cout << "\nTop Neuron Gradient Changes\n";

            for (const auto& neuron : neuronGradients)
            {
                std::cout << "Neuron " << neuron.Id << " Activation ";
                PrintValueComparison(neuron.ActivationGradient);
                std::cout << " Pre-activation ";
                PrintValueComparison(neuron.PreActivationGradient);
                std::cout << " Bias ";
                PrintValueComparison(neuron.BiasGradient);
                std::cout << "\n";
            }

            auto connectionGradients = comparison.Connections;
            connectionGradients.erase(
                std::remove_if(
                    connectionGradients.begin(),
                    connectionGradients.end(),
                    [](const auto& connection)
                    {
                        return !connection.HasFirstGradient ||
                            !connection.HasSecondGradient;
                    }),
                connectionGradients.end());
            std::sort(
                connectionGradients.begin(),
                connectionGradients.end(),
                [](const auto& left, const auto& right)
                {
                    const double leftChange = ComparisonRank(
                        left.WeightGradient.AbsoluteDelta);
                    const double rightChange = ComparisonRank(
                        right.WeightGradient.AbsoluteDelta);
                    return leftChange != rightChange
                        ? leftChange > rightChange
                        : left.Id < right.Id;
                });

            if (connectionGradients.size() > maximumItems)
            {
                connectionGradients.resize(maximumItems);
            }

            std::cout << "\nTop Connection Gradient Changes\n";

            for (const auto& connection : connectionGradients)
            {
                std::cout
                    << "Connection " << connection.Id
                    << " (" << connection.FromNeuron
                    << " -> " << connection.ToNeuron << ") ";
                PrintValueComparison(connection.WeightGradient);
                std::cout << "\n";
            }

            auto weightChanges = comparison.Connections;
            weightChanges.erase(
                std::remove_if(
                    weightChanges.begin(),
                    weightChanges.end(),
                    [](const auto& connection)
                    {
                        return !connection.HasFirstUpdate ||
                            !connection.HasSecondUpdate;
                    }),
                weightChanges.end());
            std::sort(
                weightChanges.begin(),
                weightChanges.end(),
                [](const auto& left, const auto& right)
                {
                    const double leftChange = ComparisonRank(
                        left.Weight.AbsoluteDelta);
                    const double rightChange = ComparisonRank(
                        right.Weight.AbsoluteDelta);
                    return leftChange != rightChange
                        ? leftChange > rightChange
                        : left.Id < right.Id;
                });

            if (weightChanges.size() > maximumItems)
            {
                weightChanges.resize(maximumItems);
            }

            std::cout << "\nTop Weight Changes\n";

            for (const auto& connection : weightChanges)
            {
                std::cout << "Connection " << connection.Id << " ";
                PrintValueComparison(connection.Weight);
                std::cout << "\n";
            }

            auto biasChanges = comparison.Neurons;
            biasChanges.erase(
                std::remove_if(
                    biasChanges.begin(),
                    biasChanges.end(),
                    [](const auto& neuron)
                    {
                        return !neuron.HasFirstUpdate ||
                            !neuron.HasSecondUpdate;
                    }),
                biasChanges.end());
            std::sort(
                biasChanges.begin(),
                biasChanges.end(),
                [](const auto& left, const auto& right)
                {
                    const double leftChange = ComparisonRank(
                        left.Bias.AbsoluteDelta);
                    const double rightChange = ComparisonRank(
                        right.Bias.AbsoluteDelta);
                    return leftChange != rightChange
                        ? leftChange > rightChange
                        : left.Id < right.Id;
                });

            if (biasChanges.size() > maximumItems)
            {
                biasChanges.resize(maximumItems);
            }

            std::cout << "\nTop Bias Changes\n";

            for (const auto& neuron : biasChanges)
            {
                std::cout << "Neuron " << neuron.Id << " ";
                PrintValueComparison(neuron.Bias);
                std::cout << "\n";
            }

            std::cout << "\n";
            return;
        }

        if (sessionAction == "inspect")
        {
            std::size_t stepIndex{};

            if (!(stream >> stepIndex))
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

            MiaIA::Core::TrainingStepSnapshot step;

            if (!MiaIAClient::TryGetTrainingSessionStep(
                stepIndex,
                step))
            {
                std::cout << "Training session step was not found.\n";
                return;
            }

            std::cout
                << "\nTraining Session Step " << stepIndex
                << "\nSample: " << step.SampleIndex
                << "\nLoss before: " << step.Before.Evaluation.Loss
                << "\nLoss after: " << step.After.Loss
                << "\nTargets: ";
            PrintValues(step.Before.Evaluation.Targets);
            std::cout << "\nPredictions before: ";
            PrintValues(step.Before.Evaluation.Predictions);
            std::cout << "\nErrors: ";
            PrintValues(step.Before.Evaluation.Errors);
            std::cout << "\nPredictions after: ";
            PrintValues(step.After.Predictions);
            std::cout << "\n\nNeuron Gradients\n";

            for (const auto& neuron : step.Before.Neurons)
            {
                std::cout
                    << "Neuron " << neuron.Id
                    << " dLoss/dActivation "
                    << neuron.ActivationGradient
                    << " dLoss/dPreActivation "
                    << neuron.PreActivationGradient
                    << " dLoss/dBias " << neuron.BiasGradient
                    << "\n";
            }

            std::cout << "\nConnection Updates\n";

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

            std::cout << "\n";
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

        if (sessionAction == "debug")
        {
            MiaIA::Core::TrainingDebugSnapshot debug;

            if (!MiaIAClient::StartTrainingSessionDebug(debug))
            {
                std::cout
                    << "Training session debug could not start. "
                    << "The session must be Active at a coherent "
                    << "sample boundary.\n";
                return;
            }

            PrintTrainingDebug(debug);
            return;
        }

        if (sessionAction == "history")
        {
            const auto history =
                MiaIAClient::GetTrainingSessionHistory();

            if (history.empty())
            {
                std::cout << "Training session history is empty.\n";
                return;
            }

            std::cout << "\nTraining Session History\n";

            for (const auto& entry : history)
            {
                std::cout
                    << "Step " << entry.StepIndex
                    << " Epoch " << entry.EpochIndex + 1
                    << " Sample " << entry.SampleIndex
                    << " Loss " << entry.LossBefore
                    << " -> " << entry.LossAfter
                    << " Weight updates " << entry.WeightUpdateCount
                    << " Bias updates " << entry.BiasUpdateCount
                    << "\n";
            }

            std::cout << "\n";
            return;
        }

        if (sessionAction == "resume")
        {
            if (!MiaIAClient::ResumeTrainingSession())
            {
                std::cout
                    << "Training session could not resume. "
                    << "It must be Active at a safe step boundary.\n";
                return;
            }

            PrintTrainingSession(MiaIAClient::GetTrainingSession());
            return;
        }

        if (sessionAction == "pause")
        {
            if (!MiaIAClient::PauseTrainingSession())
            {
                std::cout
                    << "No running training worker to pause.\n";
                return;
            }

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

    if (action == "breakpoint")
    {
        HandleTrainingBreakpointCommand(stream);
        return;
    }

    if (action == "debug")
    {
        std::string debugAction;

        if (!(stream >> debugAction))
        {
            PrintTrainingDebugUsage();
            return;
        }

        if (debugAction == "start")
        {
            std::size_t sampleIndex{};
            double learningRate{};
            std::string lossName;

            if (!(stream >> sampleIndex >> learningRate >> lossName) ||
                lossName != "mse")
            {
                PrintTrainingDebugUsage();
                return;
            }

            stream >> std::ws;

            if (!stream.eof())
            {
                PrintTrainingDebugUsage();
                return;
            }

            MiaIA::Core::TrainingDebugSnapshot debug;

            if (!MiaIAClient::StartTrainingDebug(
                sampleIndex,
                learningRate,
                MiaIA::Core::LossType::MeanSquaredError,
                MiaIA::Core::OptimizerType::StochasticGradientDescent,
                debug))
            {
                std::cout
                    << "Training debug could not be started. "
                    << "Check the sample, network, dataset, or active "
                    << "training state.\n";
                return;
            }

            PrintTrainingDebug(debug);
            return;
        }

        if (debugAction == "neuron")
        {
            std::uint64_t neuronId{};

            if (!(stream >> neuronId))
            {
                PrintTrainingDebugUsage();
                return;
            }

            stream >> std::ws;

            if (!stream.eof())
            {
                PrintTrainingDebugUsage();
                return;
            }

            MiaIA::Core::TrainingDebugNeuronSnapshot neuron;

            if (!MiaIAClient::TryGetTrainingDebugNeuron(
                neuronId,
                neuron))
            {
                std::cout
                    << "Training debug neuron was not found or no "
                    << "debug transaction is available.\n";
                return;
            }

            PrintTrainingDebugNeuron(neuron);
            return;
        }

        if (debugAction == "connection")
        {
            std::uint64_t connectionId{};

            if (!(stream >> connectionId))
            {
                PrintTrainingDebugUsage();
                return;
            }

            stream >> std::ws;

            if (!stream.eof())
            {
                PrintTrainingDebugUsage();
                return;
            }

            MiaIA::Core::TrainingDebugConnectionSnapshot connection;

            if (!MiaIAClient::TryGetTrainingDebugConnection(
                connectionId,
                connection))
            {
                std::cout
                    << "Training debug connection was not found or no "
                    << "debug transaction is available.\n";
                return;
            }

            PrintTrainingDebugConnection(connection);
            return;
        }

        stream >> std::ws;

        if (!stream.eof())
        {
            PrintTrainingDebugUsage();
            return;
        }

        if (debugAction == "status")
        {
            PrintTrainingDebug(MiaIAClient::GetTrainingDebug());
            return;
        }

        if (debugAction == "next")
        {
            MiaIA::Core::TrainingDebugSnapshot debug;

            if (!MiaIAClient::AdvanceTrainingDebug(debug))
            {
                std::cout
                    << "Training debug could not advance. "
                    << "The current phase was preserved.\n";
                return;
            }

            PrintTrainingDebug(debug);
            return;
        }

        if (debugAction == "cancel")
        {
            if (!MiaIAClient::CancelTrainingDebug())
            {
                std::cout << "No active training debug to cancel.\n";
                return;
            }

            std::cout
                << "Training debug cancelled. "
                << "The candidate network was discarded.\n";
            return;
        }

        PrintTrainingDebugUsage();
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

const char* ActivationTypeName(MiaIA::Core::ActivationType activation)
{
    switch (activation)
    {
    case MiaIA::Core::ActivationType::Sigmoid:
        return "Sigmoid";
    case MiaIA::Core::ActivationType::ReLU:
        return "ReLU";
    case MiaIA::Core::ActivationType::Tanh:
        return "Tanh";
    case MiaIA::Core::ActivationType::Linear:
        return "Linear";
    }

    return "Unknown";
}

void PrintNeuronContext(
    const MiaIA::Core::NeuronContextSnapshot& context)
{
    std::cout
        << "Neuron " << context.Neuron.Id
        << "\n  Layer: [" << context.LayerOrder << "] "
        << context.LayerName
        << " (" << ActivationTypeName(context.LayerActivation) << ")"
        << "\n  Activation: " << context.Neuron.Activation
        << "\n  Bias: " << context.Neuron.Bias
        << "\n";
}

void PrintRelatedConnections(
    const char* title,
    const std::vector<MiaIA::Core::ConnectionSnapshot>& connections,
    std::size_t totalCount)
{
    std::cout
        << "\n" << title << ": "
        << connections.size() << " shown of " << totalCount << "\n";

    for (const auto& connection : connections)
    {
        std::cout
            << "  Connection " << connection.Id
            << ": " << connection.FromNeuron
            << " -> " << connection.ToNeuron
            << " Weight " << connection.Weight << "\n";
    }

    if (connections.size() < totalCount)
    {
        std::cout
            << "  ... " << totalCount - connections.size()
            << " more\n";
    }
}

void InspectElement(const std::string& command)
{
    using MiaIA::SDK::MiaIAClient;

    std::stringstream stream(command);
    std::string inspectToken;
    std::string elementType;
    stream >> inspectToken >> elementType;

    if (elementType == "neuron")
    {
        std::uint64_t neuronId{};
        std::size_t maximumConnections{ 10 };

        if (!(stream >> neuronId))
        {
            std::cout
                << "Usage: inspect neuron <neuron-id> "
                   "[maximum-connections]\n";
            return;
        }

        stream >> std::ws;

        if (!stream.eof())
        {
            std::int64_t requestedMaximum{};

            if (!(stream >> requestedMaximum) || requestedMaximum <= 0)
            {
                std::cout
                    << "Usage: inspect neuron <neuron-id> "
                       "[maximum-connections]\n";
                return;
            }

            maximumConnections = static_cast<std::size_t>(
                requestedMaximum);

            stream >> std::ws;

            if (!stream.eof())
            {
                std::cout
                    << "Usage: inspect neuron <neuron-id> "
                       "[maximum-connections]\n";
                return;
            }
        }

        MiaIA::Core::NeuronInspectionSnapshot inspection;

        if (!MiaIAClient::TryInspectNeuron(
            neuronId,
            maximumConnections,
            inspection))
        {
            std::cout << "Neuron inspection failed. Check the neuron ID.\n";
            return;
        }

        std::cout << "\nNeuron Inspection\n\n";
        PrintNeuronContext(inspection.Context);
        PrintRelatedConnections(
            "Incoming connections",
            inspection.IncomingConnections,
            inspection.IncomingConnectionCount);
        PrintRelatedConnections(
            "Outgoing connections",
            inspection.OutgoingConnections,
            inspection.OutgoingConnectionCount);
        std::cout << "\n";
        return;
    }

    if (elementType == "connection")
    {
        std::uint64_t connectionId{};
        std::string extra;

        if (!(stream >> connectionId) || stream >> extra)
        {
            std::cout
                << "Usage: inspect connection <connection-id>\n";
            return;
        }

        MiaIA::Core::ConnectionInspectionSnapshot inspection;

        if (!MiaIAClient::TryInspectConnection(
            connectionId,
            inspection))
        {
            std::cout
                << "Connection inspection failed. "
                   "Check the connection ID.\n";
            return;
        }

        std::cout
            << "\nConnection Inspection\n\n"
            << "Connection " << inspection.Connection.Id
            << ": " << inspection.Connection.FromNeuron
            << " -> " << inspection.Connection.ToNeuron
            << "\nWeight: " << inspection.Connection.Weight
            << "\n\nFrom Endpoint\n";
        PrintNeuronContext(inspection.FromNeuron);
        std::cout << "\nTo Endpoint\n";
        PrintNeuronContext(inspection.ToNeuron);
        std::cout << "\n";
        return;
    }

    std::cout
        << "Usage: inspect [neuron <neuron-id> [maximum-connections]"
           "|connection <connection-id>]\n";
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
        "project",
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

}

std::vector<MiaIA::CLI::CommandSuggestion>
MiaIA::CLI::MiaIACommandProcessor::GetSuggestions(
    const std::string& input,
    std::size_t maximumResults)
{
    std::vector<CommandSuggestion> suggestions;

    if (maximumResults == 0)
    {
        return suggestions;
    }

    const std::string trimmed = Trim(input);
    const bool endsWithWhitespace =
        !input.empty() &&
        std::isspace(static_cast<unsigned char>(input.back())) != 0;
    const auto inputTokens = CommandTokens(trimmed);
    const std::size_t desiredDepth = inputTokens.empty()
        ? 1
        : inputTokens.size() + (endsWithWhitespace ? 1 : 0);

    for (const auto& entry : CommandCatalog())
    {
        const auto entryTokens = CommandTokens(entry.Completion);

        if (entryTokens.size() != desiredDepth)
        {
            continue;
        }

        bool matches = true;

        for (std::size_t index = 0; index < desiredDepth; ++index)
        {
            if (index >= inputTokens.size())
            {
                break;
            }

            const bool currentPartialToken =
                !endsWithWhitespace && index + 1 == inputTokens.size();

            if (currentPartialToken)
            {
                matches = StartsWith(
                    entryTokens[index],
                    inputTokens[index]);
            }
            else
            {
                matches = entryTokens[index] == inputTokens[index];
            }

            if (!matches)
            {
                break;
            }
        }

        if (!matches)
        {
            continue;
        }

        suggestions.push_back(
        {
            entry.Completion,
            entry.Syntax,
            entry.Description
        });

        if (suggestions.size() == maximumResults)
        {
            return suggestions;
        }
    }

    if (!suggestions.empty() || trimmed.empty())
    {
        return suggestions;
    }

    const CommandCatalogEntry* argumentEntry{};

    for (const auto& entry : CommandCatalog())
    {
        if (!entry.AcceptsArguments)
        {
            continue;
        }

        const std::string completion(entry.Completion);

        if (trimmed == completion ||
            StartsWith(trimmed, completion + " "))
        {
            if (!argumentEntry ||
                completion.size() >
                    std::string(argumentEntry->Completion).size())
            {
                argumentEntry = &entry;
            }
        }
    }

    if (argumentEntry)
    {
        suggestions.push_back(
        {
            trimmed,
            argumentEntry->Syntax,
            argumentEntry->Description
        });
    }

    return suggestions;
}

std::string MiaIA::CLI::MiaIACommandProcessor::Banner()
{
    return "MiaIA Console v0.1\n";
}

MiaIA::CLI::CommandResult
MiaIA::CLI::MiaIACommandProcessor::Execute(
    const std::string& input,
    const std::string& workingDirectory)
{
    const std::scoped_lock lock(CommandExecutionMutex);
    std::ostringstream output;
    ScopedCommandOutput scopedOutput(output);
    ScopedWorkingDirectory scopedDirectory(workingDirectory);
    CommandResult result;
    std::string command = Trim(input);

    if (command.empty())
    {
        return result;
    }

    const std::string resolved = ResolveCommand(command);

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
    }
    else if (command.rfind("network", 0) == 0)
    {
        HandleNetworkCommand(command);
    }
    else if (command.rfind("input", 0) == 0)
    {
        SetInput(command);
    }
    else if (command.rfind("predict", 0) == 0)
    {
        Predict(command);
    }
    else if (command.rfind("import", 0) == 0)
    {
        ImportOnnx(command);
    }
    else if (command.rfind("export", 0) == 0)
    {
        ExportOnnx(command);
    }
    else if (command.rfind("project", 0) == 0)
    {
        HandleProjectCommand(command);
    }
    else if (command.rfind("dataset", 0) == 0)
    {
        HandleDatasetCommand(command);
    }
    else if (command.rfind("train", 0) == 0)
    {
        HandleTrainCommand(command);
    }
    else if (command == "summary")
    {
        PrintSummary();
    }
    else if (command == "inspect")
    {
        InspectNetwork();
    }
    else if (command.rfind("inspect ", 0) == 0)
    {
        InspectElement(command);
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
        result.ExitRequested = true;
    }
    else
    {
        std::cout << "Unknown command\n";
    }

    result.Output = output.str();
    return result;
}
