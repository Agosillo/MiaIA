# Coding Guidelines

## Scope

These guidelines describe the conventions already used by the MiaIA C++ foundation and the expectations for future contributions. They are intended to protect module boundaries and make mathematical behavior reviewable.

## Language

All repository-facing material is written in English:

- identifiers;
- comments;
- user-visible Console text;
- documentation;
- tests;
- commit messages.

This rule keeps the public SDK and documentation accessible to contributors and client developers regardless of the language used in project discussions.

## C++ baseline

- C++20;
- MSVC v143 is the current compiler toolset;
- warnings should be treated as actionable when they originate in MiaIA code;
- Debug and Release behavior must both remain valid;
- external-library warnings should be distinguished from project warnings.

## Naming

The existing codebase uses:

- `PascalCase` for types, public methods, and public data fields;
- `camelCase` for parameters and local variables;
- descriptive subsystem names such as `NetworkValidator`, `SampleEvaluator`, and `BackwardEngine`;
- `TryGet...` for queries that can fail without being exceptional;
- `Snapshot` suffixes for public read-only value representations;
- `Evaluator`, `Importer`, `Exporter`, `Inspector`, `Editor`, and `Runtime` suffixes to communicate responsibility.

Names should describe the represented concept rather than the current user interface. Engine code should not contain Unreal-, Console-, or Blueprint-specific terminology.

## Namespace and module boundaries

Core types live in `MiaIA::Core`, engine operations in `MiaIA::Engine`, the client facade in `MiaIA::SDK`, and reusable text-command dispatch in `MiaIA::CLI`.

The allowed dependency direction is:

```text
Client -> CLI -> SDK -> Engine -> Core
Client --------> SDK -> Engine -> Core
```

Do not introduce reverse dependencies. In particular:

- Core must not call Engine or SDK;
- Engine must not know about Console or Unreal;
- CLI may translate commands but must not reimplement Engine mathematics;
- clients should not mutate `Core::Network` directly;
- new client features should first be exposed through `MiaIAClient` when they represent shared functionality.

## Single responsibility

Prefer a focused class in the appropriate subsystem over a large multipurpose manager. Examples from the current design include separate ONNX importer and exporter classes, separate loss and sample evaluators, and a backward engine that calculates gradients without optimizing parameters.

A class should have one primary reason to change. File and folder placement should make that reason visible.

## Validation and state changes

Validate all externally supplied values before publishing state:

- identifiers must respect topology invariants;
- numeric inputs, weights, biases, and samples must be finite;
- input and target dimensions must match the current operation;
- failed imports must not replace valid current state;
- output result parameters should be assigned only after a complete successful calculation.

Use a temporary object when an operation replaces a complete network, dataset, or snapshot. Move the temporary into the destination only after validation succeeds.

## Public SDK design

`MiaIAClient` is a facade, not a second engine implementation. SDK methods should delegate to a focused Engine class and expose Core public snapshots where inspection is required.

Public calls should:

- use clear value and snapshot types;
- return `bool` for expected validation failures;
- preserve caller-supplied result objects when an operation fails;
- avoid exposing mutable engine-owned references;
- behave consistently for Console, Unreal, and future clients.

When the facade grows, group implementations by responsibility in separate source files while preserving one coherent public client API.

The CLI command processor is a host-independent adapter. It must not own a terminal loop, terminate a process, or change the process working directory. Hosts supply a command and working directory, receive textual output plus an explicit exit request, and decide how those results affect their own interface.

Command syntax and descriptions belong to the CLI catalog rather than an individual host. Contextual completion should query that catalog so additions remain testable and consistent across clients. Keyboard navigation, visual suggestion lists, and history lifetime remain host responsibilities.

## Mathematical code

Mathematical operations must favor clarity and verifiability:

- name the quantity represented by each gradient;
- document the sign convention for errors;
- make averaging factors explicit;
- reject non-finite intermediate results;
- keep observation separate from mutation;
- avoid hidden parameter updates inside evaluation code.

The current convention is:

```text
error = prediction - target
MSE = sum(error^2) / output-count
dMSE/dPrediction = 2 * error / output-count
```

## Concurrency

The SDK owns the synchronization boundary for its process-local network, dataset, and training session. Every public SDK operation must acquire the shared client-state lock before reading or writing those objects.

Background training may publish state only after one complete atomic sample step. Inspection is allowed while the worker runs because it observes state under the same lock. Mutating calls must reject Running state rather than interleave with the worker. Pause and cancellation must request cooperative stop and join the worker without holding the client-state lock.

Concurrency tests should synchronize through public state and join operations. Avoid fixed sleeps as correctness conditions.

## Formatting and includes

- use `#pragma once` in headers;
- place project includes before standard-library includes when this improves dependency readability;
- include the declarations a file uses rather than relying on accidental transitive includes;
- use braces for control-flow blocks;
- keep public signatures vertically formatted when they would otherwise become difficult to scan;
- prefer `const` and `[[nodiscard]]` when they clarify intent.

Formatting changes should not be mixed with unrelated behavioral work.

## Snapshots

Snapshots are public data-transfer values. They should be:

- independent of engine object lifetime;
- safe to copy or move;
- explicit about identifiers and represented quantities;
- free of client-specific rendering state;
- assigned atomically after successful inspection or calculation.

MiaIA-specific persistent state belongs to dedicated project or visualization structures rather than being inserted into the mathematical network model without a clear boundary. The `.mai` v1 implementation follows this rule through `ProjectArchive` and `ProjectInfoSnapshot`; future visualization sections should preserve the same separation.

## Tests

Every behavioral change should include a focused named test group. Tests must remain active in Release builds; use the MiaIA test harness rather than relying on standard `assert`, which may be compiled out.

Tests should cover:

- the successful path;
- rejected invalid input;
- preservation of state after failure;
- invariants across SDK boundaries;
- exact values for simple mathematical cases;
- numerical gradient checks for differentiation code;
- import/export round trips where interchange is involved.

Temporary test files should be created in the system temporary directory and removed by the test. User-created files such as Console experiments must not be committed accidentally.

## Documentation and commits

Documentation must describe current behavior accurately and label future behavior as planned. Update the relevant document when a public command, SDK contract, supported format, or architectural boundary changes.

Commits should be thematic and reviewable. Do not include local datasets, generated build products, IDE recovery files, or unrelated user changes.
