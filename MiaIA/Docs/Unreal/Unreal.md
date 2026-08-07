# MiaIA Unreal Integration

## Current scope

The Unreal project is a client of the same `MiaIAClient` facade used by the Console. The first Blueprint integration exposes a complete vertical slice from model and dataset setup to phase-by-phase inspection. Unreal does not own or duplicate Engine mathematics.

The current integration includes a runtime Blueprint function library and the first custom Unreal editor panel. The panel is an initial functional shell for the MiaIA IDE, not the final visualization or interaction design.

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

## MiaIA editor panel

Open the dockable panel from `Window > MiaIA`. It reads the same shared `MiaIAClient` state used by the Console and Blueprint nodes; it does not create a separate model or duplicate Engine mathematics.

![MiaIA editor panel](Assets/miaia-editor-panel.png)

### Opening and running the demonstration

1. Build the native solution in `Release | x64`, then build `IDEEditor | Win64 | Development`.
2. Open `IDE.uproject` and load the `MiaIAMain` level.
3. Select `Window > MiaIA` from the main Unreal Editor menu.
4. Dock the MiaIA tab or keep it as a separate window. Keeping it separate makes the state transitions easier to record or inspect.
5. Start Play in Editor from the main toolbar or press `Alt+P`.
6. Leave the panel controls untouched during the automatic demonstration. `BP_MiaIADemo` creates the network and dataset, starts a session, and advances one debug step automatically at short intervals.
7. Watch the session and debug state in the upper-right corner, the candidate activations in the topology, and the current values in the inspector.
8. Press `Esc` or the main Unreal `Stop` button to end Play in Editor.

To repeat the demonstration, stop Play in Editor completely and start it again. `BeginPlay` recreates the small network and starts a new controlled session. Starting a second phase advance from the panel while the Blueprint automation is running is intentionally discouraged because both actions would control the same shared debug state.

![MiaIA phase debug demonstration](Assets/miaia-debug-demo.gif)

The animation is slowed down for documentation. The demonstration itself advances more quickly so normal editor testing remains responsive.

### Panel layout

- **Model explorer** lists layers and their neurons. Selecting a neuron here also selects it in the topology.
- **Network topology** renders the current layers, neurons, and weighted connections. Neuron color reflects activation strength; connection color and intensity reflect weight sign and magnitude.
- **Inspector** shows the selected neuron's layer, activation, bias, and phase-dependent gradient values.
- **Session and debug status** report training progress and the currently inspected phase.
- **Training timeline** summarizes the forward, backward, update, verification, and commit sequence.
- **Console** currently presents a read-only command-style view of the shared state.
- **Breakpoints** reserves the location of the future breakpoint authoring interface.

### Panel controls

- `Refresh` immediately reloads all visible snapshots. The panel also refreshes runtime values automatically.
- `Continue` resumes an active paused training session when no phase inspection owns the current step.
- `Pause` requests a safe pause for a running training session.
- `Step phase` advances an active debug inspection by exactly one phase.

Buttons are enabled only when their operation is valid for the current session and debug state. During the automatic Blueprint demonstration, phase progression is controlled by `BP_MiaIADemo`; the panel buttons are intended for later manual and Console-driven workflows.

The first panel increment provides:

- a layer and neuron explorer;
- a live two-dimensional topology view;
- activation-based neuron coloring;
- positive and negative connection coloring with weight strength;
- neuron selection from either the explorer or topology;
- an inspector for activation, bias, and available debug gradients;
- training-session status and phase timeline;
- working refresh, continue, pause, and debug phase-step actions;
- a read-only command-style status view.

The panel refreshes runtime values automatically while rebuilding its explorer only when the topology changes. Weight editing, breakpoint authoring, an interactive command console, and the eventual three-dimensional model navigator remain outside this increment.

## Build order

The Unreal module currently links the native Release libraries directly. After changing Core, Engine, or SDK:

1. build the MiaIA native solution in `Release | x64`;
2. confirm `x64/Release/Engine.lib` and `SDK.lib` are current;
3. build `IDEEditor | Win64 | Development`;
4. open the project and locate the nodes under the `MiaIA` categories.

## Planned Unreal work

- Blueprint coverage for broader Console and SDK operations;
- activation, gradient, and weight-change color mapping;
- scalable rendering that does not create one ticking Actor per neuron;
- forward and backward flow animation;
- IDE command console integration.
