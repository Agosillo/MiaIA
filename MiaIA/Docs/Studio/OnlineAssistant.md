# MiaIA Studio command assistant

## Scope

See the [Studio action inventory](ActionInventory.md) for the current GUI/CLI/intent
coverage and the planned, approval-gated expansion to view and navigation actions.

The command assistant belongs to MiaIA Studio, not to the mathematical Engine or
public SDK. It accepts natural text in the selected installed language, or mixed text in `Auto`, asks the selected provider to
identify one intent and its entities, converts that structured result into a validated
MiaIA Console command or allowlisted Studio view action, and displays the exact proposal for confirmation.

**MiaIA Local is the default provider.** Its default `Auto (all installed languages)`
mode evaluates the built-in English and Italian corpora plus every installed Language Pack in the same session. It runs offline, needs no account or token,
and sends no text outside the process. **Wit.ai is retained as an experimental online
provider** for comparison and optional use.

The assistant never executes a recognized intent directly. `Confirm command`, or an
exact local response of `confirm`, `yes`, `ok`, or `execute`, passes the displayed
command to the host dispatcher: model/data commands use the shared
`MiaIACommandProcessor`; the finite `studio ...` view-action catalog uses existing
Studio handlers. `Discard`, `cancel`, `no`, or `discard`
removes the proposal. Confirmation responses are always handled locally. The normal
Console remains available when the assistant is disabled.

`Confident mode (exact 100% only)` is a second, session-local opt-in. When enabled, a
locally valid proposal is executed immediately only if the intent confidence is
exactly `1.0` and every returned entity confidence is also exactly `1.0`. A displayed
`99.99%` remains subject to confirmation. The option starts disabled and is not
persisted. Provider confidence is statistical rather than proof, which is why this
behavior requires a separate explicit choice.

```text
Text in any installed language, including mixed text
    -> MiaIA Local (default) or Wit.ai (experimental)
    -> provider-neutral intent and entities
    -> local validated command proposal
    -> explicit user confirmation
    -> shared MiaIA command processor OR allowlisted Studio view handler
```

MiaIA Local uses no network. Wit.ai receives only the text entered while that provider
and the assistant are enabled; no dataset, checkpoint, model, or project content is
sent. Assistant state and credentials are not stored in `.mai` files.

## MiaIA Local provider

`LocalCommandAssistant` lives in `IDE/StudioCore` and depends only on the C++20
standard library. It has no Unreal, Windows, HTTP, JSON, or token dependency, so the
same library can be reused later by another Unreal target or platform.

The provider combines a built-in bilingual phrase corpus and installed Language Packs
with deterministic command rules. Intent recognition is separated from entity extraction: model names and
project paths remain text, while model IDs, topology sizes, epochs, learning rates,
seeds, and step counts are extracted into the existing validated roles. Unknown text
returns no intent and can never bypass `CommandAssistant::Propose`. Studio view
actions have a separate finite host-side allowlist; arbitrary commands cannot be
supplied by a language pack.

The automated native tests exercise the same English and Italian commands, a third
installed language, `Auto`, out-of-scope text, and all current entity shapes.

## Portable Language Packs

Assistant configuration and learning controls are in the scrollable left sidebar.
The right pane contains console history, the current recognized phrase/intent/command
and its Confirm/Discard/Wrong interpretation controls, followed by console input.
Console output wraps to the available pane width, including long unbroken paths.
The parallel **Settings** and **Review (count)** buttons keep configuration and
classification separate. Both start collapsed so examples remain visible. Settings
contains provider/language options, backups and language-pack management; Review
contains phrase classification only. A newly queued phrase opens Review and closes
Settings automatically. Either panel can be closed by pressing its button again.
Import/export, validation and configuration messages are written to console history, including the
path actually read when an import fails.

### Console and Assistant history

The **History** selector, immediately before the input field, controls only what **Up/Down** recalls:

- **Auto** (default): commands when the assistant is disabled, phrases when enabled.
- **Console**: directly submitted CLI commands.
- **Assistant**: submitted natural-language phrases.
- **All**: both in their original submission order.

Changing the recall filter does not change the input interpretation mode. Review a
recalled entry before submitting it. Generated CLI commands and bare confirmations
such as `ok` are not additional input-history entries.

