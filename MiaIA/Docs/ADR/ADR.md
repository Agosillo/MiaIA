# Architectural Decision Register

This document summarizes the architectural decisions currently reflected by the MiaIA repository. It is a living register, not a replacement for the preserved [Project Genesis](../Architecture/000_ProjectGenesis.md).

Future decisions that require a detailed comparison of alternatives should be recorded as separate numbered ADR files with context, decision, consequences, and status.

## ADR-001: English is the repository language

**Status:** Accepted

**Decision:** Source identifiers, comments, documentation, tests, Console output, and commit messages are written in English.

**Reason:** MiaIA is intended to support external users, multiple clients, and future contributors. A single repository language reduces friction across those boundaries.

## ADR-002: Use a layered engine architecture

**Status:** Accepted

**Decision:** Organize the system as Core, Engine, SDK, and Clients, with dependencies pointing inward.

**Reason:** The mathematical engine must remain independent of Unreal Engine or any other presentation technology. Focused subsystems also allow contributors to improve one responsibility without rewriting every layer above it.

**Consequence:** Clients do not manipulate engine internals directly. Shared operations must be designed at the Engine and SDK boundaries.

## ADR-003: Make `MiaIAClient` the client facade

**Status:** Accepted

**Decision:** Console, Unreal Engine, and future clients call one SDK facade.

**Reason:** The same capability should be accessible from C++, a command console, Blueprint wrappers, or another editor without duplicating the neural-network implementation.

**Consequence:** The Console acts as a reference SDK client. A feature implemented only inside a client is not yet a reusable MiaIA engine capability.

## ADR-004: Expose observable state through snapshots

**Status:** Accepted

**Decision:** Return value snapshots for network state, datasets, evaluations, and gradients instead of exposing mutable engine-owned references.

**Reason:** Debugging and visualization need stable data that can be displayed, compared, logged, and eventually transported across editor boundaries.

**Consequence:** Snapshot schemas are part of the public contract and should evolve deliberately.

## ADR-005: Use ONNX for model interchange

**Status:** Accepted with a deliberately limited initial subset

**Decision:** Import and export supported model graphs through ONNX. The current implementation focuses on dense feed-forward `Gemm` graphs with supported activation functions.

**Reason:** ONNX is a standard model interchange format and avoids inventing a proprietary representation for model exchange.

**Consequence:** ONNX compatibility does not imply support for every ONNX operator. Unsupported graphs must fail safely. MiaIA-specific editor and debugging data does not belong in the plain interchange graph.

## ADR-006: Reserve `.mia` for the MiaIA workspace

**Status:** Planned; format not implemented

**Decision:** Introduce a future `.mia` format only when MiaIA-specific information must be persisted.

**Reason:** Editor layout, annotations, breakpoints, debug sessions, training history, and visualization metadata exceed the responsibility of ONNX.

**Consequence:** A `.mia` workspace should retain an export path to ONNX for the representable model portion.

## ADR-007: Start dataset interchange with explicit numeric CSV

**Status:** Accepted

**Decision:** Import rows from CSV by requiring the caller to specify input and target column counts.

**Reason:** A small, transparent format is sufficient to validate the input, evaluation, and differentiation pipeline before introducing richer dataset schemas.

**Consequence:** CSV currently carries finite numeric samples only. Feature names, categorical encodings, preprocessing pipelines, and dataset metadata are not represented.

## ADR-008: Separate differentiation from optimization

**Status:** Accepted

**Decision:** Backward propagation produces gradient snapshots without applying them to weights or biases.

**Reason:** In MiaIA, gradients are first-class debugging information. Users and graphical clients must be able to inspect what an optimizer would use before a parameter update occurs.

**Consequence:** Evaluation and gradient commands are non-training operations. Optimizers and training steps will be added as a separate layer that explicitly consumes gradients.

## ADR-009: Keep tests active in Debug and Release

**Status:** Accepted

**Decision:** Use a named test harness with checks that are not removed by `NDEBUG`.

**Reason:** Release-only linking, dependency, and numerical behavior must be detectable. Standard C assertions are insufficient when they disappear from Release builds.

**Consequence:** New features require named tests, and mathematical features should include known-value or numerical verification.

## ADR-010: Publish original MiaIA source under MPL 2.0

**Status:** Accepted

**Decision:** Publish original MiaIA source code under the Mozilla Public License 2.0 while leaving Unreal Engine, third-party dependencies, and MiaIA branding under their respective terms. Keep the repository author-led and closed to unsolicited code contributions until a contribution and copyright-management process is established.

**Reason:** File-level copyleft keeps distributed modifications to MiaIA source files available while allowing the engine and SDK to participate in larger applications and leaving room for separately implemented modules under other terms. Delaying external code contributions preserves clear copyright ownership during the initial public phase.

**Consequence:** Source and executable distributions must identify the corresponding source location and preserve MPL and third-party notices. New dependency and packaging changes require a license review. The contribution policy must be revised before external patches are accepted.
