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

### Unreal Blueprint training debug slice

The Unreal client progressed from a native `BeginPlay` linkage proof to a reflected Blueprint function library. Unreal structures now represent training sessions, debug phases, selected neurons, and selected connections without exposing STL types to Unreal Header Tool.

Blueprint nodes can create a dense network, import a CSV dataset, start a controlled session, advance its current sample through mathematical debug phases, cancel the candidate, and inspect focused gradient and update data. All nodes delegate to `MiaIAClient`; no training mathematics moved into the IDE module.

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

### Cooperative background pause and resume

The SDK gained a single background training worker with explicit Resume and Pause operations. Pause requests cooperative stop, waits for the current atomic step, and joins the worker before returning an Active session at a coherent boundary.

All access to the process-local network, dataset, and session is serialized. Inspection remains available while Running, while state-changing SDK operations are rejected until pause. Completion, requested pause, requested cancellation, and failed-step stops are observable without exposing partially updated parameters.

### Navigable training history

Retained session steps became directly queryable through a focused Engine inspector and SDK facade. A lightweight history view maps each global step to its epoch, sample, loss transition, and update counts, while detailed lookup returns the original gradients and parameter deltas.

Console history and inspect commands make this information usable without requiring clients to interpret the complete session object themselves.

### Phase-by-phase mathematical training debug

The atomic sample step was decomposed into an explicit transaction controller without duplicating its mathematics. A client can start from a private candidate network and advance separately through forward evaluation, backward differentiation, SGD update, post-update verification, and final commit.

Every phase exposes a stable snapshot containing the candidate network and the results calculated so far. Public weights and biases remain unchanged until the verified candidate is committed; cancellation discards the candidate completely. The ordinary atomic training step now composes the same controller through completion.

Focused inspector snapshots subsequently made individual neurons and connections queryable without traversing the complete transaction. They compare public and candidate values and expose gradients and optimizer updates only after the corresponding phase, using availability flags so a client never confuses missing data with a mathematical zero.

The phase transaction was then attached to controlled sessions. A session can open its current sample in the debugger, inspect or cancel it without moving progress, and promote a verified commit into network state, history, and the next cursor exactly once. Ordinary and debugged steps share the same guarded recording operation.

### MiaIA Studio runtime and standalone host

The first graphical shell grew from an Unreal Editor tab into a shared runtime UI. Renderer-neutral `StudioCore` owns command, refresh, selection, scalability, and logical 2D/3D scene policies. The Unreal `IDEStudio` runtime module owns the Slate panel, topology view, theme, and game-instance host, while `IDEEditor` retains only editor tab registration and demonstration installation.

The `MiaIAStudio` game target now launches the same interface without Unreal Editor. A reproducible Win64 packaging script selects that target, cooks the main map, stages Pak and IoStore content, includes Windows prerequisites, and archives the complete application outside tracked source files. The first Development archive was built and executed successfully. The standalone host subsequently gained an explicit windowed default and clean `Exit` action while preserving the editor panel's native tab behavior.

### Safe training breakpoints

Controlled sessions gained engine-owned breakpoints for mathematical phases, neuron activation thresholds, neuron bias-gradient magnitudes, and connection update magnitudes. Automatic training evaluates conditions only after an atomic sample commit and returns the worker to an Active session before the following sample, preserving the existing safe-boundary guarantee.

Structured breakpoint definitions, hit counters, and latest-trigger snapshots are exposed consistently through Engine, `MiaIAClient`, the shared CLI interpreter, Unreal Blueprint wrappers, and the MiaIA Studio Breakpoints tab.

### Shared 2D and 3D topology experience

The shared editor and standalone panel gained a user-selectable 2D/3D topology mode. The existing Slate canvas remains the editable two-dimensional renderer, while the new mode embeds a real runtime Unreal preview scene driven by StudioCore three-dimensional coordinates.

The first 3D renderer supports orbit, pan, an extended zoom range, fit, left-click neuron or connection selection through the same Inspector callbacks as 2D, and camera-plane dragging for detailed neurons. Its default camera preserves the familiar 2D topology reading before orbit reveals the depth arrangement. `Fit view` now derives its front-camera target and distance from the current network bounds and actual viewport aspect ratio, preserving manual positions while filling the available canvas; `Reset layout` additionally restores automatic coordinates. The workspace can temporarily collapse Explorer and the lower tools around the same live topology widget while retaining the Inspector on the right, then restore them without losing view or selection state. Shaded neuron spheres and weighted connection cylinders are combined into one adaptive-tessellation dynamic mesh submission, with stable FXAA, neutral viewport gamma, faithful non-emissive semantic colors, projected neuron identifiers, and a perspective-correct circular selection outline rather than one Actor per element. Detailed mode therefore preserves element identity without multiplying Unreal object overhead, while the pre-existing compact mode continues to summarize networks beyond the visualization limits and deliberately leaves aggregate-node dragging for the future layout design.

