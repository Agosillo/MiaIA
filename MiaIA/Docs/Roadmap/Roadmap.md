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

## Next milestone: explicit training step

The next engine milestone should turn an inspected gradient into an explicit, controlled parameter update.

Planned work:

- optimizer abstraction;
- stochastic gradient descent as the first optimizer;
- learning-rate validation;
- apply-gradient operation separated from gradient calculation;
- before/after parameter snapshots;
- one-sample `train step` SDK and Console operation;
- tests proving the expected weight and bias update;
- tests proving that a suitable step can reduce loss.

This milestone should preserve the ability to stop between backward propagation and parameter application.

## Controlled training sessions

After a trustworthy single step exists:

- iterate over a dataset;
- define epochs and sample ordering;
- expose current epoch and sample index;
- pause, resume, cancel, and advance one step;
- retain loss history;
- expose parameter deltas for each step;
- add deterministic seeding where randomness is introduced;
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
