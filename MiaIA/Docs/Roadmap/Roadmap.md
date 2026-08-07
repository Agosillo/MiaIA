# MiaIA Roadmap

The roadmap is organized by dependency rather than by a fixed release date. Mathematical correctness and observable state come before a complex graphical experience.

## Completed foundation

- layered Core, Engine, SDK, and Client architecture;
- network creation, editing, validation, and snapshots;
- Sigmoid, ReLU, Tanh, and Linear forward propagation;
- Console client and initial Unreal Engine linkage;
- Debug- and Release-capable named test harness;
- ONNX export, import, and round-trip coverage for the supported dense subset;
- numeric CSV dataset import and sample inspection;
- sample application and forward evaluation;
- mean squared error and signed output errors;
- observable backward propagation;
- neuron, bias, and connection gradient snapshots;
- analytical and numerical gradient verification.
- atomic single-sample SGD training step;
- before/after loss and parameter update snapshots;
- transactional rejection of invalid or non-finite updates.
- direct SDK and Console prediction for single- and multi-output networks.
- one ordered, atomic full-dataset SGD epoch;
- per-sample step snapshots and running epoch loss means;
- complete epoch rollback when any sample fails.
- fixed-model full-dataset evaluation with per-sample results and mean loss;
- non-mutating public network state during complete dataset evaluation.
- synchronous multi-epoch training sessions;
- explicit session start, status, single-step advance, and cancel operations;
- session epoch/sample cursors and complete step history;
- state preservation when a controlled step is rejected.

## Implemented optimizer foundation

The engine can turn an inspected gradient into one explicit SGD parameter update:

- `train step` is available through SDK and Console;
- learning rate and all calculated parameters are validated;
- the operation is atomic at the public network boundary;
- before/after loss and every weight and bias delta are observable;
- tests verify exact updates, loss reduction, and state preservation on failure.

Gradient evaluation remains a separate non-mutating operation, so a client can inspect the mathematical result before choosing to execute a training step.

One explicit full-dataset pass is also available through `train epoch`. It reuses the observable single-sample steps in CSV order and preserves the whole public network if any step fails.

## Implemented controlled training foundation

Building on trustworthy single steps and one explicit epoch, the current session layer provides:

- a multi-epoch session lifetime with ordered sample traversal;
- current epoch and next sample indexes;
- explicit cancellation and one-step advancement;
- retained loss and parameter-delta history through full step snapshots;
- a natural pause boundary between synchronous commands.

## Next milestone: automatic session control

- run a session automatically without blocking client interaction;
- pause and resume the automatic worker safely;
- expose structured stop reasons and diagnostics;
- add deterministic seeding where randomness is introduced;
- add configurable sample ordering;
- introduce mini-batches only after single-sample behavior remains transparent.

## Debugging experience

The engine and SDK should evolve toward a debugger-like training workflow:

- breakpoints before and after forward, backward, and update phases;
- inspect a selected neuron or connection;
- compare current and previous activations, gradients, weights, and biases;
- identify inactive or consistently saturated neurons;
- trace signal and gradient flow through the network;
- preserve selected checkpoints for comparison;
- provide structured diagnostics rather than only Boolean failure results.

## Persistence and interchange

- design and version the `.mia` workspace format;
- save model topology and parameters;
- save dataset references without silently embedding unavailable external data;
- persist editor layout, annotations, debug state, and training history;
- reload a workspace safely;
- export the representable model graph to ONNX;
- broaden ONNX support incrementally with explicit compatibility tests.

The `.mia` format is planned and does not exist in the current implementation.

## Unreal Engine IDE

- expose reusable SDK operations through Unreal C++ wrappers;
- expose appropriate operations to Blueprint nodes;
- provide an IDE command console;
- visualize layers, neurons, and weighted connections;
- color neurons using activation and debug state;
- animate forward signal flow and backward gradient flow;
- navigate from network overview to an individual neuron or connection;
- compare snapshots and training checkpoints;
- keep Unreal-specific rendering data outside the mathematical Core.

## Additional clients

Once the SDK contract and session model are stable:

- support another client such as Unity or a lightweight desktop viewer;
- introduce explicit engine/session contexts instead of one process-global state;
- define thread-safety and lifetime rules;
- consider language bindings only where the SDK boundary is sufficiently stable.

## Model analysis

Longer-term analysis features may include:

- side-by-side model comparison;
- activation and gradient distributions;
- dead-neuron and saturation analysis;
- weight-change heat maps;
- sample-specific execution traces;
- checkpoint comparison;
- targeted parameter experiments;
- model interrogation from Console and graphical clients.

## Non-goals for the foundation phase

- competing with mature high-performance training frameworks;
- claiming complete ONNX operator coverage;
- hiding mathematical stages behind one opaque training call;
- coupling the engine to a single graphical client;
- building the full visualization layer before the training primitives are correct and observable.
