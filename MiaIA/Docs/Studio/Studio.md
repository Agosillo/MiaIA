# MiaIA Studio

## Purpose

MiaIA Studio is the graphical application for creating, observing, training, and debugging neural networks. The same Slate experience is now hosted both by an Unreal Editor panel and by an Unreal game target intended to become the standalone Windows application. A future native frontend, such as Qt, may present the same application model without depending on Unreal.

The repository contains the platform-neutral application foundation in `IDE/StudioCore`, the shared Unreal runtime UI in `IDE/Unreal/Source/IDEStudio`, and the `MiaIAStudio` game target. The target is buildable and directly launchable for development; a distributable packaged build has not yet been verified.

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
- layer, neuron, and connection selection;
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

In the three-dimensional layout:

- layers progress along the Z axis;
- the neurons in each layer are distributed on an X/Y grid;
- all positions are normalized so each renderer can choose its own world scale.

Compact two-dimensional scenes place one node per layer along X. Compact three-dimensional scenes place one node per layer along Z. Aggregate links describe layer progression rather than pretending to be individual model connections.

## Unreal delivery path

The reusable Slate panel, topology view, and theme implementation live in the runtime-capable `IDEStudio` module. `IDEEditor` contains only editor integration such as dock-tab registration and installation of the Blueprint demonstration. The native `UMiaIAStudioGameInstance` places the same panel over the game viewport, so editor and standalone hosts do not maintain separate IDE implementations.

The `MiaIAStudio` game target is the development entry point for the independent host. The remaining delivery steps are:

1. verify the standalone target in Development configuration;
2. add a 2D/3D selector backed by `StudioCore`;
3. implement the first three-dimensional renderer;
4. package and verify a distributable Windows `MiaIAStudio.exe`.

The packaged executable will not require Unreal Editor to be installed. It will still contain the Unreal runtime. A future Qt frontend would instead render the same `StudioCore` state through Qt and its selected graphics backend.

## Development locations

- `IDE/StudioCore`: portable application and visualization logic;
- `IDE/Unreal/Source/IDEStudio`: reusable runtime Slate frontend and standalone host;
- `IDE/Unreal/Source/IDEEditor`: Unreal Editor-only tab and demonstration integration;
- `IDE/Qt`: reserved naming for a possible future native Qt frontend;
- `Docs/Studio`: frontend-independent Studio documentation.

`StudioCore` is part of `MiaIA.sln` and is built with the other C++20 static libraries. The Unreal build consumes the Release x64 `StudioCore.lib` alongside CLI, SDK, and Engine.
