# MiaIA Console Guide

## Purpose

The MiaIA Console is an interactive SDK client. Every command translates user input into `MiaIAClient` calls, so the Console exercises the same public boundary intended for Unreal Engine and future clients.

Command parsing and dispatch live in the reusable CLI command processor. `Console.exe` is only its terminal host; the Unreal editor panel calls the same processor directly. A command therefore has the same syntax and behavior in both interfaces and operates on the host process's shared `MiaIAClient` state.

The state is process-local. A host contains one current MiaIA project state; save it as `.mai` to restore the supported model, dataset reference, training configuration, and breakpoints in a later process. Starting `Console.exe` beside Unreal does not connect it to the editor: the two processes own independent state and must open or save projects explicitly. Remote sessions are not implemented.

Relative paths are resolved from the host working directory. For `Console.exe`, this is the directory from which it was launched. In the Unreal editor console, it is the Unreal project directory that contains `IDE.uproject`. Absolute paths work in both hosts.

## Command discovery and host history

The shared CLI module exposes a structured command catalog in addition to execution. Each suggestion contains the text that can be completed, the full command syntax, and a short description. Filtering is contextual rather than a flat search:

```text
tr
    -> train

train s
    -> train step <sample-index> <learning-rate> mse
    -> train session <action>

train session r
    -> train session run <steps|all>
    -> train session resume

dataset import
    -> dataset import csv <inputs> <targets> [--no-header] <path>

project
    -> project new
    -> project open <path.mai>
    -> project save [path.mai]
    -> project info
```

Once a command starts accepting values, its complete syntax remains visible while the values are entered. For example, `create 2` continues to display the four required shape arguments and every optional initialization argument.

History belongs to the host because keyboard behavior is an interface concern. The Unreal editor panel currently provides session-local Up/Down history, clickable suggestions, and Tab completion. The thin `Console.exe` host still relies on the terminal's ordinary line input and does not yet provide interactive suggestion rendering. Neither host persists command history after it closes.

## Notation

This guide uses:

- `<value>` for a required argument;
- `<value...>` for one or more required values;
- `[--option]` for an optional flag.

Sample indices are zero-based. Paths containing spaces should be enclosed in double quotes.

## Quick start

The following session creates a two-input network, runs a direct prediction, imports a dataset, evaluates its first sample, and inspects the gradients used by SGD:

```text
create 2 2 1 1
predict 1 1
dataset import csv 2 1 sample.txt
dataset summary
dataset inspect 0
dataset evaluate 0 mse
dataset evaluate all mse
dataset gradients 0 mse
dataset diagnose
train step 0 0.01 mse
train debug start 0 0.01 mse
train debug next
train debug next
train debug status
train debug neuron 1003
train debug connection 1
train debug cancel
train epoch 0.01 mse
dataset evaluate all mse
train session start 2 0.01 mse
train breakpoint add activation-above 1005 0.9
train breakpoint list
train session next
train session debug
train debug next
train debug cancel
train session status
train session run 1
train session history
train session inspect 0
train session compare 0 2
train session cancel
train session start 100 0.01 mse
train session resume
train session status
train session pause
train session cancel
project save xor-training.mai
```

`dataset evaluate` and `dataset gradients` require a current network whose input and output dimensions match the dataset.

## General commands

### `help`

```text
help
```

Displays the command list available in the current Console build.

### `exit`

```text
exit
```

Closes the Console. The current in-memory network and dataset are discarded.

## Network creation and input

### `create`

```text
create <inputs> <neurons-per-hidden-layer> <hidden-layers> <outputs>
       [--hidden-activation sigmoid|relu|tanh|linear]
       [--output-activation sigmoid|relu|tanh|linear]
       [--weight <initial-weight>] [--bias <initial-bias>]
```

Creates a new fully connected feed-forward network and replaces the current network.

Arguments:

- `inputs`: number of input neurons;
- `neurons-per-hidden-layer`: width shared by every hidden layer;
- `hidden-layers`: number of hidden layers; zero is allowed by the engine;
- `outputs`: number of output neurons;
- `--hidden-activation`: activation used by every hidden layer;
- `--output-activation`: activation used by the output layer;
- `--weight`: finite initial value assigned to every connection;
- `--bias`: finite initial value assigned to every hidden and output neuron.

Example:

```text
create 784 256 3 10
```

This describes the shape:

```text
784 -> 256 -> 256 -> 256 -> 10
```

The optional arguments can describe different experiments without changing Engine code. A classification-oriented example can retain a Sigmoid output while using ReLU hidden layers:

```text
create 784 256 3 10 --hidden-activation relu --output-activation sigmoid
```

