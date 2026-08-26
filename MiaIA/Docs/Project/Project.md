# MiaIA Project Format

## Purpose

`.mai` is the versioned MiaIA project format. The product, namespaces, libraries, APIs, and documentation remain named **MiaIA**.

ONNX remains the external model-interchange format. A `.mai` project owns the MiaIA-specific context around that model and can still export its representable model portion to ONNX.

## Version 1 contents

A `.mai` v1 project contains:

- one required ONNX model section containing topology, parameters, and supported layer activations;
- an optional CSV dataset reference with input count, target count, and header policy;
- an optional training configuration with epoch count, learning rate, MSE loss, and SGD optimizer;
- one required breakpoint section containing every breakpoint definition and enabled state.

The dataset samples are not embedded. When possible, the stored source is relative to the project file, making a project folder movable as one unit. An absolute source remains absolute.

Version 1 intentionally does not persist:

- additional runtime model instances or the active-model selection;
- process-local model checkpoint stores;
- current training progress, retained step history, or worker state;
- an active phase-debug transaction or uncommitted candidate values;
- breakpoint hit counters or the last breakpoint hit;
- manually edited 2D/3D positions, camera state, or selection;
- per-user theme, refresh frequency, or detailed-rendering limits.

Those values are either transient execution state or user preferences. Later format versions can introduce new tagged sections without changing the responsibility of the mathematical network model.

## Binary container contract

All integer and IEEE-754 `double` values use little-endian byte order. Strings are UTF-8 and are prefixed by an unsigned 64-bit byte count.

The file header is:

| Field | Size | Version 1 value |
| --- | ---: | --- |
| Magic | 8 bytes | `MIAIPRJ\0` |
| Format version | 32-bit unsigned | `1` |
| Section count | 32-bit unsigned | Number of following sections |

Every section contains a four-byte ASCII tag, a 64-bit payload size, and exactly that many payload bytes:

| Tag | Required | Payload |
| --- | --- | --- |
| `ONNX` | Yes | Complete supported ONNX model |
| `DATA` | No | Source string, input count, target count, and one-byte header flag |
| `TRNG` | No | Epoch count, learning rate, loss identifier, and optimizer identifier |
| `BRKP` | Yes | Breakpoint count followed by identifier, enabled flag, kind, phase, target, and threshold for each definition |

Readers reject duplicate known sections, invalid sizes, unsupported versions, invalid enum values, malformed breakpoints, trailing bytes, or a missing required section. Unknown tagged sections are skipped by payload size so a newer writer can add optional information without making the container impossible to scan. Semantic compatibility still requires an explicitly supported top-level format version.

## Save and open behavior

Saving requires a `.mai` path, exactly one runtime model instance, and a network that can be exported through the supported ONNX subset. A project containing multiple model instances is rejected rather than silently saving only the active model. The writer creates the complete archive in a sibling temporary file, flushes and closes it, then replaces the destination. A failed write therefore does not publish a partially written project.

Opening is transactional. Engine code parses and validates an independent network, dataset, training configuration, and breakpoint collection first. `MiaIAClient` replaces the current process-local state only after the complete archive is accepted. The replacement `ProjectState` contains one model with runtime ID `1` and default name `Model 1`; the loaded network, dataset, and training state belong to that model. A corrupt or unsupported project leaves the current state unchanged.

A missing or invalid referenced CSV is a recoverable condition rather than archive corruption. The model, training configuration, and breakpoints open normally; project information reports the dataset reference as unavailable. Restoring the file at the recorded path and reopening the project restores the dataset.

New, open, and save operations are rejected while background training is running or a phase-debug transaction is active. Pause or cancel the operation first so project replacement occurs at a safe boundary.

## Public access

The same operations are exposed through every current client boundary:

- SDK: `NewProject`, `OpenProject`, `SaveProject`, and `GetProjectInfo`;
- CLI: `project new`, `project open`, `project save`, and `project info`;
- Unreal Blueprint: the `MiaIA|Project` function category, with ONNX operations under `MiaIA|Interchange`;
- MiaIA Studio: the `Project` toolbar menu, including separate Import ONNX and Export ONNX actions.

Runtime multi-model management is currently available through the native SDK (`CreateModelInstance`, `GetModelInstances`, `GetActiveModelInstance`, `SelectModelInstance`, `RenameModelInstance`, and `RemoveModelInstance`) and the shared CLI (`model create`, `model list`, `model select`, `model rename`, and `model remove`). Existing operations always target the active model. Blueprint and the graphical Studio model selector have not yet been added.

Hosts remain process-local. Opening a project in `Console.exe` does not change the separate project state owned by an Unreal Editor or standalone MiaIA Studio process. Multi-model persistence requires a later `.mai` format version.
