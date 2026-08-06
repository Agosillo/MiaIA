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

        << "  create [input hidden layers output]\n"
        << "      Create a dense neural network\n\n"
        << "      Example:\n"
        << "        create 784 256 3 10\n\n"

        << "  input [values]\n"
        << "      Set the input layer activation values\n\n"
        << "      Example:\n"
        << "        input 0.5 0.2 0.9\n\n"

        << "  import onnx [path]\n"
        << "      Replace the current network with an ONNX model\n\n"

        << "  export onnx [path]\n"
        << "      Export the current network as an ONNX model\n\n"

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
        "import",
        "export",
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