A regression-oriented network can instead use a Linear output and a smaller uniform starting weight:

```text
create 4 16 2 1 --hidden-activation tanh --output-activation linear --weight 0.01 --bias 0
```

Defaults preserve the original behavior: hidden and output activations are Sigmoid, every connection starts at `0.1`, and every non-input bias starts at `0.0`. The input layer is deliberately not activation-configurable: `input` and `predict` assign raw values to it, and preprocessing belongs in a separate future data pipeline rather than being hidden inside network creation. Its biases remain `0.0`.

Dense size grows primarily with connections. For `I` inputs, hidden width `H`, `L` hidden layers, and `O` outputs, a network with hidden layers contains `I*H + (L-1)*H*H + H*O` connections. For example, `create 100 100 100 1` creates 10,101 neurons and 1,000,100 connections. The factory builds this graph in a preallocated linear batch and publishes it only after final validation. Graphical clients may intentionally switch to an aggregate layer view rather than drawing every connection.

Calling `create` without arguments currently uses the Console defaults `10 32 2 3` together with the initialization defaults above. Once a shape is started, all four positional values are required before any optional argument. Supplying the full configuration explicitly is recommended because it makes the experiment reproducible and clear.

### `network configure`

```text
network configure
        [--hidden-activation sigmoid|relu|tanh|linear]
        [--output-activation sigmoid|relu|tanh|linear]
        [--weight <value>] [--bias <value>]
```

Atomically changes one or more parameter groups in the current network. Omitted options preserve their current values. At least one option is required.

```text
network configure --hidden-activation tanh --output-activation linear
network configure --weight 0.01 --bias 0
```

The Engine applies the complete request to a candidate copy, validates it, and publishes it only when every requested value is valid. Input activations and biases are never changed. The command reports the number of hidden layers, output layers, connection weights, and neuron biases that actually changed.

Changing activation functions preserves current weights and biases. In contrast, `--weight` replaces **every** connection weight and `--bias` replaces **every** hidden and output bias. Using either option after training therefore discards the corresponding learned parameter values. Network mutation is rejected while automatic training or a phase-debug transaction owns the model.

### `network set neuron-bias`

```text
network set neuron-bias <neuron-id> <value>
```

Changes one non-input neuron bias identified by its stable model ID. Input-layer biases are not trainable parameters and the command rejects them. The value must be finite.

```text
network set neuron-bias 1003 0.25
```

### `network set connection-weight`

```text
network set connection-weight <connection-id> <value>
```

Changes one connection weight without affecting any other model parameter.

```text
network set connection-weight 7 -0.1
```

Both targeted commands use the same serialized `MiaIAClient` mutation boundary as every other frontend. Unknown IDs, invalid values, automatic training, and an active phase-debug transaction leave the public network unchanged.

### `input`

```text
input <value...>
```

Assigns activation values to the input layer. The number of values must exactly equal the input neuron count, and every value must be finite.

Example:

```text
input 0.5 0.2 0.9
```

This command changes input activations only. Use `forward` afterward to propagate the new input through the network.

### `predict`

```text
predict <value...>
```

Validates an input vector, applies it to the current network, performs forward propagation, and prints only the output vector.

Examples:

```text
predict 1 1
Prediction: 0.87
```

```text
predict 0.2 0.5 0.9
Prediction: 0.04, 0.91, 0.05
```

The number of supplied values must match the input layer. The output values are collected from the highest-order layer, so prediction does not depend on a layer being named `Output`.

Prediction is inference: it requires no dataset and no target values. It updates neuron activations with the successful forward result but does not change weights or biases. Invalid dimensions, non-finite inputs, an invalid network, or non-finite outputs fail without replacing the caller result; previous activations are preserved or restored.

Use `predict` when only the model output is needed. Use separate `input`, `forward`, and `inspect` commands when examining execution phase by phase.

### `trace forward`

```text
trace forward <value...>
```

Calculates a read-only forward execution trace for one input vector. The command prints every ordered layer and every neuron with these values:

- `weighted`: the sum of incoming `source activation * weight` terms, before bias;
- `bias`: the neuron's bias;
- `pre-activation`: `weighted + bias` for a non-input neuron;
- `activation`: the value after the layer activation function.

Input neurons are marked as `input`. Their weighted sum is zero and their raw supplied value is both pre-activation and activation because the input layer does not transform values.

```text
trace forward 1 1
```

The operation evaluates a private network copy. It does not replace current activations, reorder public layers, or modify weights and biases. This differs intentionally from a successful `predict`, which leaves its calculated activations available as current runtime state.

### `trace backward`

