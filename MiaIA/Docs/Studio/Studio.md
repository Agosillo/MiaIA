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
- layer, neuron, connection, and multi-neuron selection, with arrow-key traversal between neurons and layers;
- focused neuron and connection relationship state with paged, filtered, and ordered traversal;
- immutable forward-trace state with one focused neuron and bounded incoming-contribution pages;
- immutable dataset signal-health state with shared thresholds and category filtering;
- controlled-training timeline state with lightweight committed-step summaries and one on-demand selected-step detail;
- automatic invalidation of a selection that no longer exists;
- detailed snapshots for manageable networks;
- lightweight compact scenes for large networks.

`StudioTopologyBuilder` converts SDK snapshots into renderer-neutral scenes. A scene contains logical nodes, links, element identities, layer information, values, and renderer-independent layout coordinates.

Every non-empty network first opens on one whole-network preview, independently of the detailed-topology limits. Its central node uses lateral parallel indicators for the input and output dimensions. Their fixed spacing starts around the center and expands symmetrically with the visible count instead of compressing them into fixed bounds; fit recalculates the complete visible extent. All numeric counts remain in the header, Explorer, and Inspector so the glyph itself stays free of labels. Clicking the node or pressing `Enter` restores the pre-preview visualization policy: networks with at most 2,000 neurons and 5,000 connections open their complete detailed topology, while networks above either limit open the linear layer-aggregate overview without requesting the complete graph snapshot. The aggregate axis follows the selected Horizontal or Vertical orientation and Forward or Reverse direction in both renderers, and its 3D extent grows with the number of layers so aggregate spheres preserve a non-overlapping minimum gap before the camera fits the complete sequence. A layer aggregate can then be selected for its summary and opened by double-click or `Enter`; the host requests only that layer through `MiaIAClient::TryGetLayer`. `Back` or `Esc` walks backward through the available levels and returns the detailed topology or layer aggregates to the same whole-network preview. `StudioTopologyBuilder` accepts explicit limits while retaining those defaults, allowing each host to expose a persistent user preference without embedding renderer policy in the Engine.

For a single selected neuron, the Inspector retains its layer context and exact incoming and outgoing connection counts. Its Relationship Explorer queries one bounded page at a time, independently selects Incoming or Outgoing direction, orders by connection ID, signed weight, or absolute weight, and filters by a minimum absolute weight. Each row can select the connection or navigate to the neuron at its opposite endpoint. In a compact large network that navigation atomically opens the endpoint layer instead of requesting the complete graph. A selected connection retains contextual snapshots of both endpoint neurons and exposes explicit navigation to its source or destination, so inspecting an edge never interrupts relationship traversal. The persistent per-direction limit is reused as page size, changing the amount displayed without changing reported totals.

Each visible neuron row in the detailed Model Explorer also exposes `Focus`. It selects that neuron and asks only the active 2D or 3D renderer to reveal it. An already visible neuron does not move the view; an off-screen neuron causes the minimum 2D pan or the corresponding 3D camera retarget while retaining the current zoom and orientation policy.

The Unreal-hosted Inspector also provides focused parameter controls. One selected hidden or output neuron can apply a finite bias, and one selected connection can apply a finite weight. Input neurons display an explanatory non-editable state. These controls call the public Blueprint adapter, which forwards to `MiaIAClient`; training/debug ownership and all mathematical validation remain below the frontend.

The lower `Execution trace` tab accepts one value per input neuron and captures the existing Engine forward explanation without changing public activations. It reports the captured inputs and outputs, colors detailed neurons from the recorded activations, dims unrelated links, and colors the current bounded page of incoming connections by their exact signed contribution. Its renderer-neutral player starts paused on the input frame, then advances through incoming signal and activation frames for every following layer. `Previous frame`, `Play`/`Pause`, `Next frame`, `Reset`, and speeds from `0.25x` through `4x` provide deterministic automatic or manual inspection in both 2D and 3D. Completion restores the complete static trace overlay. Selecting a neuron adds weighted input, bias, pre-activation, and final activation to the Inspector and refreshes the contribution page. The lower Previous and Next controls navigate dense contribution pages, while selecting a contribution row opens the ordinary connection Inspector. `Clear` removes only the graphical overlay and retained trace state.

The adjacent `Gradient trace` tab accepts separate input and target vectors and captures one immutable mean-squared-error backward pass. Its renderer-neutral player starts at output gradients and alternates connection-flow and source-layer-gradient frames until it reaches the inputs. Positive and negative palette colors encode gradient sign, while normalized intensity encodes magnitude. Selecting a neuron shows activation, `dL/da`, `dL/dz`, and bias gradient; selecting a connection shows its weight gradient and signed source-gradient contribution. The 2D and 3D renderers consume the same snapshot and frame state. Running a Forward trace clears an active Gradient trace and vice versa, preventing Inspector or overlay values from different executions from being combined. Clear and playback operations only change retained presentation state and never update model parameters.

