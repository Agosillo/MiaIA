# MiaIA Unreal Integration

## Current scope

The Unreal project is a client of the same `MiaIAClient` facade used by the Console. The first Blueprint integration exposes a complete vertical slice from model and dataset setup to phase-by-phase inspection. Unreal does not own or duplicate Engine mathematics.

The current integration is a runtime Blueprint function library. It is not yet a custom Unreal editor plugin, graph renderer, or final MiaIA IDE panel.

## Blueprint types

The IDE module converts SDK snapshots into Unreal-reflected types:

- `EMiaIATrainingDebugPhase`;
- `EMiaIATrainingSessionStatus`;
- `FMiaIATrainingDebugSnapshot`;
- `FMiaIATrainingSessionSnapshot`;
- `FMiaIATrainingDebugNeuron`;
- `FMiaIATrainingDebugConnection`.

No STL type crosses the Unreal reflection boundary. IDs and indexes use signed 64-bit Blueprint values and are validated before conversion to SDK types. Phase-dependent values include explicit availability flags.

## Blueprint nodes

The `MiaIA` Blueprint categories currently provide:

```text
Create Dense Network
Import Csv Dataset
Start Training Session
Get Training Session
Start Session Debug
Advance Debug Phase
Cancel Debug
Get Debug Status
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

## Build order

The Unreal module currently links the native Release libraries directly. After changing Core, Engine, or SDK:

1. build the MiaIA native solution in `Release | x64`;
2. confirm `x64/Release/Engine.lib` and `SDK.lib` are current;
3. build `IDEEditor | Win64 | Development`;
4. open the project and locate the nodes under the `MiaIA` categories.

## Planned Unreal work

- a dedicated editor module and dockable MiaIA panel;
- Blueprint coverage for broader Console and SDK operations;
- model topology snapshots converted to Unreal arrays;
- layer, neuron, and connection selection;
- activation, gradient, and weight-change color mapping;
- scalable rendering that does not create one ticking Actor per neuron;
- forward and backward flow animation;
- IDE command console integration.