```text
trace backward <input...> -- <target...>
```

Calculates a read-only backward gradient-flow trace for one input and target pair. The `--` delimiter separates the input vector from the target vector. The current implementation uses mean squared error and prints the captured predictions, signed errors, loss, and every layer in reverse order from outputs to inputs.

Each neuron record contains its captured activation, `dL/da`, `dL/dz`, and bias gradient. Each connection contains its weight gradient and its signed contribution to the source neuron's activation gradient: `weight * target dL/dz`.

```text
trace backward 1 1 -- 1
```

The operation runs forward evaluation and differentiation on a private network copy. It does not apply an optimizer, publish temporary activations, or change any weight or bias. Input or target dimension errors, non-finite values, an invalid network, or an unsupported loss leave both the public network and caller-owned SDK result unchanged.

### `trace neuron`

```text
trace neuron <neuron-id> [page] [page-size]
        [id|contribution|abs-contribution] [asc|desc]
        [minimum-absolute-contribution] -- <value...>
```

Traces one neuron and returns a bounded page of its incoming connection contributions. The `--` delimiter separates optional query arguments from the input vector. Defaults are page `1`, page size `10`, connection-ID order, ascending direction, and no contribution filter. The maximum page size is `1000`.

```text
trace neuron 1003 1 25 abs-contribution desc 0.01 -- 1 1
```

Each record shows `source activation * weight = contribution`. The output also reports exact incoming count, filtered count, current page, and previous/next availability. Input neurons validly return zero contributions. Rejected dimensions, non-finite inputs, unknown neuron IDs, invalid page values, or invalid filters leave both the current network and caller-owned SDK result unchanged.

## Network inspection and execution

### `summary`

```text
summary
```

Shows layer order, layer name, neuron count, and total connection count. It is the fastest way to verify the shape of the current network.

### `inspect`

```text
inspect
```

Prints every layer, neuron ID, neuron bias, current activation, connection ID, endpoints, and weight.

Activations are runtime state. Before a forward pass they contain their initial or manually assigned values; after a forward pass they describe the most recent execution.

### `inspect neuron`

```text
inspect neuron <neuron-id> [maximum-connections]
```

Inspects one neuron without printing the complete network. The result includes its ID, layer order and name, layer activation function, current activation, bias, and its incoming and outgoing connections.

`maximum-connections` limits each direction independently and defaults to `10`. The command always reports the exact incoming and outgoing totals, even when only a bounded subset is printed. This makes the command suitable for highly connected neurons without hiding their true degree.

Example:

```text
inspect neuron 1003 5
```

The operation is read-only. An unknown neuron ID or a non-positive limit is rejected without changing the network.

### `inspect connection`

```text
inspect connection <connection-id>
```

Inspects one connection and both endpoint neurons. In addition to the connection ID, endpoints, and weight, the result identifies the layer, activation function, current activation, and bias of the source and target neurons.

Example:

```text
inspect connection 1
```

This focused relationship view is the textual counterpart of selecting an element in MiaIA Studio. It uses the same SDK inspection contract and never mutates the model.

### `inspect relationships`

```text
inspect relationships <neuron-id> <incoming|outgoing>
        [page] [page-size] [id|weight|abs-weight]
        [asc|desc] [minimum-absolute-weight]
```

Traverses one relationship direction without requesting or printing the complete network. Page numbering starts at `1`; the default page size is `10` and the maximum accepted page size is `1000`. Results can be ordered by stable connection ID, signed weight, or absolute weight, in ascending or descending order. The optional non-negative weight threshold filters by absolute magnitude.

```text
inspect relationships 1003 incoming 1 25 abs-weight desc 0.01
```

The result reports both the exact unfiltered direction total and the count remaining after filtering, followed by page position and previous/next availability. An offset beyond the filtered result is a valid empty page at the SDK level; interactive clients clamp navigation to an existing page.

### `forward`

```text
forward
```

Runs forward propagation on the current network.

For a neuron `j`, the engine computes:

```text
z_j = bias_j + sum(activation_i * weight_ij)
activation_j = activation_function(z_j)
```

Layers are evaluated in ascending order. The supported activation functions are Sigmoid, ReLU, Tanh, and Linear.

The Console prints neurons from a layer named `Output`. Forward execution itself is based on layer order, but the current output display expects this conventional name. Networks created by `create` use it.

### `benchmark`

```text
benchmark
```

Runs 10,000 forward iterations and prints total and average elapsed time.

Use this command only with a valid current network. It is a lightweight local timing aid, not a rigorous benchmark framework: it does not provide warm-up control, statistical sampling, hardware normalization, or comparison isolation.