The `Diagnostics` tab extends that single-sample view across the loaded dataset. `Analyze dataset` captures one immutable signal-health snapshot using the configured sample limit, activation and saturation thresholds, and vanishing/exploding gradient thresholds. `0` means all samples. The summary separates healthy and flagged neuron and connection counts; the category menu filters inactive, saturated, vanishing-gradient, or exploding-gradient findings without rerunning the analysis. Findings are selectable and synchronize the topology and Inspector. Both renderers use palette-owned diagnostic colors, while healthy elements are subdued to keep the selected category legible. Custom, High Contrast, Color-blind Safe, and Monochrome palettes therefore remain consistent with the ordinary network and trace views. Starting a forward or backward trace clears an active dataset diagnostic, and starting diagnostics clears both traces, so values from different captured executions are never mixed. Clearing diagnostics changes only retained Studio presentation state.

The `Training timeline` tab presents the current controlled-session status, epoch and sample cursor, completed and total steps, learning rate, loss, optimizer, active phase-debug boundary, and latest breakpoint stop. Its six-phase strip follows Before, Forward, Backward, Update, Verify, and Commit; completed phases use the debug color and a yellow upward cursor identifies the exact current boundary. The committed-step history is newest first and uses the Engine-owned history summaries, so every row reports stable step, epoch and sample identities, loss before and after, and update counts without duplicating training calculations. The frontend bounds its widget list to the newest 200 entries while leaving the native retained history unchanged. Selecting a visible row requests only that retained `TrainingStepSnapshot` and displays its targets, predictions, errors, loss delta, learning rate, optimizer, weight updates, and bias updates. `Clear view` hides the currently displayed rows and selected detail without deleting native history; later committed steps appear normally, and a replaced session resets the view filter. The selected step survives ordinary refreshes while it remains in the current session and is cleared automatically when the session history is replaced.

The integrated Console keeps command output separate from command history and MiaIA state. `Clear output` restores the short Console introduction and clears only the displayed transcript; commands remain available through Up and Down, and the active network, dataset, project, training session, debug state, and input draft are not changed.

In the Unreal host, the panel addresses this state through an opaque `FMiaIAInstanceHandle`. `FMiaIAInstanceService` owns the corresponding controller in the runtime `IDE` module and performs every stateful call there, including forward- and backward-trace playback and training-timeline selection, so Blueprint, the integrated CLI, and `IDEStudio` observe one SDK state even though Unreal builds them across multiple DLL modules. Only the `default` instance exists today. The handle is the frontend seam for future independent project contexts; it is not yet a claim that the process-global SDK can host several networks simultaneously.

## Model checkpoints

The `Checkpoints` tab captures and compares complete model states without leaving
MiaIA Studio. Enter a name and choose `Capture`, select an item, and use `Set A` / `Set
B` followed by `Compare A / B` for stable-ID parameter deltas. The detail panel reports
topology compatibility, changed parameter counts, and the ten largest bias and weight
changes.

`Restore selected` uses the transactional SDK restore and clears stale execution,
gradient, and signal-health views before refreshing the topology. `Remove selected` and
`Clear all` affect only the in-memory registry. Checkpoints do not survive process exit
and are not stored in `.mai` version 1.

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

Detailed automatic positions are measured in neuron diameters. `Expanded` spacing keeps the topology readable, while `Packed` spacing can place symmetric neurons and layers directly adjacent without intersecting them. `Horizontal` flow places layers from left to right and neurons from top to bottom; `Vertical` flow places layers from top to bottom and neurons from left to right. Clicking the already-active orientation toggles Forward and Reverse layer direction, producing the corresponding left/right or top/bottom mirror while preserving neuron order within each layer. Uniform node size determines the minimum safe spacing; additional neuron and layer gaps, camera zoom, and connection visibility remain separate controls. This lets 2D and 3D choose different pixel or world scales while preserving the same symmetry, orientation, direction, and non-overlap rule. Compact scenes apply the same flow direction, place one node per layer, and use aggregate links to describe layer progression rather than pretending to display individual model connections.

## Unreal delivery path

The reusable Slate panel, topology view, and theme implementation live in the runtime-capable `IDEStudio` module. `IDEEditor` contains only editor integration such as dock-tab registration and installation of the Blueprint demonstration. The native `UMiaIAStudioGameInstance` places the same panel over the game viewport, so editor and standalone hosts do not maintain separate IDE implementations.