**Output is always a single, continuous chronological log.** Switching input mode
or recall filter never hides existing output, results, errors or pending proposals.
Late provider replies are logged but cannot execute or replace a newer request.

Each input mode retains its draft and recall position. Down after the newest entry
restores the draft. Changing the recall filter restarts navigation from the newest
matching entry, preserving the current input as its draft.

**Clear output** explicitly clears the entire output log, not the input histories,
drafts, learned phrases or model state. History is session-local and is not exported
with projects or language packs.

Language Packs add a third, fourth, or later language without recompiling Studio and
without creating an online account. Open **Settings** and choose **Export language
template**. Studio writes the editable UTF-8 tab-separated file:

`Saved/MiaIA/CommandAssistant/language-template.miaia-language`

Edit the `L` row by replacing `xx` and `Language name`, then translate the last column
of any `E` rows you want to support. Keep the intent and English source columns
unchanged. Empty translations are allowed and are skipped, so a language can be
introduced incrementally. For example:

```text
MIAIA_LOCAL_LANGUAGE_PACK	2
L	es	Espanol
E	miaia_model_list	Show all models	Mostrar todos los modelos
E	miaia_neuron_inspect	Inspect neuron 2	Inspeccionar neurona 2
```

In Settings, **Browse...** opens a portable in-app file browser. Navigate folders
or enter a folder path (including another drive) and press Enter, then select a
`.miaia-language` file. Selecting a file does not install it. The full file path
also remains editable manually.

Choose **Preview and install language pack** to validate the selected file. Before
any write, a confirmation shows its name/code, translated phrase count, source and
destination paths, up to three examples, and whether it replaces an existing pack.
Answering No leaves installed packs, the provider and the current proposal unchanged.
The validated snapshot shown in the preview is the snapshot installed. Save your
changes in the text editor before installing. Do not export again to install: export
creates a new blank template. The literal `xx` / `Language name` placeholders must
both be replaced. Use real TAB characters, not spaces or the two characters `\t`.
Translations belong in column **four**, not in the English source column. An empty
fourth column is skipped (including when an editor removes its trailing TAB).

A valid pack is stored at `Saved/MiaIA/CommandAssistant/Languages/<code>.miaia-language`, selected
immediately, added to the language menu, shown in the example sidebar, and loaded on
later starts. Installing a pack clears any old proposal and disables the assistant;
enable it again before testing the new language. Installing the same code asks before replacing that pack. To add another
language, export a fresh template and repeat the process. Export also asks before
overwriting an existing work-in-progress template.

**Installed language packs (count)** lists names, codes, translated phrase counts and
parameter synonym counts. **Export editable pack...** includes its current phrases
and synonyms, plus blank parameter rows for further translation.
Each additional pack has **Remove...**, with a confirmation. English and Italian are
built in and cannot be removed. Removal moves the verified installed file to
`Saved/MiaIA/CommandAssistant/Languages/Removed/<code>-<unique-id>.miaia-language`;
this folder is not loaded automatically. Browse to that file and reinstall to restore
it. The console reports the recovery path. The language menu and examples refresh
immediately; removing the selected language switches selection to Auto. The assistant
is disabled and pending execution proposals are cleared after removal.

Removal does not delete exported templates or learned/pending phrases in
`local-corpus.miaia`. These are managed separately through Review, and learned phrases
can therefore still be recognized in Auto even after their original pack is removed.

Language codes contain 2-24 lowercase letters, digits, or hyphens and begin with a
letter; the `xx` placeholder and `auto` are reserved. Version 2 also accepts `en` and
`it` supplements without replacing their built-in corpora. A pack must contain at
least one translated phrase or parameter synonym and can only reference the compiled
catalog of supported intents and roles. Invalid,
oversized, malformed, or unsupported entries are rejected atomically. Exact imported
phrases are recognized at `100%`; similar phrases still require review. Inspection
numbers retain their command-specific positional order. Network creation instead
requires labelled topology values, as described below. Existing version 1 packs
remain importable; newly exported packs use version 2. Parameter vocabulary can now
be translated with `P` records, including markers for quoted model names and project
paths. A translated intent example alone still does not teach parameter labels.

