# MiaIA Studio

## Purpose

MiaIA Studio is the graphical application for creating, observing, training, and debugging neural networks. The same Slate experience is hosted both by an Unreal Editor panel and by the packaged standalone Windows application. A future native frontend, such as Qt, may present the same application model without depending on Unreal.

The repository contains the platform-neutral application foundation in `IDE/StudioCore`, the shared Unreal runtime UI in `IDE/Unreal/Source/IDEStudio`, and the `MiaIAStudio` game target. The editor, game target, and archived Development package have been verified with the same runtime experience.

## Frontend architecture

```text
Core / Engine
      |
     SDK
      |
     CLI
      |
 StudioCore
      |-- Unreal Slate frontend
      |     |-- Unreal Editor panel
      |     `-- MiaIAStudio game target
      |
      `-- possible future native frontend
            `-- Qt or another desktop toolkit
```

`StudioCore` is ordinary C++20 and contains no Unreal, Slate, Qt, or renderer types. A frontend converts its value objects into native widgets, meshes, materials, or draw commands.

## Implemented StudioCore responsibilities

`StudioController` currently provides:

- execution through the shared CLI command processor;
- contextual command suggestions;
- one refresh boundary over `MiaIAClient`;
- two-dimensional and three-dimensional view selection;
- layer, neuron, connection, and multi-neuron selection;
- automatic invalidation of a selection that no longer exists;
- detailed snapshots for manageable networks;
- lightweight compact scenes for large networks.

`StudioTopologyBuilder` converts SDK snapshots into renderer-neutral scenes. A scene contains logical nodes, links, element identities, layer information, values, and normalized three-dimensional positions.

Detailed mode is used while a network has at most 2,000 neurons and 5,000 connections. Above either limit, compact mode creates one aggregate node per layer and does not require the complete graph snapshot. These limits are shared with the current Unreal panel.

## Layout conventions

In the two-dimensional layout:

- layers progress from left to right on the X axis;
- neurons are distributed vertically on the Y axis;
- Z is zero.

The initial three-dimensional layout deliberately preserves the same reading:

- layers progress from left to right;
- neurons form vertical columns;
- every automatic node starts on one shared plane;
- orbiting, camera-relative dragging, and manual group movement reveal and introduce depth without changing the model.

All automatic positions remain normalized so each renderer can choose its own world scale. Compact scenes place one node per layer and use aggregate links to describe layer progression rather than pretending to display individual model connections.

## Unreal delivery path

The reusable Slate panel, topology view, and theme implementation live in the runtime-capable `IDEStudio` module. `IDEEditor` contains only editor integration such as dock-tab registration and installation of the Blueprint demonstration. The native `UMiaIAStudioGameInstance` places the same panel over the game viewport, so editor and standalone hosts do not maintain separate IDE implementations.

The `MiaIAStudio` game target is the development entry point for the independent host. `IDE/Unreal/Build/Package-Windows.ps1` builds, cooks, stages, and archives that target for Win64. Its shared panel now exposes a `2D`/`3D` selector backed by `StudioCore`. Both views share single and multiple neuron selection, additive `Ctrl` selection, rectangular marquee selection, group dragging, and Inspector state. The 3D mode hosts a real runtime Unreal viewport with an initial 2D-equivalent front camera, one aggregated mesh of shaded neuron spheres and weighted connection cylinders, stable FXAA, the same gamma and semantic color composition as the Slate canvas, projected neuron labels, a perspective-correct circular selection outline, orbit, pan, extended zoom, and adaptive fit. A shared expanded-workspace mode temporarily gives the topology all space outside the retained right-side Inspector while preserving the same widget and state, so the editor tab and packaged application present the same visualization behavior.

The toolbar `Help` menu provides an embedded interaction reference and an `About MiaIA Studio` dialog. Both use the same themed in-panel Slate overlay in the editor and packaged application instead of relying on host-specific operating-system message boxes. The About text reads the project version from Unreal configuration and identifies the shared Engine, SDK, and CLI service boundary instead of presenting the Unreal frontend as the complete MiaIA architecture.

Quick Help also identifies the public source location and license boundary. About displays the MPL-covered source location and the Unreal Engine attribution required for the packaged frontend. Public archives are created through `Build/Package-Windows.ps1`, which adds the MiaIA license, end-user terms, trademark policy, third-party notice index, and collected dependency licenses beside the top-level launcher after Unreal packaging succeeds.

The Unreal renderer batches neuron markers and weighted links in one runtime component instead of creating one Actor per model element. Detailed mode still retains every element ID for selection and Inspector queries. Compact mode remains a separate scalability policy that summarizes each layer when the full graph exceeds the shared limits.

The packaged executable does not require Unreal Editor to run. It still contains the Unreal runtime. A future Qt frontend would instead render the same `StudioCore` state through Qt and its selected graphics backend.

## Development locations

- `IDE/StudioCore`: portable application and visualization logic;
- `IDE/Unreal/Source/IDEStudio`: reusable runtime Slate frontend and standalone host;
- `IDE/Unreal/Source/IDEEditor`: Unreal Editor-only tab and demonstration integration;
- `IDE/Qt`: reserved naming for a possible future native Qt frontend;
- `Docs/Studio`: frontend-independent Studio documentation.

`StudioCore` is part of `MiaIA.sln` and is built with the other C++20 static libraries. The Unreal build consumes the Release x64 `StudioCore.lib` alongside CLI, SDK, and Engine.

## Windows packaging

From the Unreal project directory, create the default Development package with:

```powershell
& .\Build\Package-Windows.ps1
```

Use `-Configuration Shipping` for a Shipping package or `-EngineRoot` when Unreal Engine is installed somewhere other than `D:\Epic Games\UE_5.8`. By default, the archive is written below `MiaIA/Artifacts/MiaIAStudio/Windows-<Configuration>`, which is ignored by Git.

The script does not launch or delete anything. It reports the final `MiaIAStudio.exe` path after Unreal Automation Tool completes successfully. The complete archived directory is the distributable unit; copying only the executable omits required Unreal runtime and cooked-content files.

The standalone application starts in a 1600 x 900 resizable window. A saved legacy fullscreen preference is converted to windowed mode at startup. Rendering is limited to 60 FPS with VSync, the covered game world is disabled, and the embedded 3D scene redraws only when its data or view changes. The `Data refresh` preference controls model polling independently: `Adaptive` uses 4 Hz while training is running and 1 Hz while idle or paused, while fixed 1, 2, 4, and 10 Hz modes are available. Commands and explicit debug controls always refresh immediately. The standard window close action, `Alt+F4`, and the Studio toolbar `Exit` button all close the application; the editor-hosted panel does not display `Exit`.

The packaged application currently has a provisional MiaIA Studio icon and startup splash derived from the initial brand board. Their source and normalized PNG files live under `IDE/Unreal/Build/Brand`; Unreal consumes the stable Windows icon and splash paths documented in the [Unreal integration guide](../Unreal/Unreal.md#provisional-application-branding).