## MiaIA projects

### `project new`

```text
project new
```

Clears the current network, dataset, training session, debug transaction, breakpoints, and saved project path. The command is rejected while background training is running or phase debugging is active.

### `project open`

```text
project open <path.mai>
```

Transactionally opens a versioned MiaIA project. The current process state changes only after the complete archive and embedded ONNX model pass validation.

Examples:

```text
project open experiment.mai
project open "C:\MiaIA Projects\xor.mai"
```

If the recorded CSV source is unavailable, opening still restores the model, training configuration, and breakpoints. The command prints a warning and `project info` reports the dataset as unavailable.

### `project save`

```text
project save [path.mai]
```

Atomically saves the current project. The first save requires a path. Later saves can omit it and reuse the current project path.

```text
project save "C:\MiaIA Projects\xor.mai"
project save
```

The destination must use `.mai`. The current network must be representable by the supported ONNX subset. Dataset samples remain in their CSV file; the project stores their source reference and schema.

### `project info`

```text
project info
```

Prints the current path and format version, model availability, dataset source and status, training configuration, and breakpoint count.

The [MiaIA project format](../Project/Project.md) documents the precise version 1 contents, exclusions, and failure behavior.

## ONNX interchange

### `import onnx`

```text
import onnx <path>
```

Loads a supported ONNX model and replaces the current network only when the complete import succeeds.

Examples:

```text
import onnx model.onnx
import onnx "C:\Models\xor model.onnx"
```

The current importer targets dense feed-forward graphs represented by `Gemm` nodes with supported Sigmoid, Relu, or Tanh activation nodes. A plain `Gemm` output represents a Linear layer. ONNX contains many more operators; importing an unsupported graph returns a failure instead of approximating its meaning.

### `export onnx`

```text
export onnx <path>
```

Exports the current supported network to ONNX opset 18.

Example:

```text
export onnx "C:\Models\experiment.onnx"
```

ONNX stores the model graph and parameters. It does not preserve MiaIA breakpoints, dataset references, training configuration, annotations, training history, or visualization state. Use `.mai` for the implemented MiaIA project context and ONNX for model interchange.

## CSV datasets

### File layout

CSV import uses positional columns. Given `N` inputs and `M` outputs, every data row must contain exactly `N + M` finite numeric values:

```text
input_1,...,input_N,target_1,...,target_M
```

Example XOR dataset with two inputs and one target:

```csv
x1,x2,target
0,0,0
0,1,1
1,0,1
1,1,0
```

The header is descriptive only. Column roles come from the counts supplied to the command.

### `dataset import csv`

```text
dataset import csv <input-count> <output-count> [--no-header] <path>
```

Imports a numeric CSV dataset and replaces the current dataset only after successful validation.

Examples:

```text
dataset import csv 2 1 sample.txt
dataset import csv 2 1 "C:\Data\xor.csv"
dataset import csv 2 1 --no-header "C:\Data\xor-no-header.csv"
```

The default behavior treats the first row as a header. Place `--no-header` after the two counts when the first row is a sample.

The file extension is not significant; `sample.txt` works when its contents follow CSV syntax.

### `dataset summary`

```text
dataset summary
```

Shows the dataset name, source path, number of samples, input count, and target count.

### `dataset inspect`

```text
dataset inspect <index>
```

Prints the input and target vectors of one zero-based sample.

Example:

```text
dataset inspect 0
```

This operation does not change the network.

### `dataset apply`

```text
dataset apply <index>
```

Copies one sample's input vector into the current network input layer. The sample input count must match the network input neuron count.

This operation does not run forward propagation and does not use the target vector. Run `forward` to calculate the resulting activations.

### `dataset evaluate`

```text
dataset evaluate <index> mse
```

Applies the sample inputs, runs forward propagation, collects output activations as predictions, and compares them with the sample targets.

The current loss is mean squared error:

```text
error_i = prediction_i - target_i
MSE = sum(error_i^2) / output-count
```

The command prints:

- targets;
- predictions;
- signed errors using `prediction - target`;
- mean squared error.

The network must have matching input and output dimensions. Evaluation updates neuron activations because it performs a forward pass. It does not change connection weights or neuron biases.

To evaluate the complete dataset against one fixed model, use:

```text
dataset evaluate all mse
```

This form prints the mean loss, each sample loss, and each prediction vector. It evaluates a copy of the network, so public activations, weights, and biases remain unchanged. Every sample uses the same parameters; this makes the result suitable for comparing model quality before and after `train epoch`.

### `dataset gradients`

```text
dataset gradients <index> mse
```

