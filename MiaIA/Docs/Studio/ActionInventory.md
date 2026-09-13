# Studio action coverage inventory

## Status and scope

Source audit: 2026-09-13, after editable parameter synonyms and input-history
separation. **This is an inventory and implementation backlog, not a new feature.**
The inventory itself enables no behavior. The first view/layout slice below has
subsequently been implemented with the author's approval; all other gaps remain.

Scope: the shared MiaIA Studio editor/standalone panel, its 2D/3D viewport gestures,
and the CLI exposed by its console. Unreal Editor's own unrelated menus and operating
system file-dialog controls are outside this inventory. Related controls are grouped
below, but distinct effects are called out explicitly.

The current shared CLI catalog has **74 executable entries** (excluding category
completion entries). **30** have a command-assistant intent; **44** do not.
These are command-entry counts, not counts of buttons or a percentage of IDE coverage.
The full command/parameter checklist is in the appendix. In addition, **12 fixed
Studio-only view actions** now have assistant intents; these are not shared CLI entries.

Language packs can translate phrases and parameter labels for supported intents.
They cannot supply the missing command implementations or add executable commands.
Every future command must include its optional parameters, not only its common form.

## Evidence and interpretation

- [CLI catalog and dispatcher](../../CLI/Source/MiaIACommandProcessor.cpp):
  `CommandCatalog()`, command execution and validation.
- [Proposal allowlist](../../IDE/StudioCore/Source/CommandAssistant.cpp):
  `CommandAssistant::Propose`.
- [Inspection intents](../../IDE/StudioCore/Include/AssistantInspectionCatalog.h)
  and [typed parameters](../../IDE/StudioCore/Include/AssistantParameterCatalog.h).
- [Local recognition](../../IDE/StudioCore/Source/LocalCommandAssistant.cpp).
- [Panel actions and control bindings](../../IDE/Unreal/Source/IDEStudio/Private/Widgets/SMiaIAEditorPanel.cpp)
  and [panel declarations](../../IDE/Unreal/Source/IDEStudio/Public/Widgets/SMiaIAEditorPanel.h).
- [2D input handlers](../../IDE/Unreal/Source/IDEStudio/Private/Widgets/SMiaIANetworkView.cpp),
  [3D input handlers](../../IDE/Unreal/Source/IDEStudio/Private/Widgets/SMiaIA3DNetworkView.cpp)
  and [visual settings](../../IDE/Unreal/Source/IDEStudio/Public/Widgets/MiaIAVisualizationSettings.h).

“CLI equivalent” means the underlying operation or report exists. It does **not**
mean that executing it opens a tab, selects an item or installs a graphical overlay.
`ExecuteConsoleCommand` executes the shared CLI, appends output and refreshes data;
it is not a general dispatcher for the panel's selection, camera or playback handlers.

For example, `inspect neuron 2` prints an inspection; it is not the same as
`SelectNeuron(2)` or `RevealNeuron(2)`. Likewise, `dataset diagnose` and
`model compare` produce reports, while the graphical actions retain separate
presentation snapshots and overlays.

## A. View, layout, appearance and performance

