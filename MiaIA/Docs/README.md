# MiaIA Documentation

MiaIA is an Interactive Development Environment for Artificial Intelligence. Its purpose is not to compete with large training frameworks on throughput. Its purpose is to make a neural network observable: users should be able to build a model, execute it, inspect its state, evaluate samples, follow gradients, and eventually control training step by step.

The project is currently in its foundation stage. The C++ engine, public SDK facade, shared command processor, terminal and Unreal command consoles, test harness, ONNX interchange, CSV dataset pipeline, versioned `.mai` project persistence, fixed-model dataset evaluation, non-mutating gradient inspection, phase-by-phase SGD debugging, controlled sessions, background pause/resume, navigable training history, interactive 2D and first runtime 3D topology views, renderer-neutral Studio application foundation, shared Unreal editor/standalone runtime UI, and verified Win64 packaging workflow are implemented. The complete production visualization experience remains planned work.

## Documentation map

- [Architecture](Architecture/Architecture.md) describes module boundaries, supported data flows, and current technical constraints.
- [Console guide](Console/Console.md) documents every interactive command and introduces the neural-network concepts behind them.
- [Unreal integration](Unreal/Unreal.md) documents the current Blueprint-facing SDK adapter and build workflow.
- [MiaIA Studio](Studio/Studio.md) documents the shared graphical application model and standalone delivery path.
- [MiaIA project format](Project/Project.md) defines the `.mai` v1 container and its safety behavior.
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
| Creation | Configurable hidden/output activations, uniform initial weights, and non-input biases with backward-compatible defaults |
| Execution | Input assignment and forward propagation |
| Inference | Direct input-to-output prediction through SDK and Console |
| Inspection | Network, dataset, focused neuron/connection debug, gradient, session-history, and completed-step snapshots |
| Interchange | Import and export of the currently supported dense ONNX subset |
| Projects | Atomic `.mai` v1 save/open with embedded ONNX, dataset reference, training configuration, and breakpoints |
| Datasets | Numeric CSV import with explicit input and target column counts |
| Evaluation | Per-sample details and fixed-model mean squared error across a complete dataset |
| Differentiation | Per-neuron, per-bias, and per-connection gradients without parameter updates |
| Optimization | Standalone and session-attached phase debugging, atomic SGD, ordered epochs, history, bounded runs, and background control |
| Clients | Shared CLI hosted by Console.exe and Unreal, Blueprint nodes, a shared editor/standalone topology panel, and renderer-neutral StudioCore |
| Verification | Named Debug- and Release-capable C++ test harness, including numerical gradient checks |

## Solution structure

```text
MiaIA/
|-- Core/       Stable data structures, activation primitives, and public snapshots
|-- Engine/     Validation, execution, evaluation, differentiation, optimization, and training
|-- SDK/        Client-facing MiaIAClient facade and process-local state
|-- CLI/        Reusable textual command parser and SDK dispatcher
|-- Console/    Thin terminal host for the shared command processor
|-- Tests/      Named integration and mathematical correctness tests
|-- IDE/        Graphical IDE client implementations
|   |-- StudioCore/ Platform-neutral Studio state and 2D/3D topology scenes
|   `-- Unreal/     Runtime Studio host, Unreal Engine client, and editor integration
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

The Unreal-based standalone application is packaged through `IDE/Unreal/Build/Package-Windows.ps1`. A Shipping archive can then be converted into an unsigned Microsoft Store MSIX through `IDE/Unreal/Build/Package-StoreMsix.ps1`. See the [Unreal integration guide](Unreal/Unreal.md#packaged-windows-application) for the complete build, Store, and verification workflows.

## Architectural direction

All clients use the SDK rather than reaching into Engine or Core internals. Text commands are implemented once in the CLI module and hosted by both `Console.exe` and Unreal. Structured Unreal and Blueprint operations continue to use the SDK facade directly. A separate executable does not share current state because the SDK context is process-local.

ONNX is used as an interchange format. The versioned `.mai` project format preserves the supported model together with the current dataset reference, training configuration, and breakpoint definitions. It deliberately does not yet preserve transient training progress, history, annotations, or visualization layout. A `.mai` project remains exportable to ONNX when its model graph is representable by the supported ONNX subset.

## Documentation rule

Documentation must distinguish clearly between:

- **implemented** behavior that exists in the current repository;
- **planned** behavior described by the roadmap;
- **historical** material preserved to explain the origin of the project.

Project documentation, source identifiers, comments, tests, and commit messages are written in English.

## License and project governance

Original MiaIA source code is published under the Mozilla Public License 2.0. Unreal Engine and every third-party dependency retain their own license terms, while MiaIA names and branding are outside the source-code license. The root `LICENSING.md`, `THIRD_PARTY_NOTICES.md`, and `TRADEMARKS.md` files define these boundaries.

Copyright 2026 Agostino Mosillo. Official project information and executable downloads are published at [www.nonop.biz](https://www.nonop.biz); the corresponding source remains available from the public repository identified in the root licensing documents.

The repository is currently author-led. External pull requests and unsolicited code contributions are not accepted until a contribution and copyright-management process is introduced; issue reports and focused feedback remain welcome under the root contribution policy.