Performs sample evaluation followed by backward propagation. It exposes the derivatives used to understand how each parameter influences the current loss.

For MSE:

```text
dLoss/dPrediction_i = 2 * (prediction_i - target_i) / output-count
```

Backward propagation applies the chain rule through each activation function. For a connection from neuron `i` to neuron `j`:

```text
dLoss/dWeight_ij = activation_i * dLoss/dPreActivation_j
dLoss/dBias_j = dLoss/dPreActivation_j
```

The Console prints three neuron quantities:

- `dLoss/dActivation`: sensitivity before applying the local activation derivative;
- `dLoss/dPreActivation`: sensitivity of the weighted sum, often called the neuron delta;
- `dLoss/dBias`: the bias gradient, equal to the pre-activation gradient for non-input neurons.

It also prints `dLoss/dWeight` for every connection.

This command does **not** train the network. Weights and biases remain unchanged. The separation is intentional: MiaIA treats gradients as inspectable debugging data before an optimizer is allowed to apply them.

### `dataset diagnose`

```text
dataset diagnose [sample-limit]
    [--inactive-magnitude <value>] [--inactive-ratio <value>]
    [--saturation-margin <value>] [--saturation-ratio <value>]
    [--vanishing-magnitude <value>] [--vanishing-ratio <value>]
    [--exploding-magnitude <value>] [--exploding-ratio <value>]
    [--max-items <count>]
```

Analyzes activation and gradient health over a fixed dataset without training or publishing temporary activations. The network and dataset dimensions must match. Samples are processed in their existing deterministic order; omit `sample-limit`, or use `0`, to inspect all samples.

The command aggregates every neuron's activation and gradient magnitude and every connection's weight-gradient magnitude. It then reports candidates in four categories:

- **inactive**: activation magnitude remains at or below the configured threshold for the required share of samples;
- **saturated**: a non-input Sigmoid or Tanh activation remains within the configured margin of its range boundary for the required share of samples;
- **vanishing gradient**: gradient magnitude remains at or below its threshold for the required share of samples;
- **exploding gradient**: gradient magnitude reaches or exceeds its threshold for the required share of samples.

ReLU and Linear neurons are not classified as saturated by this diagnostic. Input neurons can be reported as inactive input features, but they are not treated as trainable biases. Neuron gradients use `dL/dz` for non-input neurons and `dL/da` for input neurons. Connection diagnostics use `dL/dWeight`.

Defaults are deliberately conservative: activation magnitude `1e-6` over `95%` of samples, saturation margin `0.01` over `95%`, vanishing magnitude `1e-8` over `95%`, and exploding magnitude `100` over `5%`. `--max-items` bounds only the printed findings; it does not reduce the analysis. For example:

```text
dataset diagnose 500 --inactive-ratio 0.99 --max-items 40
dataset diagnose --vanishing-magnitude 1e-7 --exploding-magnitude 50
```

The result is evidence about the selected dataset and thresholds, not an automatic proof that a neuron is permanently dead. A different input distribution can produce different classifications.

## Process-local model checkpoints

Model checkpoints capture a complete validated network in memory. They support
before/after training inspection, safe experimentation, parameter comparison, and fast
rollback during one MiaIA process. They are independent from the current dataset and
are not yet persisted in `.mai` project archives.

```text
checkpoint create <name>
checkpoint list
checkpoint inspect <id>
checkpoint compare <first-id> <second-id> [maximum-items]
checkpoint restore <id>
checkpoint remove <id>
checkpoint clear
```

Names may contain spaces. Each checkpoint receives a stable, monotonically increasing
ID for the process lifetime. Comparison matches neurons and connections by stable ID,
reports `second - first`, and orders the displayed bias and weight changes by absolute
magnitude. A topology mismatch is reported instead of producing misleading deltas.

Restore is transactional: MiaIA validates a copy before replacing the current network.
Capture, restore, remove, and clear are rejected while a training session is running or
a phase-debug transaction is active. Listing, inspection, and comparison are read-only.

```text
checkpoint create before training
train session start 10 0.01 mse
train session run all
checkpoint create after training
checkpoint compare 1 2 20
checkpoint restore 1
```

## Training

### `train step`

```text
train step <sample-index> <learning-rate> mse
```

Executes one atomic stochastic gradient descent step on the selected sample.

Example:

```text
train step 0 0.01 mse
```

The operation performs:

```text
sample inputs
    -> forward propagation
    -> MSE before update
    -> backward propagation
    -> SGD parameter update
    -> forward propagation again
    -> MSE after update
```

SGD applies:

```text
updated parameter = previous parameter - learning rate * gradient
```