The Language Pack is intentionally separate from the learned-corpus backup. A pack is
a reusable translation of built-in examples; `local-corpus.backup.miaia` contains one
user's validated and pending phrases. Both formats live in portable `StudioCore`
memory APIs while Unreal owns the file access, so the same packs can be reused by a
future Unreal target or another C++ host.

## Editable and exportable parameter synonyms

Choose the language, then **Settings > Export parameter synonyms**. In Auto, this
exports an English supplement; choose Italian or another language first to edit that
language. Studio writes `Saved/MiaIA/CommandAssistant/parameters-<code>.miaia-language`
and selects it as the import path. Existing installed phrases and aliases are retained
in the export. Export does not change the active vocabulary.

Edit the file in a UTF-8 text editor, keeping real TAB separators:

```text
MIAIA_LOCAL_LANGUAGE_PACK	2
L	en	English
P	miaia_network_create	hidden_width	neurons per hidden layer	units per tier
P	miaia_network_create	hidden_layers	hidden layers	depth
P	miaia_network_create	weight	initial weight	connection start
P	miaia_training_start	learning_rate	learning rate	speed
P	miaia_model_create	model_name	name	title
P	miaia_project_save	project_path	path	destination
```

Each `P` row has five columns: record type, intent, parameter role, source label,
and editable synonym. **Only column 5 is the synonym**; columns 2 and 3 identify its
meaning. Duplicate a row to add another synonym. Empty fifth columns are skipped.
`E` rows still have four columns and translate whole intent examples. Preserve them
when editing an existing pack. A parameter-only pack is valid, including for `en`
and `it`. Supplements add vocabulary; they do not delete built-in labels.

Save and choose **Preview and install language pack**. The preview shows both counts
and sample mappings before asking to replace an installed pack. Enable the assistant
again after installation. To change a custom association, edit its role/synonym row
and reinstall the complete pack. To delete a custom synonym, remove its `P` row and
reinstall; to remove the entire supplement, use **Installed language packs > Remove...**.
The existing recoverable removal workflow applies; built-in English/Italian and the
learned corpus remain untouched. Do not export a fresh blank language template over
the file you are editing.

Synonyms are scoped to **intent + role + pack language**. Auto accepts all installed
vocabulary, including mixed phrases. The same label cannot name two different roles
within an intent, even across installed languages: import rejects that ambiguity
before replacing the file. Long labels take precedence over their shorter prefixes,
so `neurons per layer` is not confused with `layer`. Synonyms are literal text, not
regular expressions; case-insensitive matching covers ASCII, with UTF-8 labels
preserved. Types, numeric limits, activation choices and command syntax are not
editable in packs. A pack accepts at most 256 distinct parameter aliases.

Examples after installing the sample supplement:

```text
Create a network with 2 inputs, 4 units per tier, 0 depth, 1 output, connection start -0.25
Start training epochs 200 speed 0.005
Create a model title "Vision Model 2"
Save project destination "C:\My Projects\model 2.mai"
```

When using custom parameter labels, label **all** supplied values. Do not mix a new
label with positional numbers whose roles would have to be guessed. Put text values
after their label and quote names/paths, especially when they contain spaces or
parameter-like words. Exact learned intent matches still extract values afresh and
validate them: editing/removing a synonym also affects already learned phrases.

The shared `AssistantParameterCatalog.h` covers every parameter role of currently
exposed intents: network creation, model creation/selection, project paths, training
session start/run and inspection/comparison, including dataset diagnostic options.
It is designed to grow with **every console command**, not to be a four-number
network parser. Commands not yet exposed as intents (such as input vectors, backward
trace and editing individual network elements) still require the console. Their
typed schemas and intent exposure belong to the next coverage steps; installing a
pack does not make them available prematurely.

The dependency-free regression suite is shared with the full native tests. From a
Visual Studio x64 Native Tools command prompt, at the repository root:

