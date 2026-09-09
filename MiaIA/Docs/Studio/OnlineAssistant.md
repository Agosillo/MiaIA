# MiaIA Studio command assistant

## Scope

The command assistant belongs to MiaIA Studio, not to the mathematical Engine or
public SDK. It accepts natural English or Italian text, asks the selected provider to
identify one intent and its entities, converts that structured result into an existing
MiaIA Console command, and displays the exact proposal for confirmation.

**MiaIA Local is the default provider.** It runs offline, needs no account or token,
and sends no text outside the process. **Wit.ai is retained as an experimental online
provider** for comparison and optional use.

The assistant never executes a recognized intent directly. `Confirm command`, or an
exact local response of `confirm`, `yes`, `ok`, or `execute`, passes the displayed
command to the shared `MiaIACommandProcessor`. `Discard`, `cancel`, `no`, or `discard`
removes the proposal. Confirmation responses are always handled locally. The normal
Console remains available when the assistant is disabled.

`Confident mode (exact 100% only)` is a second, session-local opt-in. When enabled, a
locally valid proposal is executed immediately only if the intent confidence is
exactly `1.0` and every returned entity confidence is also exactly `1.0`. A displayed
`99.99%` remains subject to confirmation. The option starts disabled and is not
persisted. Provider confidence is statistical rather than proof, which is why this
behavior requires a separate explicit choice.

```text
English or Italian text
    -> MiaIA Local (default) or Wit.ai (experimental)
    -> provider-neutral intent and entities
    -> local validated command proposal
    -> explicit user confirmation
    -> shared MiaIA command processor
```

MiaIA Local uses no network. Wit.ai receives only the text entered while that provider
and the assistant are enabled; no dataset, checkpoint, model, or project content is
sent. Assistant state and credentials are not stored in `.mai` files.

## MiaIA Local provider

`LocalCommandAssistant` lives in `IDE/StudioCore` and depends only on the C++20
standard library. It has no Unreal, Windows, HTTP, JSON, or token dependency, so the
same library can be reused later by another Unreal target or platform.

The provider combines a small bilingual phrase corpus with deterministic command
rules. Intent recognition is separated from entity extraction: model names and
project paths remain text, while model IDs, topology sizes, epochs, learning rates,
seeds, and step counts are extracted into the existing validated roles. Unknown text
returns no intent and can never bypass `CommandAssistant::Propose` or the shared
`MiaIACommandProcessor` allowlist.

The automated native tests exercise the same English and Italian commands, including
out-of-scope text and all current entity shapes. Add a paired English/Italian test
whenever the local corpus or a supported command is extended.

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

Configure number entities with the following roles:

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
archives for both supported languages. Each archive contains all 13 intents listed
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

The next command-assistant tranche extends the bilingual local corpus from the current
13 intents to the rest of the shared Console and then to Studio-only interface actions.
Those actions must retain the same proposal, validation, confirmation, and automated
test boundary. A later microphone path (`whisper.cpp`, `sherpa-onnx`, or another
speech-to-text adapter) will feed its transcription into the same local provider; it
will not change command execution or the Engine.