The detailed-versus-compact thresholds then became persistent Studio preferences rather than fixed host policy. The default remains 2,000 neurons and 5,000 connections, while independent high finite values can be applied from the shared toolbar and are immediately re-evaluated against the lightweight network overview.

Detailed layout then moved to a shared diameter-based coordinate system. Expanded and Packed placement, persistent Horizontal and Vertical flow, uniform neuron scale, explicit neuron and layer gaps, and All/Selected connection display now drive both renderers. Automatic and manual placement prevent physical node intersections, Packed mode can make symmetric layers exactly adjacent, and the 3D viewport follows the Slate canvas aspect ratio so spheres and selection outlines remain circular while resizing. The 2D canvas supports both middle- and right-button panning while preserving left-button marquee selection.

As the shared controls grew, the single toolbar was split into stable layout and training-debug rows. This keeps view and performance preferences separate from step controls while retaining session and phase status at the point of interaction on narrow editor and standalone windows.

### Versioned MiaIA projects

The first `.mai` project format established persistence above ONNX without changing MiaIA product or component names. Version 1 embeds the supported ONNX model and records the CSV dataset reference and schema, training configuration, and breakpoint definitions in tagged sections. Engine loading is transactional, saving publishes through a sibling temporary file, and an unavailable external dataset does not prevent the embedded model and other metadata from opening.

The SDK, shared CLI, Unreal Blueprint library, and MiaIA Studio `Project` menu expose the same new, open, save, and information operations. Tests cover round trips, atomic overwrite, missing datasets, corrupt archives, unsupported versions, and unchanged client state after a rejected open.

### Retained training-step comparison

Controlled-session history gained a renderer-neutral comparison operation above the existing retained step snapshots. `TrainingStepComparer` associates neurons and connections by stable ID rather than vector position and reports signed and absolute differences for loss, output predictions, gradients, final weights, and final biases. Availability flags preserve the distinction between missing information and a mathematical zero.

`TrainingSessionInspector` validates both history indexes, `MiaIAClient` serializes access through the existing client-state lock, and the shared CLI exposes bounded ranked output through `train session compare`. Comparisons remain read-only while foreground or background training continues. Full hidden-neuron activation comparison remains deliberately deferred because retaining complete network state for every step requires an explicit memory policy.

### Focused network relationship inspection

Ordinary network inspection gained element-level contracts independent of the training debugger. A neuron query now returns its layer context, current activation and bias, exact incoming and outgoing connection counts, and a caller-bounded subset of each relationship direction. A connection query returns its weight and contextual snapshots of both endpoint neurons. Invalid identifiers preserve the caller's previous result.

`NetworkInspector` and `NetworkTopology` own the read-only Engine operation, `MiaIAClient` exposes it through the serialized SDK facade, and the shared CLI provides `inspect neuron` and `inspect connection`. StudioCore retains the selected element's inspection state, while the Unreal Blueprint adapter and shared 2D/3D Inspector present the same information with a persistent configurable relationship limit. Complete topology snapshots are no longer required merely to explain one selected element.

### Configurable dense-network creation

Dense creation gained one shared Core configuration for hidden-layer activation, output-layer activation, uniform initial connection weight, and uniform non-input bias. Engine creation remains transactional: unsupported activation values, non-finite parameters, invalid dimensions, and invalid generated graphs leave the published network unchanged. The original overload remains available and preserves Sigmoid activations, `0.1` weights, and zero biases.

The SDK overload, shared CLI `create` options, and Unreal `Create Configured Dense Network` node expose the same contract without duplicating factory logic. Input values remain raw and layer zero is not activation-configurable. Random initialization and preprocessing policies remain deliberately separate future concerns.

### Transactional parameter reconfiguration

Existing networks gained a batch parameter-update contract above the single-element editing operations. A request can independently replace hidden activation types, the output activation type, every connection weight, and every non-input bias. Engine applies the complete request to a candidate network, validates it, and publishes both the network and change counts only after success. Invalid, empty, or training/debug-blocked requests preserve both the public network and the caller's previous result.

The SDK, shared CLI `network configure` command, and Unreal `Configure Network Parameters` node expose the same operation. Layer zero remains untouched. Global weight and bias replacement is intentionally explicit because applying it to a trained network overwrites learned parameter values; fine-grained single-element operations remain available separately.

### Targeted parameter editing

Single-element parameter editing became consistent across the shared clients. `network set neuron-bias` and `network set connection-weight` address stable model IDs, while matching Blueprint functions allow graphical flows to use the same operations. The MiaIA Studio Inspector exposes the focused controls only for one selected editable element and explains why input-neuron biases are unavailable.