```bat
if not exist MiaIA\x64\Release\AssistantChecks mkdir MiaIA\x64\Release\AssistantChecks
cl /nologo /std:c++20 /EHsc /utf-8 /W4 /FoMiaIA\x64\Release\AssistantChecks\ /FeMiaIA\x64\Release\AssistantChecks\AssistantTests.exe MiaIA\Tests\CommandAssistantStandalone.cpp MiaIA\IDE\StudioCore\Source\CommandAssistant.cpp MiaIA\IDE\StudioCore\Source\LocalCommandAssistant.cpp
MiaIA\x64\Release\AssistantChecks\AssistantTests.exe
```

This runs without Unreal, ONNX or an online service. The standard `Tests` project
calls the identical suite, including legacy EN/IT requests, pack v1 compatibility,
v2 replacement/round-trip/conflict tests, mixed languages, all eight `create`
parameters, signed/finite validation and diagnostic option limits.

## Supervised local learning

MiaIA Local can extend its corpus without changing the compiled command allowlist.
When a proposal below exact `100%` confidence is accepted with **Confirm command**,
the source phrase and provider-neutral intent are stored as a validated example. The
same normalized phrase then matches at `100%`; the raw proposed command is never
stored or replayed directly, and every future execution still passes through
`CommandAssistant::Propose` and the appropriate allowlisted host dispatcher.

An unrecognized phrase is saved in the local **to classify** queue but does not affect
recognition. Expanding **Review** changes the left Console sidebar into **Local
learning**: its **To classify** and **Validated** sections list every user phrase and
show their counts. Selecting an item edits it in the left configuration panel. A pending phrase can be
assigned one of the supported intents with **Validate**; a validated phrase exposes
its current intent and can be corrected with **Change intent**. **Delete** removes the
selected item. **Wrong interpretation** removes any matching learned example, opens
**Review**, and selects the rejected phrase immediately; older unknown phrases remain
in the queue. **Discard** only cancels the current proposal and deliberately teaches
nothing.

The same left sidebar follows the current input mode. With the assistant disabled it
shows exact Console **Commands**. With the assistant enabled it shows clickable
**Assistant examples** for the selected language; clicking copies a natural-language
example into the input without executing it. `Auto` shows every installed language.
Opening **Review** temporarily replaces those examples with the local corpus manager;
closing it restores the appropriate command or assistant-example view.

**Export backup** writes a portable snapshot named
`local-corpus.backup.miaia` beside the active corpus. **Import backup** asks for
confirmation before replacing the active learning data, while **Reset learned data**
asks for confirmation before deleting all validated and pending user phrases. Reset
does not remove built-in intents, rules, or assistant examples.

Classification associates the phrase with an intent, not with a stored executable
command. Entities are extracted again from the phrase whenever it is interpreted. A
custom network topology needs all four integer values, labelled as
**inputs, neurons per hidden layer, hidden layers, outputs**, in any order. For example:
`Create a network with 1 output, 3 hidden layers, 2 inputs and 4 neurons per hidden layer`
becomes `create 2 4 3 1`. Labels before numbers also work, such as `inputs: 2` or
`hidden_width: 4`. Inputs, hidden width and outputs must be positive; **hidden layers
may be zero**, as in the engine. Counts must fit the console's integer range.
English and Italian are built in (`neuroni per layer`, `layer hidden`, and `layer`
are also accepted); other labels come from packs. Numbers must be digits, not
spelled-out words.

The four optional `create` parameters are also supported: `hidden_activation` and
`output_activation` accept `sigmoid`, `relu`, `tanh`, `linear`; `weight` and `bias`
accept finite signed numbers, including zero, dot decimals and scientific notation.
Use distinct labels for hidden/output activation. For example:

```text
Crea una rete 2 input 4 neuroni per layer 0 layer hidden 1 output attivazione hidden relu attivazione output linear peso iniziale -0.25 bias 0
```

proposes `create 2 4 0 1 --hidden-activation relu --output-activation linear --weight -0.25 --bias 0`.
Optional settings require a complete explicit topology, matching the CLI syntax;
`create` alone retains its existing defaults. Decimal commas are rejected rather
than silently reinterpreted. No activation names are automatically translated.

`Create a network` intentionally uses Console defaults. Partial, unlabelled,
duplicate, extra or invalid parameters block the proposal and request
clarification. The user must repeat the complete corrected phrase; no conversational
parameter state or automatic completion is stored yet. A recognized intent with a
parameter error is not added to the unknown-intent Review queue. Learning an intent
at 100% does not bypass parameter validation. Exact CLI `create 2 4 3 1` remains
available with the assistant disabled; positional natural-language guessing is removed.

