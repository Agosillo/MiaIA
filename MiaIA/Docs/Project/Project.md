# MiaIA Project Format

## Purpose

`.mai` is the versioned MiaIA project format. The product, namespaces, libraries, APIs, and documentation remain named **MiaIA**.

ONNX remains the external model-interchange format. A `.mai` project contains one or more model contexts. Each context groups MiaIA-specific state around an optional neural network, while the active context's network can still be imported from or exported to ONNX when its graph belongs to the supported subset.

## Current version: 2

A `.mai` v2 project preserves:

- every model context, including stable identifiers, names, ordering, and the active selection;
- the next context identifier, so identifiers remain monotonic after reopening;
- an optional supported ONNX network for each context; an intentionally empty context is valid;
- an optional CSV dataset reference per context, including input count, target count, and header policy;
- an optional training configuration per context with epoch count, learning rate, MSE loss, and SGD optimizer;
- every breakpoint definition and enabled state per context;
- every model checkpoint, including its stable identifier, name, network, and the next checkpoint identifier.

Dataset samples are not embedded. When possible, a stored CSV source is relative to the project file, making a project folder movable as one unit. An absolute source remains absolute. Each checkpoint network is embedded as its own ONNX payload, independently of the context's current network.

Version 2 intentionally does not persist:

- current training progress, retained step history, or worker state;
- an active phase-debug transaction or uncommitted candidate values;
- breakpoint hit counters or the last breakpoint hit;
- manually edited 2D/3D positions, camera state, or selection;
- per-user theme, refresh frequency, or detailed-rendering limits.

Those values are transient execution state or user preferences. Future tagged versions can add project metadata without moving presentation state into the mathematical network model.

## Binary container contract

All integer and IEEE-754 `double` values use little-endian byte order. Strings are UTF-8 and are prefixed by an unsigned 64-bit byte count.

The file header is:

| Field | Size | Version 2 value |
| --- | ---: | --- |
| Magic | 8 bytes | `MIAIPRJ\0` |
| Format version | 32-bit unsigned | `2` |
| Section count | 32-bit unsigned | Number of following sections |

Every section contains a four-byte ASCII tag, a 64-bit payload size, and exactly that many payload bytes. Version 2 is an ordered stream:

| Tag | Occurrence | Payload |
| --- | --- | --- |
| `PRJT` | Once, first | Active context ID, next context ID, and context count |
| `MODL` | Once per context | Context ID, name, next checkpoint ID, presence flags, and checkpoint count |
| `ONNX` | Optional after `MODL` | Current supported model network; presence is declared by `MODL` |
| `DATA` | Optional after the context network | CSV source, input count, target count, and one-byte header flag |
| `TRNG` | Optional after dataset metadata | Epoch count, learning rate, loss identifier, and optimizer identifier |
| `BRKP` | Once per context | Breakpoint count followed by identifier, enabled flag, kind, phase, target, and threshold for each definition |
| `CKPT` | Once per checkpoint | Checkpoint ID and name |
| `ONNX` | Once after each `CKPT` | Complete supported checkpoint network |

The flags in `MODL` determine whether the current-network `ONNX`, `DATA`, and `TRNG` sections occur. `BRKP` is present even when its count is zero. The declared checkpoint count determines how many `CKPT`/`ONNX` pairs follow before the next `MODL`.

Readers reject invalid sizes or counts, unsupported versions, unknown context flags, duplicate or out-of-range identifiers, empty names, malformed metadata, invalid enum values, malformed breakpoints, missing or out-of-order required sections, trailing bytes, or networks outside the supported ONNX subset. The complete top-level version must be explicitly supported; version 2 does not silently reinterpret unknown sections.

## Version 1 compatibility

Version 1 contains exactly one required `ONNX` section, optional `DATA` and `TRNG` sections, and one required `BRKP` section. It does not contain context metadata or checkpoints.

The current reader accepts version 1 and migrates it in memory to one model context named `Model 1` with ID `1`, active ID `1`, and next context ID `2`. Saving that project writes version 2. The Engine retains a version-1 writer only for compatibility fixtures and migration tests; normal SDK, CLI, Blueprint, and Studio saves always publish version 2.

## Save and open behavior

Saving requires a `.mai` path and at least one valid model context. A network is optional, but every network or checkpoint that is present must be exportable through the supported ONNX subset. All context names, identifiers, active selection, next identifiers, context-local metadata, and checkpoints are validated before publishing.

ONNX payloads are streamed through temporary files instead of being retained as duplicate in-memory byte arrays. The writer creates the complete archive in a sibling temporary file, flushes and closes it, then replaces the destination. A failed write therefore does not publish a partially written project.

Opening is transactional. Engine code parses and validates a complete independent `ProjectArchiveState` first. `MiaIAClient` replaces the current process-local `ProjectState` only after every context and checkpoint is accepted. A corrupt or unsupported project leaves the current state unchanged.

A missing referenced CSV is a recoverable condition rather than archive corruption. The affected context, training configuration, breakpoints, and checkpoints open normally; project information reports its dataset reference as unavailable when that context is active. Restoring the file at the recorded path and reopening the project restores the samples.

Training progress and phase debugging resume as idle state after opening. New, open, save, context-selection, and context-removal operations are rejected while background training is running or a phase-debug transaction is active. Pause or cancel first so replacement occurs at a safe boundary.

## Public access

The same project operations are exposed through every current client boundary:

- SDK: `NewProject`, `OpenProject`, `SaveProject`, and `GetProjectInfo`;
- CLI: `project new`, `project open`, `project save`, and `project info`;
- Unreal Blueprint: the `MiaIA|Project` category, with ONNX operations under `MiaIA|Interchange`;
- MiaIA Studio: the `Project` toolbar menu, including separate Import ONNX and Export ONNX actions.

Model-context management is exposed through:

- native SDK: `CreateModelContext`, `GetModelContexts`, `GetActiveModelContext`, `SelectModelContext`, `RenameModelContext`, and `RemoveModelContext`;
- shared CLI: `model create`, `model list`, `model select`, `model rename`, and `model remove`;
- Unreal Blueprint: the `MiaIA|Project|Model Context` category and `FMiaIAModelContext` snapshots;
- MiaIA Studio: the model-context selector shared by the Unreal Editor panel and standalone application.

Existing network, dataset, training, debug, and checkpoint operations always target the active model context. Project information reports the total context count and active identity; network availability and context-local counts describe the active context.

Hosts remain process-local. Opening a project in `Console.exe` does not change the separate project state owned by an Unreal Editor or standalone MiaIA Studio process.