The shared toolbar uses two stable functional rows rather than overflowing one horizontal strip. The first row begins with a `Project` menu for New, Open, Save, Save as, Import ONNX, Export ONNX, and Info. `.mai` operations preserve project context, while ONNX operations exchange only the supported model portion. The `Layout` menu shares persistent Expanded/Packed placement, Horizontal/Vertical flow, uniform neuron scale, spacing, connection visibility, and All/Selected connection display between 2D and 3D. `Theme` controls interface surfaces and text, while `Colors` independently selects the shared semantic visualization palette. MiaIA Classic preserves the original theme-aware mapping; High Contrast, Color-blind Safe, Monochrome, and ten persisted custom colors apply immediately to both renderers, legends, traces, selection, debug emphasis, signal-health diagnostics, and timeline state. Performance preferences, help, and host actions follow on the same row. Training and phase-debug actions share the second row with their live session and phase status. Path-based operations use a themed in-panel prompt so the editor and packaged application retain identical behavior without an editor-only file-dialog dependency.

The `MiaIAStudio` game target is the development entry point for the independent host. `IDE/Unreal/Build/Package-Windows.ps1` builds, cooks, stages, and archives that target for Win64. Its shared panel now exposes a `2D`/`3D` selector backed by `StudioCore`. Both views share single and multiple neuron selection, additive `Ctrl` selection, rectangular marquee selection, group dragging, Inspector state, and a persistent neuron-label preference. Detailed neuron IDs use the same compact semantic capsule in both renderers, highlight the primary selection, and derive their measured bounds and clamped font size from the visible neuron diameter. A blinking vertical cursor appears to the left of the primary detailed neuron in both renderers, matches its visible diameter, and completes one blink per second without replacing its activation or debug color. Its default Adaptive behavior follows automatic zoom and density visibility while remaining independent of `Show neuron labels` and available in Packed placement. The persistent `Always show selection cursor` option bypasses those automatic visibility limits and preserves a minimum 12-pixel marker; secondary selections retain their semantic label highlight. A single neuron selection displays its paged Relationship Explorer with exact and filtered totals; a connection selection displays both endpoint neurons. The per-direction page size is stored beside the detailed-topology limits and applies immediately. The 3D mode hosts a real runtime Unreal viewport with an initial 2D-equivalent front camera, one aggregated mesh of shaded neuron spheres and weighted connection cylinders, stable FXAA, the same gamma and semantic color composition as the Slate canvas, projected neuron labels, orbit, pan, extended zoom, and adaptive fit. A shared expanded-workspace mode temporarily gives the topology all space outside the retained right-side Inspector while preserving the same widget and state, so the editor tab and packaged application present the same visualization behavior.

The toolbar `Help` menu provides an embedded interaction reference and an `About MiaIA Studio` dialog. Both use the same themed in-panel Slate overlay in the editor and packaged application instead of relying on host-specific operating-system message boxes. The About text reads the project version from Unreal configuration and identifies the shared Engine, SDK, and CLI service boundary instead of presenting the Unreal frontend as the complete MiaIA architecture.

The lower `Breakpoints` tab is connected to the same SDK state used by the Console. It can create mathematical-phase, neuron-activation, neuron-gradient, and connection-update conditions; enable, disable, or remove individual entries; clear the collection; display hit counters; and report the latest trigger. Phase breakpoints select one of the existing debug phases. Value breakpoints accept a neuron or connection ID and a numeric threshold. A background session that hits a condition returns to `Active` at the next safe sample boundary, so the topology and Inspector can be examined without observing a partial update.

All breakpoint operations are also exposed by the Unreal Blueprint function library. The graphical tab is therefore a client of the public `MiaIAClient` breakpoint facade rather than a separate Unreal-only implementation.

Quick Help also identifies the public source location and license boundary. About displays the MPL-covered source location and the Unreal Engine attribution required for the packaged frontend. Public archives are created through `Build/Package-Windows.ps1`, which adds the MiaIA license, end-user terms, trademark policy, third-party notice index, and collected dependency licenses beside the top-level launcher after Unreal packaging succeeds.

The Unreal renderer batches neuron markers and weighted links in one runtime component instead of creating one Actor per model element. Detailed mode still retains every element ID for selection and Inspector queries. Compact mode remains a separate scalability policy that summarizes each layer when the full graph exceeds the shared limits.

The detailed Model Explorer uses expandable layer branches, neuron children, and one independent connection branch. Expand-all and collapse-all controls make dense models manageable, and selection from either topology renderer reveals the corresponding branch without changing the renderer-neutral selection state. Universal preview navigation shows a whole-network summary first. Opening a network within the limits restores the normal detailed explorer; opening a larger network shows layer summaries, and a focused compact layer reuses the detailed explorer for that layer alone. Expansion and preview-navigation state are frontend-only and never become part of the network model.

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