The versioned corpus is stored outside `.mai` project archives under the Unreal
Saved directory at `Saved/MiaIA/CommandAssistant/local-corpus.miaia`. `StudioCore`
only imports and exports portable in-memory corpus text; the Unreal host owns file
access, preserving the provider's independence from Windows and Unreal APIs. English,
Italian, installed custom languages, and automatically detected mixed examples retain their language metadata.
Malformed, oversized, unsupported, or newer-format corpus data is ignored without
replacing the active in-memory corpus.

## Build and Visual Studio

The integration is part of a normal MiaIA Studio build. The provider-neutral command
translation and MiaIA Local provider live in the native `StudioCore` library. The
optional HTTP provider and the provider selector live in Unreal. `IDEStudio.Build.cs`
defines `MIAIA_WITH_WIT_AI=1` by default and links Unreal's HTTP and JSON modules for
the experimental provider. After changing either assistant layer, first open
`MiaIA.sln` and build
`Release | x64`; confirm that `MiaIA/x64/Release/StudioCore.lib` was updated. Then open
`IDE.sln`, select `Development` and `Win64`, and make the generated `IDE` project the
startup project. That solution configuration maps the project to the standalone
`MiaIAStudio` target. Use **Build > Rebuild Solution**, then start it with **Debug >
Start Debugging**. If Visual Studio still has an older object, use **Build > Clean
Solution** once before rebuilding. Building `StudioCore.vcxproj` directly writes to a
project-local output directory and does not replace the central library linked by
Unreal; build it through `MiaIA.sln`.

An intentionally restricted Unreal build can exclude the current assistant panel and
its Wit.ai dependencies by setting `MIAIA_WITH_WIT_AI=0` before Unreal Build Tool
runs. The portable local provider remains part of `StudioCore`. The normal Windows
packaging script includes the panel unless `-DisableWitAI` is passed:

```powershell
& .\Build\Package-Windows.ps1 -Configuration Development
& .\Build\Package-Windows.ps1 -Configuration Development -DisableWitAI
```

## Experimental Wit.ai client-token configuration

Use **Client Access Token**, not **Server Access Token**. Meta's Unreal runtime uses
the client token for application requests; a server token has administrative scope
and must never be entered into or shipped with MiaIA Studio.

The assistant controls appear at the top of the Console output area because they
change how Console input is interpreted. Choose **Wit.ai (experimental)** to reveal
**Settings** in Development builds, then
enter the English and/or Italian Client Access Token, then select
**Save tokens**. The values are stored as per-user Studio settings outside `.mai`
projects and take effect without recompiling or restarting Studio. Replacing a token
also changes the private Wit.ai application used for that language because the token
identifies its app. Saving an empty field removes that developer override and restores
the next available default.

Shipping and Microsoft Store builds hide **Settings** and both credential fields.
Customers see only the language selector, explicit online-assistant opt-in, status,
proposal, and confirmation controls. Shipping ignores Development user overrides and
uses its packaged defaults. Rotating a token or changing a private app therefore means
packaging a new release with the replacement Client Tokens; it does not require a
source-code change.

Development token resolution has this precedence:

1. the token saved by the current user in Assistant settings;
2. a packaged `MiaIAAssistant.ini` default;
3. the development environment variables `MIAIA_WIT_TOKEN_EN` and
   `MIAIA_WIT_TOKEN_IT` (`MIAIA_WIT_TOKEN` remains an English-only fallback).

Environment variables remain useful for a temporary local test, but they are no
longer required for Visual Studio compilation:

```powershell
$env:MIAIA_WIT_TOKEN_EN = "YOUR_ENGLISH_CLIENT_ACCESS_TOKEN"
$env:MIAIA_WIT_TOKEN_IT = "YOUR_ITALIAN_CLIENT_ACCESS_TOKEN"
```

Never add real tokens to source, committed Unreal configuration, documentation,
screenshots, logs, or a `.mai` archive.

## Release packaging

