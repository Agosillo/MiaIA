# MiaIA Studio for Unreal

## Current scope

The Unreal project is a client of the same `MiaIAClient` facade used by the Console. The first Blueprint integration exposes a complete vertical slice from model and dataset setup to phase-by-phase inspection. Unreal does not own or duplicate Engine mathematics.

The current integration includes a runtime Blueprint function library, the shared CLI command processor, and the first custom Unreal editor panel. **MiaIA Studio** is the product name of the graphical IDE, while this project is its Unreal client implementation. The panel is an initial functional shell, not the final visualization or interaction design.

The complete Unreal project lives under `MiaIA/IDE/Unreal`. Future graphical IDE implementations can be added beside it, for example under `MiaIA/IDE/Unity`, without changing Core, Engine, SDK, CLI, or the native Console.

## Blueprint types

The IDE module converts SDK snapshots into Unreal-reflected types:

- `EMiaIATrainingDebugPhase`;
- `EMiaIATrainingSessionStatus`;
- `EMiaIAActivationType`;
- `FMiaIANetworkSnapshot` with reflected layer, neuron, and connection arrays;
- `FMiaIATrainingDebugSnapshot`;
- `FMiaIATrainingSessionSnapshot`;
- `FMiaIATrainingDebugNeuron`;
- `FMiaIATrainingDebugConnection`.

No STL type crosses the Unreal reflection boundary. IDs and indexes use signed 64-bit Blueprint values and are validated before conversion to SDK types. Phase-dependent values include explicit availability flags.

## Blueprint nodes

The `MiaIA` Blueprint categories currently provide:

```text
Create Dense Network
Get Network Snapshot
Import Csv Dataset
Start Training Session
Get Training Session
Resume Training Session
Pause Training Session
Start Session Debug
Advance Debug Phase
Cancel Debug
Get Debug Status
Get Debug Network Snapshot
Get Debug Neuron
Get Debug Connection
Execute Command
```

The training nodes currently select MSE and SGD internally because those are the only implemented loss and optimizer choices. Future enum pins should be added when the Engine supports more than one valid choice.

## Minimal Blueprint workflow

```text
Create Dense Network
    -> Import Csv Dataset
    -> Start Training Session
    -> Start Session Debug
    -> Advance Debug Phase
    -> Get Debug Neuron or Get Debug Connection
```

Call `Advance Debug Phase` repeatedly to move through forward evaluation, backward differentiation, candidate update, verification, and commit. Use `Cancel Debug` before commit to discard the candidate without advancing session progress.

The focused neuron and connection structures expose public and candidate values, gradients, and updates. This is sufficient to drive a simple Blueprint widget or prototype visualization without scanning the complete native snapshot.

## Demonstration Blueprint

The editor module creates `BP_MiaIADemo` under `Content/MiaIA/Demo` and places one instance named `MiaIA Blueprint Demo` in `MiaIAMain`. Its visible Event Graph uses the public Blueprint nodes to create a small dense network, import the bundled `DemoData/and.csv` dataset, start a controlled training session, inspect one complete debug step phase by phase, and commit it. The CSV remains outside Unreal `Content` so the editor does not try to import it as a Data Table asset.

The previous `NeuronActor` level instance is removed to prevent two demonstrations from mutating the shared client state during the same `BeginPlay`. Its C++ source remains available until the Blueprint replacement has been verified.

The Blueprint asset is generated only after the initial Asset Registry scan and validator registration have completed. Opening `MiaIAMain` is sufficient to install the level instance when it was not the startup map.

## MiaIA Studio editor panel

Open the dockable panel from `Window > MiaIA Studio`. It reads the same shared `MiaIAClient` state used by the Console and Blueprint nodes; it does not create a separate model or duplicate Engine mathematics.

![MiaIA editor panel](Assets/miaia-editor-panel.png)

The captured foundation panel shows a network created directly from its embedded Console. Contextual command suggestions remain in the narrow left column, while the shared output history, persistent left-side scrollbar, input field, and `Send` action occupy the larger workspace. Suggestions can be accepted by mouse or `Tab`, and previous commands remain available through Up/Down navigation for the lifetime of the open panel.

### Opening and running the demonstration