All actions in this table exist in the GUI. The first slice now supports 2D/3D,
Expanded/Packed, labels show/hide, connections show/hide and All/Selected, Fit view and Reset layout
through the [12 Studio host commands](OnlineAssistant.md#studio-view-and-layout-actions).
Other rows remain GUI-only, with no CLI command or assistant intent. Handler names
are source anchors, not proposed public syntax.

| Action / parameters | Existing implementation / important distinction |
| --- | --- |
| Switch 2D / 3D | `SelectViewMode`; entering 3D fits its view |
| Select Classic / Coaxial Rings / Spiral Tokens | `SelectVisualization`; the latter two are 3D-only |
| Expanded / Packed layout | `SelectLayoutMode`; non-Classic 3D constraints must be preserved |
| Horizontal / Vertical flow and Forward / Reverse direction | `SelectLayoutOrientation`; clicking the current orientation reverses direction |
| Neuron size, 25–300% | `HandleNeuronScaleChanged` |
| Neuron gap, 0–500% of diameter | `HandleNeuronGapChanged` |
| Layer gap, 0–1000% of diameter | `HandleLayerGapChanged` |
| Show / hide neuron labels | `bShowNeuronLabels` control |
| Always show selection cursor | `bAlwaysShowSelectionCursor` control |
| Show / hide connections | `bShowConnections` control |
| Connection visibility, 0–200% | `HandleConnectionScaleChanged`; affects fade/thickness, not network weights |
| All / Selected connections | `SelectConnectionDisplayMode`; presentation filter, not selection mutation |
| Fit view | `HandleFitView`; active renderer only |
| Reset layout | `HandleResetLayout`; 2D `ResetLayout` versus 3D `ResetView` |
| Reset visualization defaults | `HandleResetVisualizationSettings`; different from resetting manual layout |
| Expand / restore topology workspace | `HandleToggleTopologyWorkspace`; deferred fit |
| Refresh now | `HandleRefresh` |
| Adaptive / 1 / 2 / 4 / 10 Hz data refresh | `SelectDataRefreshMode`; presentation polling, not training speed |
| Neuron, connection and Inspector connection limits; apply / reset | `HandleApplyTopologyLimits`, `HandleResetTopologyLimits`; bounded rendering/query sizes |
| Theme: Follow Unreal / Dark / Light | `SelectTheme` |
| Palette presets and custom individual colors | `SelectVisualizationPalette`, `HandleEditVisualizationColor`, `HandleVisualizationColorCommitted` |
| Resize split panes | Slate splitters; presentation only |

## B. Navigation, selection and exploration

All have **no CLI/assistant equivalent for their graphical effect**. Textual
inspection equivalents are listed separately below.

| Action / parameters | Existing implementation |
| --- | --- |
| 2D zoom in/out at pointer | `SMiaIANetworkView::OnMouseWheel` |
| 2D pan | Right/middle drag in `OnMouseButtonDown` / `OnMouseMove` |
| 3D zoom in/out | `SMiaIA3DNetworkView::OnMouseWheel` |
| 3D orbit | Right drag |
| 3D camera pan | Middle drag |
| Move a neuron / selected group visually | Drag handlers in both views; does not change mathematical topology |
| Move a compact layer visually | 3D layer-drag handlers |
| Select neuron, connection or layer by stable ID | `SelectNeuron`, `SelectConnection`, `SelectLayer` |
| Multi-select / modify selection / marquee selection | Viewport selection handlers and `SelectNeurons` |
| Clear selection by viewport interaction | Viewport mouse handlers |
| Previous / next neuron; previous / next layer | `NavigateNeuron` and viewport arrow-key handlers |
| Focus / reveal a neuron | `HandleFocusExplorerNeuron`, renderer `RevealNeuron`; distinct from inspection |
| Open network overview / open layer detail / go back | `OpenNetworkFromPreview`, `OpenLayerDetail`, `NavigateBackFromTopology`; double-click / Enter / Escape |
| Expand/collapse Explorer layer, connections, all nodes | `HandleToggleExplorerLayer`, `HandleToggleExplorerConnections`, expand/collapse-all handlers |
| Relationship direction: incoming/outgoing | `SelectRelationshipDirection` |
| Relationship sort key, ascending/descending, minimum absolute weight | Relationship sort/menu/threshold handlers |
| Previous/next relationship page | Relationship pagination handlers; page size comes from Inspector limit |
| Select relationship connection / navigate opposite neuron | `HandleSelectRelationshipConnection`, `HandleNavigateRelationshipNeuron` |
| Navigate source/destination of selected connection | `HandleNavigateSelectedConnectionEndpoint` |

Numeric zoom increments, orbit units, pan axes, target selection semantics and
phrases for these operations remain **unapproved design decisions**, not implicit
defaults derived from mouse gestures.

## C. Projects, models, editing and data

“Supported” below refers to an existing assistant proposal for the CLI operation.

| IDE operation or console-accessible operation | CLI equivalent | Assistant coverage |
| --- | --- | --- |
| New / open / save project; Save As | `project new/open/save [path]` | Supported; native path-picker interaction is separate |
| Project information | `project info` | Supported text report |
| Create / list / select model context | `model create/list/select` | Supported |
| Fork / rename / remove model context | `model fork/rename/remove` | Missing |
| Create network, including bare defaults and all 8 explicit parameter roles | `create` | Supported |
| Configure existing activations, weights and biases | `network configure` | Missing |
| Apply selected neuron's bias | `network set neuron-bias` | Missing; ID and finite value required; input neurons excluded |
| Apply selected connection's weight | `network set connection-weight` | Missing |
| Import / export ONNX | `import onnx`, `export onnx` | Missing |
| Assign inputs / predict / forward / benchmark | `input`, `predict`, `forward`, `benchmark` | Missing |
| Import CSV, including header option and dimensions | `dataset import csv` | Missing |
| Dataset summary / inspect sample | `dataset summary/inspect` | Supported reports |
| Apply sample / evaluate sample or all / gradients / clear dataset | `dataset apply/evaluate/gradients/clear` | Missing |
| Network summary / full inspect / neuron / connection inspection | `summary`, `inspect`, `inspect neuron/connection` | Supported reports, not visual selection |
| Page/sort/filter relationships | `inspect relationships` | Missing, and does not configure the GUI relationship explorer |

Some rows are available through Studio's integrated console rather than a dedicated
button. They remain in scope because complete console coverage is required too.

## D. Training, debugging and breakpoints

| Action | CLI equivalent | Assistant coverage / GUI distinction |
| --- | --- | --- |
| Start session: epochs, learning rate, order, seed | `train session start` | Supported; MSE fixed by current builder |
| Run steps or all / resume / pause / status | `train session run/resume/pause/status` | Supported |
| Next sample / cancel session | `train session next/cancel` | Missing |
| Atomic SGD sample / full epoch | `train step/epoch` | Missing |
| Start standalone debug with sample and rate | `train debug start` | Missing |
| Attach debug to next session sample | `train session debug` | Missing |
| Advance / cancel phase debugging | `train debug next/cancel` | Missing |
| Read debug status | `train debug status` | Supported report |
| Inspect debug neuron / connection | `train debug neuron/connection` | Missing |
| Add breakpoint: phase, activation above/below, gradient magnitude, weight-update magnitude | `train breakpoint add ...` | Missing; each condition's IDs/thresholds/phase must be mapped |
| List breakpoints | `train breakpoint list` | Supported report |
| Enable/disable, remove, clear breakpoints | `train breakpoint enable/remove/clear` | Missing |
| List/inspect/compare retained training steps | `train session history/inspect/compare` | Supported reports |
| Select a timeline row | Report counterpart: `train session inspect` | Visual selection missing (`HandleSelectTrainingTimelineStep`) |
| Clear timeline view | None | Missing; must not cancel training or delete retained Engine history |

## E. Trace, diagnostics, checkpoints and comparison

| Action | CLI equivalent | Assistant coverage / GUI distinction |
| --- | --- | --- |
| Capture forward trace with input vector | `trace forward` | Missing; GUI snapshot/overlay requires its presentation action too |
| Inspect incoming contribution page with sort/filter | `trace neuron` | Missing |
| Capture backward trace with input and target vectors | `trace backward` | Missing |
| Forward/backward previous frame, next frame, play/pause, restart, speed, clear | None | Missing; `Handle*Trace*` and speed handlers; never updates network parameters |
| Previous/next forward contribution page; select contribution | Text counterpart: `trace neuron` | Missing GUI navigation |
| Diagnose dataset: sample limit and all threshold/max-items options | `dataset diagnose` | Supported report; Analyze dataset overlay is a separate GUI action |
| Select diagnostic category, finding, or clear diagnostics | None for GUI effect | Missing; filter must not rerun analysis |
| Capture / restore / remove / clear checkpoint | `checkpoint create/restore/remove/clear` | Missing |
| List / inspect / compare checkpoints | `checkpoint list/inspect/compare` | Supported reports |
| Refresh checkpoint list; select checkpoint; assign A/B; compare in tab | Report counterparts above | Visual selection and comparison-side assignment missing |
| Compare two model contexts, maximum items | `model compare` | Supported report |
| Choose Current/Reference and maximum displayed differences; show/clear comparison overlay | Report counterpart: `model compare` | Graphical state missing; `HandleSelectModelComparisonContext`, `HandleCompareModelContexts`, `HandleClearModelComparison` |

## F. Workspace, console and assistant management

These are genuine IDE operations but currently are not ordinary NLU intents unless
explicitly stated.

| Actions | Current boundary |
| --- | --- |
| Switch Console, Training timeline, Breakpoints, Execution trace, Gradient trace, Diagnostics, Checkpoints, Model compare tabs | GUI-only `SelectBottomTab` |
| Quick help / About / dismiss dialog | GUI-only; `miaia_help` produces CLI help, not these dialogs |
| Confirm/cancel project-path dialog | GUI-only; distinct from executing a known path |
| Exit application | `HandleExit`; CLI `exit` only exits terminal host and explicitly does not close Studio |
| Submit input / choose suggestion or example / Tab completion | Host controls; not independent model commands |
| Recall previous/next entry, History Auto/All/Console/Assistant | Host input history; output remains continuous |
| Clear output | Host-only; does not clear history, models or datasets |
| Settings / Review open-close; Enabled; provider; language; Confident mode | GUI configuration; no ordinary NLU intents |
| Wit.ai credential save/cancel and token fields | Development-only configuration; credentials must not become corpus/examples |
| Confirm / discard / wrong interpretation | Existing local proposal controls; confirm/cancel phrases handled locally, not new provider intents |
| Select pending/validated phrase, choose intent, validate, reassign, delete | Supervised-learning GUI controls |
| Export/import corpus backup; reset learned data | Local data-management GUI controls |
| Export language template / parameter synonyms / editable installed pack | Local file export controls |
| Browse pack file/directory, preview, install/replace, remove installed pack | Local file/pack-management controls; existing validation and recovery behavior must remain |

The target is eventual semantic access to IDE actions, not arbitrary button clicking.
Security-sensitive configuration, destructive operations, file replacement and
confirmation-policy changes need explicit design approval before adding intents.

## Delivery order and approval gates

The target is full semantic assistant coverage of every IDE action, including
navigation, selection, settings and all console parameters. This is an incremental
backlog, not a claim that all actions are implemented. New UI/behavior choices require
the project author's confirmation before implementation.

1. **Completed:** editable/exportable parameter synonyms for the existing intent set.
2. **This document:** audit GUI actions and enumerate every current executable CLI
   entry, including optional parameters and missing assistant coverage.
3. **First slice implemented:** view/layout subset below. Next, subject to approval:
   navigation, selection/inspection, then remaining operations in sections C–F.

The approved first slice is: 2D/3D, Expanded/Packed, neuron labels,
connections, Fit view and Reset layout. It uses the existing assistant UI, existing
confirmation/Confident policy and existing control effects. No new toolbar buttons
were added. Explicit show/hide and mode intents map to a finite Studio host catalog.

For subsequent slices or changes to the approved behavior, confirm with the project author:

- Where new controls (if any) appear, and whether existing controls suffice.
- Whether selecting a view changes layout or framing as the current button does.
- Which “reset” is intended: manual layout/camera or all visualization preferences.
- Explicit set versus toggle semantics, and handling of unavailable combinations.
- Confirmation/Confident-mode behavior for presentation-only actions.
- How Studio-only commands report “unavailable” in a terminal-only host.

Later navigation needs separate confirmation of units, axes, relative/absolute values
and what “selected”, “this neuron”, “next” and “focus” mean. Filter changes must be
approved rather than inferred from a phrase.

Implementation requirements after those choices: keep local recognition portable;
validate against a finite supported action/parameter catalog; reuse existing handlers
or shared controller operations; do not simulate mouse clicks; test identical outcomes
in English and Italian and exported aliases; cover invalid values, missing selection,
compact topology, 2D/3D restrictions, training ownership and destructive boundaries.
The Studio view-action dispatch path now calls existing view handlers. It does not
turn textual inspection commands into graphical selection, overlays or navigation.

## Complete current CLI checklist

This table is transcribed from executable `CommandCatalog()` entries. Category-only
completion prefixes are excluded. “Missing” means no mapping in the current proposal
allowlist, even if the command is already usable manually in Studio.
CLI `help` also accepts a topic/prefix through the command processor; the assistant's
current help proposal is bare `help`.

| CLI syntax (existing) | Current assistant intent |
| --- | --- |
| `help` | `miaia_help` |
| `create <inputs> <hidden-width> <hidden-layers> <outputs> [--hidden-activation <type>] [--output-activation <type>] [--weight <value>] [--bias <value>]` | `miaia_network_create` |
| `network configure [--hidden-activation <type>] [--output-activation <type>] [--weight <value>] [--bias <value>]` | Missing |
| `network set neuron-bias <neuron-id> <value>` | Missing |
| `network set connection-weight <connection-id> <value>` | Missing |
| `input <value...>` | Missing |
| `predict <value...>` | Missing |
| `trace forward <value...>` | Missing |
| `trace backward <input...> -- <target...>` | Missing |
| `trace neuron <neuron-id> [page] [page-size] [id\|contribution\|abs-contribution] [asc\|desc] [minimum-absolute-contribution] -- <value...>` | Missing |
| `import onnx <path>` | Missing |
| `export onnx <path>` | Missing |
| `project new` | `miaia_project_new` |
| `project open <path.mai>` | `miaia_project_open` |
| `project save [path.mai]` | `miaia_project_save` |
| `project info` | `miaia_project_info` |
| `model create <name>` | `miaia_model_create` |
| `model fork <source-id> <name>` | Missing |
| `model list` | `miaia_model_list` |
| `model select <id>` | `miaia_model_select` |
| `model rename <id> <name>` | Missing |
| `model remove <id>` | Missing |
| `model compare <reference-id> <current-id> [maximum-items]` | `miaia_model_compare` |
| `checkpoint create <name>` | Missing |
| `checkpoint list` | `miaia_checkpoint_list` |
| `checkpoint inspect <id>` | `miaia_checkpoint_inspect` |
| `checkpoint compare <first-id> <second-id> [maximum-items]` | `miaia_checkpoint_compare` |
| `checkpoint restore <id>` | Missing |
| `checkpoint remove <id>` | Missing |
| `checkpoint clear` | Missing |
| `dataset import csv <inputs> <targets> [--no-header] <path>` | Missing |
| `dataset summary` | `miaia_dataset_summary` |
| `dataset inspect <sample-index>` | `miaia_dataset_inspect` |
| `dataset apply <sample-index>` | Missing |
| `dataset evaluate <sample-index\|all> mse` | Missing |
| `dataset gradients <sample-index> mse` | Missing |
| `dataset diagnose [sample-limit] [--inactive-magnitude <value>] [--inactive-ratio <value>] [--saturation-margin <value>] [--saturation-ratio <value>] [--vanishing-magnitude <value>] [--vanishing-ratio <value>] [--exploding-magnitude <value>] [--exploding-ratio <value>] [--max-items <count>]` | `miaia_dataset_diagnose` |
| `dataset clear` | Missing |
| `train breakpoint add phase <before\|forward\|backward\|update\|verify\|commit>` | Missing |
| `train breakpoint add activation-above <neuron-id> <threshold>` | Missing |
| `train breakpoint add activation-below <neuron-id> <threshold>` | Missing |
| `train breakpoint add gradient-above <neuron-id> <magnitude>` | Missing |
| `train breakpoint add weight-update-above <connection-id> <magnitude>` | Missing |
| `train breakpoint list` | `miaia_breakpoint_list` |
| `train breakpoint enable <id> <on\|off>` | Missing |
| `train breakpoint remove <id>` | Missing |
| `train breakpoint clear` | Missing |
| `train step <sample-index> <learning-rate> mse` | Missing |
| `train epoch <learning-rate> mse` | Missing |
| `train debug start <sample-index> <learning-rate> mse` | Missing |
| `train debug status` | `miaia_debug_status` |
| `train debug next` | Missing |
| `train debug neuron <neuron-id>` | Missing |
| `train debug connection <connection-id>` | Missing |
| `train debug cancel` | Missing |
| `train session start <epochs> <learning-rate> mse [sequential\|shuffle <seed>]` | `miaia_training_start` |
| `train session status` | `miaia_training_status` |
| `train session next` | Missing |
| `train session run <steps\|all>` | `miaia_training_run` |
| `train session resume` | `miaia_training_resume` |
| `train session pause` | `miaia_training_pause` |
| `train session debug` | Missing |
| `train session history` | `miaia_training_history` |
| `train session inspect <step-index>` | `miaia_training_inspect` |
| `train session compare <first-step-index> <second-step-index> [maximum-items]` | `miaia_training_compare` |
| `train session cancel` | Missing |
| `summary` | `miaia_network_summary` |
| `inspect` | `miaia_network_inspect` |
| `inspect neuron <neuron-id> [maximum-connections]` | `miaia_neuron_inspect` |
| `inspect connection <connection-id>` | `miaia_connection_inspect` |
| `inspect relationships <neuron-id> <incoming\|outgoing> [page] [page-size] [id\|weight\|abs-weight] [asc\|desc] [minimum-absolute-weight]` | Missing |
| `forward` | Missing |
| `benchmark` | Missing |
| `exit` | Missing |