For a release whose users should not have to supply their own Wit.ai accounts, set
both Client Access Tokens only in the PowerShell process that launches the packaging
script:

```powershell
$env:MIAIA_WIT_TOKEN_EN = "YOUR_ENGLISH_CLIENT_ACCESS_TOKEN"
$env:MIAIA_WIT_TOKEN_IT = "YOUR_ITALIAN_CLIENT_ACCESS_TOKEN"

& .\Build\Package-Windows.ps1 `
    -Configuration Shipping `
    -OutputDirectory "D:\MiaIA-Releases\Windows-Shipping"
```

The script creates an untracked `MiaIAAssistant.ini` beside both the bootstrap and
runtime executables in the archive. `Package-StoreMsix.ps1` copies the runtime file
into the Store package. Shipping does not expose or apply the Development token
editor. Rotating a token or moving to another Wit.ai app requires a new package, but
does not require changing MiaIA source code.

Client credentials distributed in any desktop package are recoverable by a determined
user. This is compatible with their client-runtime role, but it also permits quota
abuse. A future server-side proxy is the stronger production boundary if usage grows
or tighter revocation, rate limiting, and monitoring are required.

This slice interprets typed text only. It does not yet capture microphone audio;
microphone capture and speech transcription are a separate follow-up that will feed
the same reviewed command-proposal path.

## Wit.ai application

Create one private application per language:

| Name | Language | Visibility |
| --- | --- | --- |
| `miaia_studio_assistant_en` | English | Private |
| `miaia_studio_assistant_it` | Italian | Private |

The Studio selector chooses the matching application and disables the online assistant
when the language changes, requiring an explicit re-enable. The local adapter calls
`GET https://api.wit.ai/message` with API version `20260908` and OAuth bearer
authentication. Copy each private application's Client Access Token into Assistant
settings or the release-only packaging environment described above.

## Studio view and layout actions

The following actions reuse existing Studio controls. No new toolbar controls are
introduced; bilingual examples appear in the existing assistant example sidebar. While typing,
the sidebar shows phrases containing the normalized input text (case and whitespace
insensitive), including installed-pack examples. Empty input shows all examples for
the selected language. Typing changes widget visibility instead of rebuilding the
example list; provider/language/pack changes still refresh its contents.
The existing manual confirmation and exact-100%-only Confident mode apply unchanged.

| Intent | Proposed Studio command | English / Italian example |
| --- | --- | --- |
| `miaia_studio_view_2d` | `studio view 2d` | Switch to 2D / Passa alla vista 2D |
| `miaia_studio_view_3d` | `studio view 3d` | Switch to 3D / Passa alla vista 3D |
| `miaia_studio_layout_expanded` | `studio layout expanded` | Set layout expanded / Imposta layout expanded |
| `miaia_studio_layout_packed` | `studio layout packed` | Set layout packed / Imposta layout packed |
| `miaia_studio_labels_show` | `studio labels show` | Show neuron labels / Mostra le etichette dei neuroni |
| `miaia_studio_labels_hide` | `studio labels hide` | Hide neuron labels / Nascondi le etichette dei neuroni |
| `miaia_studio_connections_show` | `studio connections show` | Show connections / Mostra le connessioni |
| `miaia_studio_connections_all` | `studio connections all` | Show all connections / Mostra tutte le connessioni |
| `miaia_studio_connections_selected` | `studio connections selected` | Show only selected connections / Mostra solo le connessioni selezionate |
| `miaia_studio_connections_hide` | `studio connections hide` | Hide connections / Nascondi le connessioni |
| `miaia_studio_fit_view` | `studio view fit` | Fit view / Adatta la vista |
| `miaia_studio_reset_layout` | `studio layout reset` | Reset layout / Ripristina il layout |

These are explicit actions, not toggles: “hide connections” always hides them.
The twelve fixed intents accept no extra entities or numeric arguments. Translate their
complete examples in exported language packs; there are no free parameter slots for
these fixed actions. Supervised reassignment/learning uses the same intent catalog.
Constrained English/Italian rules also accept mixed phrases such as “mostra neuron
labels” and “nascondi connections”. Negated, conflicting or compound non-exact requests
are not silently reduced to one view action; repeat a single explicit request.