1. Build the native solution in `Release | x64`, then build `IDEEditor | Win64 | Development`.
2. Open `MiaIA/IDE/Unreal/IDE.uproject` and load the `MiaIAMain` level.
3. Select `Window > MiaIA Studio` from the main Unreal Editor menu.
4. Dock the MiaIA tab or keep it as a separate window. Keeping it separate makes the state transitions easier to record or inspect.
5. Start Play in Editor from the main toolbar or press `Alt+P`.
6. Leave the panel controls untouched during the automatic demonstration. `BP_MiaIADemo` creates the network and dataset, starts a session, and advances one debug step automatically at short intervals.
7. Watch the session and debug state in the upper-right corner, the candidate activations in the topology, and the current values in the inspector.
8. Press `Esc` or the main Unreal `Stop` button to end Play in Editor.

To repeat the demonstration, stop Play in Editor completely and start it again. `BeginPlay` recreates the small network and starts a new controlled session. Starting a second phase advance from the panel while the Blueprint automation is running is intentionally discouraged because both actions would control the same shared debug state.

![MiaIA phase debug demonstration](Assets/miaia-debug-demo.gif)

The animation is slowed down for documentation. The demonstration itself advances more quickly so normal editor testing remains responsive.

### Panel layout

- **Model explorer** lists layers, neurons, and connections. Selecting an item here also selects it in the topology.
- **Network topology** renders the current layers, neurons, and weighted connections. Neuron color reflects activation strength; connection color and intensity reflect weight sign and magnitude.
- **Inspector** shows neuron activation and bias data or connection weight data, including phase-dependent gradients and candidate updates.
- **Session and debug status** report training progress and the currently inspected phase.
- **Console** is the first and initially selected lower tab. It uses a narrow command-suggestion column on the left and a larger output/input workspace on the right. It accepts the same commands as `Console.exe` and operates on the same process-local state displayed by the panel and used by Blueprint nodes.
- **Training timeline** follows the Console and summarizes the forward, backward, update, verification, and commit sequence.
- **Breakpoints** reserves the location of the future breakpoint authoring interface.

### Color theme

Use the `Theme` selector in the panel toolbar to choose `Follow Unreal`, `Dark`, or `Light`. `Follow Unreal` is the default and derives its semantic colors from the active Unreal Slate style. The explicit dark and light palettes remain stable independently of the surrounding editor theme.

The selection is stored in the local Unreal `EditorPerProjectUserSettings` configuration and restored when the panel is opened again. It is a per-user preference and does not modify tracked project configuration. The selected palette consistently controls panel surfaces, splitters, buttons, menus, inputs, scrollbars, text, neuron activation, positive and negative weights, selection, and debug-phase emphasis.

### Panel controls

- `Refresh` immediately reloads all visible snapshots. The panel also refreshes runtime values automatically.
- `Fit view` adjusts zoom and pan so every neuron in the current layout is visible without changing neuron positions.
- `Reset layout` discards manual neuron positions, restores the automatic layer layout, and fits the complete network.
- `Continue` resumes an active paused training session when no phase inspection owns the current step.
- `Pause` requests a safe pause for a running training session.
- `Start debug` attaches a new phase inspection to the next pending training sample. It can start from an idle debug state or after the previous step was committed.
- `Step phase` advances an active debug inspection by exactly one phase.
- `Cancel debug` discards the active candidate before commit and leaves the public network unchanged.

Buttons are enabled only when their operation is valid for the current session and debug state. During the automatic Blueprint demonstration, phase progression is controlled by `BP_MiaIADemo`; the panel buttons are intended for later manual and Console-driven workflows.

### Topology navigation and layout

The topology view supports direct 2D navigation and layout editing:

- use the mouse wheel to zoom around the pointer;
- drag with the middle mouse button to pan the view;
- drag a neuron with the left mouse button to assign a manual position;
- select `Fit view` to recover the complete topology after zooming, panning, or moving neurons;
- select `Reset layout` to remove every manual position and restore the original automatic arrangement.

Manual positions use normalized layout coordinates rather than Slate pixel coordinates. They remain stable while the panel is open and can later be reused by another renderer, including a 3D view. They are intentionally not written to ONNX or project configuration. Persistent visualization layouts belong to future MiaIA-specific model metadata.

### Interactive command console