The Engine owns finite-value, identifier, and input-layer rules; `MiaIAClient` owns the common training and phase-debug mutation guard. Unreal remains a forwarding and presentation layer, so Console, Blueprint, and Inspector requests cannot bypass the same model boundary.

The Model Explorer was reorganized as an expandable hierarchy in the same panel update. Layers own their neuron rows, connections use an independent branch, and global expand/collapse actions avoid forcing complete long lists onto narrow windows. Selection from either topology renderer reveals the appropriate branch while compact mode continues to expose summaries only.

Shared layout preferences then separated the layout axis from its layer direction. Re-selecting Horizontal or Vertical toggles Forward and Reverse, mirrors detailed and compact scenes consistently in 2D and 3D, persists the choice per user, clears obsolete manual positions, and reverses only the between-layer keyboard mapping so arrow navigation continues to follow the visible model.

Compact topology gained its first bounded drill-down. A layer aggregate can be selected for an Inspector summary, then opened by double-click or `Enter`. The focused view requests only that layer through the existing atomic SDK inspection boundary, exposes its neurons through the normal detailed renderers and Model Explorer, and returns to the overview through `Back` or `Esc` without ever requesting the complete large-network snapshot.

The topology flow then gained a universal whole-network preview shared by 2D and 3D. One distinctive central node summarizes the complete model, while label-free lateral parallel indicators communicate input and output dimensions. Indicator spacing remains constant and grows symmetrically away from the center, with fit adapting to the complete visible bundle instead of compressing it into predetermined bounds. Every new, opened, or imported network reaches this preview before the detailed-versus-compact limit policy is applied. Clicking the node or pressing `Enter` restores the earlier behavior: safe networks open their complete detailed topology, while larger networks open the existing linear layer aggregates and retain their bounded layer drill-down. `Back` or `Esc` reverses the available levels to the preview. Numeric headers, Explorer, and Inspector values retain exact input, output, layer, neuron, and connection counts even when extremely large visible indicator bundles are capped for frontend responsiveness.

Focused relationships then gained an explicit page contract above the original bounded compatibility snapshot. Engine queries select one direction, offset and page size, order by stable ID or signed/absolute weight, and filter on minimum absolute magnitude while preserving exact unfiltered and filtered totals. SDK, CLI, and Blueprint expose the same immutable operation. MiaIA Studio uses it as an interactive Relationship Explorer with previous/next navigation, connection selection, and opposite-neuron navigation; compact large models switch atomically between focused layers rather than copying the complete graph.

### Immutable forward execution trace

Forward propagation gained a renderer-neutral explanation contract without introducing a second mathematical implementation. The traced `ForwardEngine` path records each neuron's weighted input sum, bias, pre-activation, and final activation while executing the same activation switch as ordinary forward propagation. `NetworkInspector` runs this path on a network copy, so successful traces preserve public activations, layer order, weights, and biases.

A focused contribution query pages one target neuron's incoming `source activation * weight` terms with deterministic sorting and absolute-magnitude filtering. `MiaIAClient`, `trace forward`, and `trace neuron` expose the complete and focused views. Exact numerical tests cover the full calculation, pagination, filtering, invalid-query result preservation, and unchanged public state. Graphical animation remains separate from this first Engine contract.

### Graphical forward trace

StudioCore now retains a renderer-neutral graphical trace session above the immutable Engine contract. It owns the captured trace, focused neuron, page request, and last valid contribution page. Invalid execution, focus, or paging operations do not replace the last valid state.

The shared Unreal editor and standalone panel gained an `Execution trace` tab. One input vector produces captured outputs, per-neuron Inspector values, activation coloring in both detailed renderers, and exact signed highlighting for the visible contribution page while unrelated links are subdued. Previous and Next keep dense incoming sets bounded, contribution rows open normal connection inspection, and Clear removes the overlay without mutating the model. Temporal signal animation remains a separate later visualization step.

The trace integration also introduced the first explicit Unreal instance boundary. An opaque handle identifies the initial `default` instance, while a runtime service in `IDE.dll` owns and invokes its `StudioController`. The Slate module therefore shares the exact process-local SDK state used by Blueprint and the integrated CLI instead of acquiring a second static-library copy. True simultaneous model contexts remain future native SDK work behind this boundary.

## Current position

MiaIA can now create dense networks with explicit initialization choices, reconfigure existing parameters transactionally, represent, execute, interchange, persist a `.mai` v1 project, inspect individual network relationships, evaluate, differentiate, debug one training step phase by phase, train through controlled foreground or background sessions, and navigate or compare the complete history of successful steps. It cannot yet persist visualization layouts or training history inside that project, retain complete hidden-neuron activations for every step, or deliver the planned complete graphical debugging experience.

Those next steps are tracked in the [Roadmap](../Roadmap/Roadmap.md).
