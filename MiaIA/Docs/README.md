# MiaIA Documentation

MiaIA is an Interactive Development Environment for Artificial Intelligence. Its purpose is not to compete with large training frameworks on throughput. Its purpose is to make a neural network observable: users should be able to build a model, execute it, inspect its state, evaluate samples, follow gradients, and eventually control training step by step.

The project is currently in its foundation stage. The C++ engine, public SDK facade, interactive console, test harness, ONNX interchange, CSV dataset pipeline, sample evaluation, and non-mutating gradient inspection are implemented. Controlled parameter updates, training sessions, persistent MiaIA project files, and the complete Unreal visualization experience are planned work.

## Documentation map

- [Architecture](Architecture/Architecture.md) describes module boundaries, supported data flows, and current technical constraints.
- [Console guide](Console/Console.md) documents every interactive command and introduces the neural-network concepts behind them.
- [Coding guidelines](Coding/Coding.md) records the conventions used by the C++ codebase.
- [Architectural decisions](ADR/ADR.md) summarizes the decisions that currently shape the project.
- [History](History/History.md) tracks the implemented foundation in chronological order.
- [Roadmap](Roadmap/Roadmap.md) separates completed work from planned capabilities.
- [Project Genesis](Architecture/000_ProjectGenesis.md) is the original historical statement of the project and is intentionally preserved unchanged.

## Current capability summary

| Area | Current capability |
| --- | --- |
| Network model | Ordered feed-forward layers, neurons, biases, weighted connections, and editable activations |
| Activations | Sigmoid, ReLU, Tanh, and Linear |
| Execution | Input assignment and forward propagation |
| Inspection | Network, layer, neuron, connection, dataset, evaluation, and gradient snapshots |
| Interchange | Import and export of the currently supported dense ONNX subset |
| Datasets | Numeric CSV import with explicit input and target column counts |
| Evaluation | Predictions, target errors, and mean squared error for a selected sample |
| Differentiation | Per-neuron, per-bias, and per-connection gradients without parameter updates |
| Clients | Interactive console and an initial Unreal Engine integration through `MiaIAClient` |
| Verification | Named Debug- and Release-capable C++ test harness, including numerical gradient checks |

## Solution structure

```text
MiaIA/
|-- Core/       Stable data structures, activation primitives, and public snapshots
|-- Engine/     Validation, editing, execution, interchange, evaluation, and differentiation
|-- SDK/        Client-facing MiaIAClient facade and process-local state
|-- Console/    Interactive command-line client
|-- Tests/      Named integration and mathematical correctness tests
|-- IDE/        Initial Unreal Engine client integration
`-- Docs/       Project documentation
```

## Build requirements

- Visual Studio 2022 with the Desktop development with C++ workload;
- MSVC v143;
- C++20;
- a Windows SDK available to Visual Studio;
- vcpkg manifest integration;
- ONNX and Protobuf dependencies declared in `vcpkg.json`.

The primary development configuration is x64. The solution also contains x86 configurations, but every target configuration should be verified before it is treated as a supported release target.

## Build and run

1. Open `MiaIA.sln` in Visual Studio.
2. Select `Debug | x64` or `Release | x64`.
3. Build the solution. The first dependency restore may take several minutes.
4. Run the `Tests` project and confirm that every named group reports `[PASS]`.
5. Set `Console` as the startup project to experiment interactively.

For a complete console session, see the [Console guide](Console/Console.md).

## Architectural direction

All clients use the SDK rather than reaching into Engine or Core internals. The console is therefore not a separate implementation: it exercises the same public facade intended for Unreal Engine, Blueprint wrappers, a future Unity client, or another editor.

ONNX is used as an interchange format. A future `.mia` format is intended to preserve MiaIA-specific information such as editor layout, debug state, annotations, breakpoints, training history, and visualization metadata. A `.mia` project should still be exportable to ONNX when its model graph is representable by the supported ONNX subset.

## Documentation rule

Documentation must distinguish clearly between:

- **implemented** behavior that exists in the current repository;
- **planned** behavior described by the roadmap;
- **historical** material preserved to explain the origin of the project.

Project documentation, source identifiers, comments, tests, and commit messages are written in English.