The `Console` tab opens automatically at the bottom of the MiaIA panel. Enter a command in the text box, then press `Enter` or select `Send`. Both actions use the same execution path. The command, its output, and any diagnostic text are appended to the history. The model explorer, topology, inspector, session status, and controls refresh immediately afterward.

The output view automatically scrolls to the newest result after execution. A persistent external vertical scrollbar is positioned on its left edge and remains available for reviewing earlier output. The horizontal splitter between suggestions and the output workspace can be dragged when more room is needed for either side.

The left column displays at most eight contextual suggestions. Each row shows the complete syntax; hovering it shows the short description:

- start typing to filter the current command level;
- press `Tab` to accept the first suggestion;
- click any suggestion to accept that entry;
- press `Up` and `Down` to navigate commands already executed in the current panel session;
- press `Down` past the newest history entry to restore the unfinished text that existed before history navigation.

Completion advances one command level at a time. For example:

```text
tr                  -> train
train s             -> train step | train session
train session r     -> train session run | train session resume
dataset import      -> dataset import csv
```

For a command that is already receiving values, the suggestion becomes a syntax guide without deleting the values already entered. Accepting the guide after typing `create 2` therefore preserves `create 2` and appends a space for the next value.

The editor calls the same reusable command processor as `Console.exe`; it does not start an external executable. This is important because the current SDK state is process-local. A network created with `create` in the panel is immediately visible in the topology, while a separate `Console.exe` process would own a different network.

A minimal editor-driven workflow is:

```text
create 2 2 1 1
predict 1 1
dataset import csv 2 1 DemoData/and.csv
dataset summary
train session start 2 0.01 mse
train session debug
train debug next
train debug status
```

Paths may be absolute or relative to the Unreal project directory. `exit` is reported but deliberately does not close Unreal Editor. Commands currently execute synchronously on the editor UI thread, so long operations such as a large benchmark or a full synchronous training run temporarily block panel interaction. Prefer controlled session steps, phase debugging, or background `train session resume` for interactive work.

### Manual phase inspection

The automatic demonstration commits one step and leaves its four-sample training session active. This provides an immediate entry point for manual inspection:

1. Wait until the demonstration reports `Debug: Committed` and `Session: Active`.
2. Select `Start debug` to attach an inspection to the next pending sample.
3. Select a neuron or connection from the explorer or directly in the topology. A selected connection is drawn thicker in amber.
4. Select `Step phase` once for each transition: forward, backward, candidate update, verification, and commit.
5. Observe candidate activations on the graph. For a selected connection, compare public and candidate weights, then inspect its gradient, delta, and updated weight as those values become available.
6. Before commit, select `Cancel debug` to discard the candidate, or continue stepping to commit it and advance session progress.
7. After commit, select `Start debug` again to inspect the next pending sample.

Neuron color ranges from inactive gray to active green. Positive weights are blue, negative weights are red, and the selected connection is amber. Color intensity and line thickness communicate value strength; exact values remain available in the inspector.

The first panel increment provides:

- a layer and neuron explorer;
- a live two-dimensional topology view;
- activation-based neuron coloring;
- positive and negative connection coloring with weight strength;
- neuron and connection selection from either the explorer or topology;
- an inspector for activations, biases, weights, gradients, and candidate updates;
- training-session status and phase timeline;
- working refresh, continue, pause, and debug phase-step actions;
- an interactive command console shared with `Console.exe`.

The panel refreshes runtime values automatically while rebuilding its explorer only when the topology changes. Command history is currently memory-only and belongs to the open panel instance. Persistent history, weight editing, breakpoint authoring, asynchronous command dispatch, and the eventual three-dimensional model navigator remain outside this increment.

## Build order

The Unreal module currently links the native Release libraries directly. After changing Core, Engine, or SDK:

1. build the MiaIA native solution in `Release | x64`;
2. confirm `x64/Release/Engine.lib`, `SDK.lib`, and `CLI.lib` are current;
3. open or regenerate the Unreal solution from `MiaIA/IDE/Unreal/IDE.uproject`;
4. build `IDEEditor | Win64 | Development`;
5. open the project and locate the nodes under the `MiaIA` categories.

## Planned Unreal work

- Blueprint coverage for broader Console and SDK operations;
- activation, gradient, and weight-change color mapping;
- scalable rendering that does not create one ticking Actor per neuron;
- forward and backward flow animation;
- persistent command history and asynchronous long-running dispatch.