The Console prints the loss before and after the step and, for every trainable weight and bias, the previous value, gradient, applied delta, and updated value. Input-layer biases are not updated.

The step is transactional. MiaIA performs it on a candidate network and publishes the candidate only after every calculation and the final evaluation succeed. A zero, negative, non-finite, or numerically unsafe learning rate fails without changing network parameters.

A suitable learning rate often reduces loss for the selected sample, but reduction is not guaranteed for every positive value. This command performs one step only; it does not start an automatic loop.

### `train debug`

```text
train debug start <sample-index> <learning-rate> mse
train debug status
train debug next
train debug neuron <neuron-id>
train debug connection <connection-id>
train debug cancel
```

Starts one SGD step as an inspectable mathematical transaction. `start` copies the public network into a private candidate and stops at `BeforeForward`. Every `next` advances exactly one phase:

1. `ForwardComplete`: inputs were applied and predictions, errors, activations, and loss are available;
2. `BackwardComplete`: neuron, bias, and weight gradients are available;
3. `UpdateComplete`: SGD deltas were applied only to the candidate network;
4. `Verified`: a second forward pass measured the candidate after the update;
5. `Committed`: the verified candidate replaced the public network atomically.

`status` returns the current phase without advancing it. Its snapshot includes the candidate network and every result produced up to that phase, making the same data suitable for Console output, Blueprint nodes, or a graphical signal/gradient visualization.

Focused inspection avoids scanning or printing the complete network:

```text
train debug neuron <neuron-id>
train debug connection <connection-id>
```

Neuron inspection reports its layer, public and candidate activation, public and candidate bias, then the gradients and bias update when the current phase has produced them. Connection inspection reports endpoints, public and candidate weight, weight gradient, and SGD update. Explicit availability flags in the SDK snapshots distinguish data that has not been calculated yet from a valid gradient or delta equal to zero. Input neurons expose gradients but correctly report that their biases are not trainable.

Both commands are read-only: they do not advance the phase or alter either network. They remain available after `Committed` for inspecting the completed transaction and reject requests while the debugger is `Idle` or when an ID does not exist.

The public network remains unchanged through `Verified`. While a debug transaction is active, operations that could mutate the public network or dataset are rejected so that the final commit cannot overwrite unrelated edits. `cancel` discards the candidate and returns the debugger to `Idle`; it is a real rollback because no candidate parameter has yet been published.

The ordinary `train step` command uses the same phased controller internally and runs it through `Committed` in one call. This keeps both execution modes mathematically identical.

### `train epoch`

```text
train epoch <learning-rate> mse
```

Executes one ordered pass over every sample in the current dataset. Each sample performs the same observable SGD operation as `train step`, and the updated candidate model is passed to the following sample.

Example:

```text
train epoch 0.01 mse
```

The Console prints the number of processed samples, the learning rate, the mean loss before and after each sample update, and the before/after loss for every sample. These means summarize the sequential updates: they are not full-dataset loss measurements taken once before and once after the epoch.

The entire epoch is transactional. MiaIA publishes the trained candidate only if every sample succeeds. An invalid sample, incompatible dimension, unsafe update, or non-finite calculation leaves the original network unchanged. The current implementation uses CSV order and does not shuffle samples.

### `train breakpoint`

```text
train breakpoint add phase <before|forward|backward|update|verify|commit>
train breakpoint add activation-above <neuron-id> <threshold>
train breakpoint add activation-below <neuron-id> <threshold>
train breakpoint add gradient-above <neuron-id> <magnitude>
train breakpoint add weight-update-above <connection-id> <magnitude>
train breakpoint list
train breakpoint enable <id> <on|off>
train breakpoint remove <id>
train breakpoint clear
```

Breakpoints belong to the controlled training state and remain configured when a new session starts. Starting a session resets their hit counters and previous trigger information without changing their IDs or enabled state.

Value breakpoints are evaluated against the complete atomic sample result:

- activation conditions read the selected neuron's committed activation;
- gradient conditions compare the absolute bias-gradient magnitude;
- weight-update conditions compare the absolute connection delta.

When an enabled condition matches during `run` or background `resume`, the successful sample remains committed and the session returns to `Active` before the following sample. The worker stop reason becomes `Breakpoint hit`, while the session snapshot records the breakpoint ID, phase, target, observed value, threshold, sample, and step. This is a safe debugger boundary: MiaIA never exposes a partly applied optimizer update.

A `phase commit` breakpoint can stop ordinary automatic training because every atomic step reaches commit. The other phase names are evaluated during `train debug next`, where the intermediate mathematical phases are observable. Activation, gradient, and update conditions are also evaluated as soon as their required debug data exists.

