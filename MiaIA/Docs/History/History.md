# Project History

## Origin

The original intent and early architectural thinking are preserved in [000_ProjectGenesis.md](../Architecture/000_ProjectGenesis.md). That document is historical source material and is intentionally not rewritten as the implementation evolves.

## Foundation sequence

The current repository developed through the following implementation stages.

### Initial Core, Engine, SDK, and Console

The first foundation established explicit network objects, forward propagation, a static SDK facade, and an interactive Console capable of creating and inspecting a dense network.

This stage demonstrated the central architectural idea: clients call a reusable C++ SDK while the engine performs the mathematical work.

### Unreal Engine integration proof

An initial Unreal Engine project linked against the SDK and retrieved a dense-network snapshot. This verified that the engine could live outside the graphical IDE and that Unreal could act as a client rather than the owner of the model.

Release configuration and linkage issues were corrected so the foundation could be consumed consistently by the native and Unreal solutions.

### Test harness and facade completion

A named test harness replaced ad hoc checks and remained active in both Debug and Release configurations. Network creation, editing, queries, parameters, topology, validation, input assignment, execution, and snapshots were progressively moved behind focused Engine classes and the `MiaIAClient` facade.

This work reduced direct manipulation of shared state and clarified responsibility boundaries.

### ONNX interchange

The engine gained ONNX export followed by ONNX import and round-trip tests. The first supported graph family is the dense feed-forward subset represented by `Gemm` and supported activation operators.

ONNX was established as model interchange rather than the final MiaIA workspace format.

### Dataset pipeline

Numeric CSV import introduced an explicit dataset model with input and target dimensions. SDK and Console operations were added for dataset summaries, individual sample inspection, applying a sample to network inputs, and clearing the current dataset.

### Sample evaluation

Dataset samples were connected to forward propagation and mean squared error. An evaluation snapshot made targets, predictions, signed errors, and loss observable without changing model parameters.

### Observable backward propagation

Backward propagation was added as a separate differentiation subsystem. It exposes activation, pre-activation, bias, and weight gradients through public snapshots while leaving weights and biases unchanged.

Known analytical cases and numerical finite-difference checks verify the gradient implementation. This is the present edge of the implemented training foundation.

### Fixed-model dataset evaluation

Evaluation was extended from one selected sample to the complete dataset. The engine evaluates a private network copy, retains every sample result, and reports the arithmetic mean of the sample losses without changing public parameters or activations.

This provides a stable metric for comparing the same dataset before and after an epoch, independently of the epoch's sequential before/after trace means.

### Atomic SGD training step

The first explicit optimizer operation connected observed gradients to parameter updates. A single sample can now perform forward propagation, MSE evaluation, backward propagation, SGD weight and bias updates, and a second evaluation.

The operation runs against a candidate network and publishes it only after every stage succeeds. Its snapshot records the loss before and after the step and every applied parameter delta. Invalid learning rates, numerical overflow, unsupported options, and incompatible samples leave both the network and caller result unchanged.

### Direct prediction pipeline

A target-free inference operation combined input validation, forward propagation, and structural output-layer discovery behind `MiaIAClient::Predict`. Console users can now submit an input vector with one command and receive the output vector directly, while `input` plus `forward` remain available for phase-by-phase debugging.

### Atomic dataset training epoch

Single-sample training was composed into one ordered pass over the loaded dataset. Every sample retains its complete `TrainingStepSnapshot`, while the epoch reports running mean losses before and after the individual updates.

The full epoch uses a candidate network and is published only after all samples succeed. Failure on a later sample therefore rolls back earlier candidate updates and preserves both the public network and the caller result.

### Controlled multi-epoch training session

The ordered training primitives were composed into a synchronous session with explicit Start, Status, Next, and Cancel operations. A session remains paused between commands, and each Next call executes exactly one previously verified atomic sample step.

Session snapshots expose state, epoch and sample cursors, progress, optimizer configuration, and the complete step history. Completion and cancellation are distinct terminal states; cancellation preserves successful updates, while a rejected step leaves both network and session position unchanged.

### Bounded synchronous session run

Controlled sessions gained repeated execution with either an explicit step limit or all remaining steps. Run snapshots record start and end cursors, successful step details, trace loss means, and whether execution stopped at its limit, session completion, or a failed step.

Runs deliberately preserve successful earlier steps when a later step fails. This progressive behavior supports debugging and retry at the failed cursor while retaining the atomic guarantee of every individual update.

## Current position

MiaIA can now represent, execute, interchange, inspect, evaluate individual samples or a complete fixed-model dataset, differentiate, apply atomic SGD steps and epochs, manually advance controlled sessions, and execute bounded synchronous runs. It cannot yet run a background pausable training worker, persist a `.mia` workspace, or deliver the planned complete graphical debugging experience.

Those next steps are tracked in the [Roadmap](../Roadmap/Roadmap.md).
