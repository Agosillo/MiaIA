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
- bounded synchronous session runs by step count or to completion;
- explicit run stop reasons and partial-progress snapshots;
- progressive preservation of successful steps before a later failure.
- serialized SDK access to network, dataset, and session state;
- non-blocking background session execution;
- cooperative pause, resume, and cancellation at atomic step boundaries;
- coherent inspection and explicit worker stop reasons while training.
- lightweight session history summaries;
- detailed lookup of retained evaluations, gradients, and parameter updates.
- phase-by-phase single-sample training transactions;
- candidate-network inspection through forward, backward, update, verification, and atomic commit;
- true candidate rollback before commit.
- focused neuron and connection inspection at every debug phase;
- explicit public-versus-candidate values and phase availability flags.
- phase debugging attached to the current controlled-session sample;
- atomic promotion of a committed debug step into session history and progress.
- one reusable CLI command processor shared by the terminal and Unreal editor hosts;
- first dockable Unreal topology, explorer, focused inspector, phase controls, and interactive command console;
- shared contextual command catalog plus Unreal suggestion filtering, Tab completion, and session-local command history.
- renderer-neutral StudioCore with shared detailed/compact policies, selection state, command execution, and normalized 2D/3D topology scenes;
- initial Unreal consumption of StudioCore scalability and two-dimensional layout rules;
- runtime-capable Unreal Studio UI shared by the editor panel and the `MiaIAStudio` game target.

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
- bounded repeated execution with `run <steps>` and `run all`.

## Implemented automatic session control

- run a session automatically without blocking client interaction;
- pause, resume, and cancel the worker safely;
- expose structured worker stop reasons;
- reject unsafe mutations while Running and preserve coherent inspection.

## Next training controls

- add deterministic seeding where randomness is introduced;
- add configurable sample ordering;
- introduce mini-batches only after single-sample behavior remains transparent.

## Debugging experience

The first debugger-like training workflow is implemented for one sample: clients can stop before forward propagation, advance through forward, backward, candidate update and verification, then commit or cancel without changing the public network prematurely.

The debugging experience should next evolve toward:

- configurable breakpoints around the implemented mathematical phases;
- expand selected-neuron and selected-connection inspection with incoming and outgoing relationships;
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

- implemented the first Blueprint runtime adapter for dense creation, CSV import, controlled sessions, phase advancement, cancellation, and focused debug inspection;
- added Unreal-reflected phase, session, neuron, and connection snapshots without leaking STL types;
- exposed reusable SDK and shared CLI operations through Unreal C++ wrappers;
- added a dockable topology panel with neuron and connection selection, candidate values, phase controls, and a shared interactive command console;
- added a lightweight network overview and automatic compact layer rendering for large topologies;
- broaden Blueprint coverage to the remaining appropriate SDK operations;
- refine the IDE command console with persistent history and asynchronous long-running execution;
- add paged drill-down from compact layer summaries into large-model neurons and connections;
- expand neuron coloring beyond activation and current debug state;
- animate forward signal flow and backward gradient flow;
- navigate from network overview to an individual neuron or connection;
- compare snapshots and training checkpoints;
- keep Unreal-specific rendering data outside the mathematical Core.

## MiaIA Studio delivery

The platform-neutral Studio application foundation and shared Unreal runtime host are implemented. The next delivery steps are:

- verify the `MiaIAStudio` game target as the independent development host;
- add a user-selectable 2D/3D topology mode;
- implement the first interactive three-dimensional renderer over StudioCore coordinates;
- package and verify a Windows `MiaIAStudio.exe` that does not require Unreal Editor;
- retain StudioCore as the shared application boundary for a possible future Qt frontend.

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