Examples:

```text
train breakpoint add phase backward
train breakpoint add activation-below 1003 0.01
train breakpoint add gradient-above 1003 5.0
train breakpoint add weight-update-above 1 0.25
train breakpoint list
```

Breakpoint editing is rejected while background training or a phase transaction is active. Pause or cancel the active operation first.

### `train session start`

```text
train session start <epochs> <learning-rate> mse
```

Creates a controlled multi-epoch session. The dataset must be non-empty, the network dimensions must match it, the epoch count and learning rate must be positive, and no other session may be Active or Running. Starting does not train a sample: the session waits Active before its first step.

### `train session status`

```text
train session status
```

Prints the session state, completed and configured epochs, completed and total steps, and—while Active or Running—the current epoch and next sample index. It also reports why the last background worker stopped. Status inspection never changes the network or session.

### `train session next`

```text
train session next
```

Executes exactly one atomic SGD step at the current session position, prints its before/after loss, appends the complete `TrainingStepSnapshot` to session history, and advances the sample cursor. Reaching the dataset end advances the epoch; reaching the configured epoch count marks the session Completed.

The command is the debugger-style pause boundary: nothing trains between two `next` commands. Compatible parameter edits and inspections may be performed between steps. If the current dataset size or network topology is no longer compatible, the operation fails without advancing the cursor or changing the network.

### `train session debug`

```text
train session debug
```

Opens the session's next sample as a phase-by-phase training transaction. The sample index, learning rate, loss, and optimizer come directly from the Active session, so they cannot drift from its configured schedule.

Continue with the regular debug commands:

```text
train debug next
train debug neuron <neuron-id>
train debug connection <connection-id>
train debug status
train debug cancel
```

Until the transition from `Verified` to `Committed`, the public network, session cursor, progress, and history remain unchanged. Commit publishes the candidate, appends exactly one `TrainingStepSnapshot`, and advances the session cursor using the same recording operation as `train session next`. If the step completes the final sample, the epoch advances; if it completes the final epoch, the session becomes Completed.

Cancelling discards the candidate and leaves the session on the same sample. While the transaction is active, session `next`, `run`, `resume`, and `cancel` are rejected. This prevents the same scheduled sample from being executed twice or by the background worker while it is open in the mathematical debugger.

### `train session history`

```text
train session history
```

Lists every completed step retained by the current session. Each row contains the global step index, one-based epoch, dataset sample index, loss before and after the update, and the number of changed weights and biases. The command is read-only and remains available while background training is Running.

### `train session inspect`

```text
train session inspect <step-index>
```

Retrieves one completed step by its global history index. It prints targets, predictions, signed errors, loss before and after, neuron gradients, and every weight and bias update with previous value, gradient, delta, and updated value. An invalid index leaves the caller result unchanged.

### `train session compare`

```text
train session compare <first-step-index> <second-step-index> [maximum-items]
```

Compares two completed steps retained by the current session without changing the network, session cursor, or history. The result reports loss and output-prediction differences, then ranks neuron gradients, connection gradients, final weights, and final biases by absolute change. Every scalar uses `second - first` as its signed delta and also reports the absolute delta.

The optional positive item limit defaults to `10` and bounds each ranked section independently, so large networks do not flood the Console. The complete comparison remains available through the SDK. Comparing two steps from different dataset samples is allowed, but the command marks loss and prediction deltas as contextual because their inputs and targets differ. Invalid indexes and a zero limit are rejected without changing caller state.

Individual hidden-neuron activations are not part of this first comparison contract because completed session history does not currently retain a full network snapshot for every step. Output predictions, gradients, and parameter endpoints are compared from the data already retained by `TrainingStepSnapshot`.

### `train session run`

```text
train session run <steps>
train session run all
```

Runs repeated session steps synchronously. A numeric limit executes at most that many steps; `all` requests every remaining step. Execution stops early when the session completes or a step fails.

The run summary reports requested and executed steps, start and end cursors, mean loss immediately before and after its successful updates, and one of these stop reasons:

- `Step limit reached`: the requested block completed and the session remains Active;
- `Breakpoint hit`: a successful step matched an enabled condition and the session paused safely;
- `Session completed`: the configured final epoch completed;
- `Step failed`: the next atomic step was rejected.

A run is progressive rather than transactional as a whole. Every individual step remains atomic, but earlier successful steps remain applied if a later step fails. The cursor stays on the failed sample, making it possible to inspect the network, modify compatible parameters, retry with `next` or another run, or cancel the session.