The exact commands can also be typed into Studio with the assistant disabled.
They are host commands, not Engine/SDK operations and not part of the 74-entry shared
CLI catalog. Console.exe has no view host and rejects them as unknown commands.
Additional tokens or unknown `studio` commands in Studio produce an error and no action.

Existing GUI side effects are preserved: entering 3D fits its view; choosing Packed
from Coaxial Rings or Spiral Tokens returns to Classic. Labels and connection
visibility use the same saved preferences as the checkboxes. Showing connections
does not reset the separate visibility percentage or All/Selected setting.
“Show all connections” enables connections and selects All; “Show only selected
connections” enables them and selects Selected. Neither changes the percentage.
With Selected and no selected neuron/connection, no edges are drawn; a zero
visibility percentage also keeps edges invisible.
Fit and Reset operate on the active renderer; Reset layout uses the current 2D
layout reset / 3D reset-view behavior, **not** Reset visualization defaults.
Compact views retain their existing rendering restrictions. No network parameters,
training state or mathematical topology are changed.

## Supported intents

Use the following exact intent names:

| Provider-neutral intent | Proposed MiaIA command |
| --- | --- |
| `miaia_help` | `help` |
| `miaia_model_list` | `model list` |
| `miaia_model_create` | `model create "<model_name>"` |
| `miaia_model_select` | `model select <model_id>` |
| `miaia_network_create` | `create` (Console defaults) or `create <inputs> <hidden_width> <hidden_layers> <outputs>` |
| `miaia_training_start` | `train session start <epochs> <learning_rate> mse [shuffle <seed>]` |
| `miaia_training_run` | `train session run <steps|all>` |
| `miaia_training_pause` | `train session pause` |
| `miaia_training_resume` | `train session resume` |
| `miaia_training_status` | `train session status` |
| `miaia_project_new` | `project new` |
| `miaia_project_open` | `project open "<project_path>"` |
| `miaia_project_save` | `project save ["<project_path>"]` |

MiaIA Local additionally supports these inspection intents. They appear in the
learning manager and in the language-aware sidebar examples; the Wit.ai archive generator
still supplies the original 13 intents.

| Local inspection intent | Proposed command |
| --- | --- |
| `miaia_project_info` | `project info` |
| `miaia_network_summary` | `summary` |
| `miaia_network_inspect` | `inspect` |
| `miaia_neuron_inspect` | `inspect neuron <neuron_id> [maximum_connections]` |
| `miaia_connection_inspect` | `inspect connection <connection_id>` |
| `miaia_model_compare` | `model compare <reference_id> <current_id> [maximum_items]` |
| `miaia_checkpoint_list` | `checkpoint list` |
| `miaia_checkpoint_inspect` | `checkpoint inspect <checkpoint_id>` |
| `miaia_checkpoint_compare` | `checkpoint compare <first_id> <second_id> [maximum_items]` |
| `miaia_dataset_summary` | `dataset summary` |
| `miaia_dataset_inspect` | `dataset inspect <sample_index>` |
| `miaia_dataset_diagnose` | `dataset diagnose [sample_limit]` |
| `miaia_training_history` | `train session history` |
| `miaia_training_inspect` | `train session inspect <step_index>` |
| `miaia_training_compare` | `train session compare <first_step_index> <second_step_index> [maximum_items]` |
| `miaia_debug_status` | `train debug status` |
| `miaia_breakpoint_list` | `train breakpoint list` |

For example, `Ispeziona neurone 2` proposes `inspect neuron 2`, and
`Compare models 1 and 2 limit 5` proposes `model compare 1 2 5`.
Numbers follow the argument order shown above. Sample and training-step indices
start at zero; IDs and optional limits must be positive integers. Missing,
negative, fractional, overflowing, or extra numeric arguments are rejected.
Existence of an ID and availability of a dataset/session are checked by the Console
when the proposal is executed. Diagnosis currently exposes the default settings
and optional sample limit; its advanced thresholds remain Console options.

Configure the original Wit.ai number entities with the following roles:

- `model_id`;
- `inputs`;
- `hidden_width`;
- `hidden_layers`;
- `outputs`;
- `epochs`;
- `learning_rate`;
- `seed`;
- `steps`.

