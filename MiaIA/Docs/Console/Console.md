# MiaIA Console Guide

## Purpose

The MiaIA Console is an interactive SDK client. Every command translates user input into `MiaIAClient` calls, so the Console exercises the same public boundary intended for Unreal Engine and future clients.

The Console is currently process-local and stateful. A session contains one current network and one current dataset. Closing the process discards both unless the model was exported to ONNX. MiaIA workspace persistence is planned but not implemented.

## Notation

This guide uses:

- `<value>` for a required argument;
- `<value...>` for one or more required values;
- `[--option]` for an optional flag.

Sample indices are zero-based. Paths containing spaces should be enclosed in double quotes.

## Quick start

The following session creates a two-input network, imports a dataset, evaluates its first sample, and inspects the gradients that a future optimizer would consume:

```text
create 2 2 1 1
dataset import csv 2 1 sample.txt
dataset summary
dataset inspect 0
dataset evaluate 0 mse
dataset gradients 0 mse
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
```

Creates a new fully connected feed-forward network and replaces the current network.

Arguments:

- `inputs`: number of input neurons;
- `neurons-per-hidden-layer`: width shared by every hidden layer;
- `hidden-layers`: number of hidden layers; zero is allowed by the engine;
- `outputs`: number of output neurons.

Example:

```text
create 784 256 3 10
```

This describes the shape:

```text
784 -> 256 -> 256 -> 256 -> 10
```

The dense factory initializes connection weights to `0.1`, biases to `0.0`, and non-input layer activations to Sigmoid. The current Console command does not expose weight initialization or activation selection; those capabilities exist at the SDK level and will eventually receive richer client controls.

Calling `create` without arguments currently uses the Console defaults `10 32 2 3`. Supplying all four values explicitly is recommended because it makes the experiment reproducible and clear.

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

ONNX stores the model graph and parameters. It does not preserve the future MiaIA editor layout, breakpoints, annotations, training history, or visualization state.

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

### `dataset clear`

```text
dataset clear
```

Clears the current dataset. It does not clear or modify the current network.

## Complete analysis workflow

```text
create 2 2 1 1
summary
dataset import csv 2 1 sample.txt
dataset summary
dataset inspect 0
dataset apply 0
inspect
forward
inspect
dataset evaluate 0 mse
dataset gradients 0 mse
```

This sequence demonstrates the difference between stages:

1. `dataset inspect` reads a sample;
2. `dataset apply` changes only input activations;
3. `forward` changes downstream activations;
4. `dataset evaluate` performs apply plus forward and calculates loss;
5. `dataset gradients` performs evaluation plus backward differentiation;
6. none of these commands updates weights or biases.

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

- there is no optimizer or `train` command yet;
- state is not persisted as a MiaIA workspace;
- MSE is the only loss;
- dataset preprocessing and categorical values are not supported;
- detailed structured error diagnostics are not yet exposed;
- commands operate on one process-local network and dataset.

See the [Roadmap](../Roadmap/Roadmap.md) for the planned training and debugging workflow.