`run <steps>` is the preferred integration surface for responsive graphical clients: an editor can request small blocks and redraw between calls. This command remains synchronous; it does not create a background worker.

### `train session resume`

```text
train session resume
```

Changes an Active session to Running and starts a background worker. The command returns immediately, allowing the Console or another client to request status and inspection snapshots while training continues.

Only coherent state is observable: SDK inspection waits if an atomic sample step is currently publishing its result. Network, dataset, input, inference, and other mutating operations are rejected while Running. Pause the session before intervening in parameters or topology.

### `train session pause`

```text
train session pause
```

Requests cooperative pause and waits for the current sample step, if any, to finish. It never interrupts forward propagation, backward propagation, or an optimizer update halfway through. When the command returns successfully, the worker has been joined and the session is Active at the next safe sample boundary.

If the worker stops because a breakpoint matches, the session returns to Active and reports `Breakpoint hit`; the triggering step has already committed and the cursor identifies the following sample. If a step fails, the session reports `Step failed`, and that failed step does not advance the cursor or change the network.

### `train session cancel`

```text
train session cancel
```

Stops and joins a Running worker when necessary, then marks the session Cancelled. Successful steps already changed the public network and are not rolled back. A Completed, Cancelled, or Idle session cannot advance or be cancelled again, but a new session may be started.

### `dataset clear`

```text
dataset clear
```

Clears the current dataset. It does not clear or modify the current network. The operation is rejected while a background training worker is Running.

## Complete analysis workflow

```text
create 2 2 1 1
summary
predict 1 1
dataset import csv 2 1 sample.txt
dataset summary
dataset inspect 0
dataset apply 0
inspect
forward
inspect
dataset evaluate 0 mse
dataset evaluate all mse
train session start 2 0.01 mse
train session next
train session debug
train debug next
train debug neuron 1003
train debug cancel
train session status
train session run 1
train session history
train session inspect 0
train session compare 0 2
train session cancel
train session start 100 0.01 mse
train session resume
train session status
train session pause
train session cancel
dataset gradients 0 mse
dataset diagnose --max-items 20
train debug start 0 0.01 mse
train debug next
train debug next
train debug next
train debug next
train debug next
train step 0 0.01 mse
train epoch 0.01 mse
dataset evaluate all mse
```

This sequence demonstrates the difference between stages:

1. `dataset inspect` reads a sample;
2. `dataset apply` changes only input activations;
3. `forward` changes downstream activations;
4. `dataset evaluate 0` performs apply plus forward and calculates one sample loss;
5. `dataset evaluate all` measures the entire dataset without changing public network state;
6. `dataset gradients` performs evaluation plus backward differentiation;
7. `dataset diagnose` aggregates activation and gradient evidence across the fixed dataset;
8. `train debug` exposes the forward, backward, update, verification, and commit phases of one candidate step;
9. `train step` runs those same phases atomically for one selected sample;
10. `train epoch` performs the same update once for every sample in dataset order;
11. the final `dataset evaluate all` measures the trained fixed model for comparison;
12. `train session start` creates a paused multi-epoch schedule;
13. `train session next` performs one inspectable update;
14. `train session status` reports the unchanged cursor;
15. `train session run 1` executes one additional synchronous step;
16. `train session history` lists the retained updates;
17. `train session inspect 0` opens one complete mathematical step;
18. `train session compare 0 2` compares two retained mathematical steps;
19. `train session cancel` stops future steps without reverting completed updates;
20. `train session resume` starts non-blocking background execution;
21. `train session pause` joins the worker at the next atomic step boundary.

## Common failures

### Evaluation or gradients fail after dataset import

Importing a dataset does not create a network. Create or import a network with matching dimensions:

```text
create 2 2 1 1
dataset import csv 2 1 sample.txt
dataset evaluate 0 mse
```

### Input values do not match

The number of values supplied to `input` or stored in the selected dataset sample differs from the input layer size.

### Output dimensions do not match

The dataset target count differs from the output neuron count. For example, a dataset imported with one output requires a network with one output neuron.

### ONNX import fails

The path may be invalid, the file may not be a valid ONNX model, or the graph may use operators or shapes outside the currently supported dense subset.

### CSV import fails

Check the path, header option, column counts, row widths, and numeric values. `nan` and infinity are rejected.

## Current limitations

- SGD is the only optimizer and background execution uses one cooperative worker;
- state is not persisted as a MiaIA workspace;
- MSE is the only loss;
- dataset preprocessing and categorical values are not supported;
- detailed structured error diagnostics are not yet exposed;
- commands operate on one process-local network and dataset.

See the [Roadmap](../Roadmap/Roadmap.md) for the planned training and debugging workflow.