Configure text or keyword entities named `model_name`, `project_path`, and
`sample_order`. The supported `sample_order` values are `sequential`, `shuffle`, and
`shuffled`. The `steps` entity may also use the keyword value `all`.

Representative training utterances are:

- `Create a model named XOR experiment`;
- `Select model 2`;
- `Create a network with 2 inputs, 4 hidden neurons, 3 hidden layers and 1 output`;
- `Start training for 20 epochs at learning rate 0.01 with shuffle seed 42`;
- `Run all remaining training steps`;
- `Pause training`;
- `Resume training`;
- `Show training status`;
- `Open project C:\Models\xor.mai`;
- `Save the project`.

Use the same intent and entity names in the Italian application, with Italian examples
such as:

- `Crea un modello chiamato Esperimento XOR`;
- `Seleziona il modello 2`;
- `Crea una rete con 2 ingressi, 4 neuroni nascosti, 3 livelli nascosti e 1 uscita`;
- `Avvia 20 epoche con learning rate 0.01 in ordine casuale e seed 42`;
- `Esegui tutti i passi di training rimanenti`;
- `Metti in pausa il training`;
- `Riprendi il training`;
- `Mostra lo stato del training`;
- `Apri il progetto C:\Models\xor.mai`;
- `Salva il progetto`.

Italian `sample_order` may resolve to `sequenziale`, `casuale`, `mescola`,
`mescolato`, or `mischia`; `tutti` and `tutto` are accepted for all remaining steps.
Typed Italian confirmation accepts `conferma`, `confermo`, `si`, `sì`, `ok`, or `esegui`, while
`annulla`, `no`, and `scarta` reject the proposal locally.

Label each complete utterance with its intent and label the variable spans with the
matching entity or number role. Add several differently worded English examples per
intent before relying on recognition. Studio rejects unknown intents, malformed or
missing values, unsafe quoted text, and intent confidence below `0.70`.

## Reproducible Wit.ai imports

`Tools/WitAI/Build-WitImports.ps1` generates complete, token-free Wit.ai backup
archives for both supported languages. Each archive contains the original 13 intents listed
above, 10 utterances per intent, 12 Out of Scope examples, the required entity roles,
and the `Europe/Rome` timezone inherited from the reference applications.

Run from the repository root:

```powershell
& .\MiaIA\Tools\WitAI\Build-WitImports.ps1
```

The generated archives are written to the ignored artifact directory:

```text
MiaIA/Artifacts/WitAI/en_miaia_studio_assistant_complete.zip
MiaIA/Artifacts/WitAI/it_miaia_studio_assistant_complete.zip
```

The `complete` archives include entity definitions and annotated parameter spans.

Use **New App > Import From a Backup** in Wit.ai and select the archive for the
new application's language. Importing creates a new Wit.ai application; it does not
reuse an App ID or either access token. After import, review the entity roles and wait
for **Training complete** before testing `/message` or copying the new Client Access
Token into Studio.

## Troubleshooting

- `HTTP 401` means the token is invalid, expired, or not a Client Access Token for
  the selected language app.
- `intent: none, confidence: 0.00%` means the HTTP request succeeded but that Wit.ai
  app returned no intent. Confirm that the selected Studio language contains the
  Client Access Token from the matching app, open that app's **Understanding** page,
  assign the exact `miaia_*` intent to the utterance, and complete **Train and
  Validate**. The same phrase should return the intent in Wit.ai's **Try the HTTP
  API** field before it can work in Studio.
- An unsupported-intent message means Wit.ai returned an intent name that is not in
  the table above; correct its name in both language apps rather than bypassing local
  validation.
- Studio never logs or displays a token. Diagnostic output contains only the HTTP
  status, provider error text, recognized intent, and confidence.

## Planned expansion path

The next command-assistant tranche extends the local multi-language catalog from the current
30 intents to the rest of the shared Console and then to Studio-only interface actions.
Those actions must retain the same proposal, validation, confirmation, and automated
test boundary. A later microphone path (`whisper.cpp`, `sherpa-onnx`, or another
speech-to-text adapter) will feed its transcription into the same local provider; it
will not change command execution or the Engine.
