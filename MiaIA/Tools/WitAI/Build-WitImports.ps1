param(
    [string]$OutputRoot = (Join-Path $PSScriptRoot "..\..\Artifacts\WitAI")
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$expectedOutputRoot = [System.IO.Path]::GetFullPath(
    (Join-Path $PSScriptRoot "..\..\Artifacts\WitAI"))
$OutputRoot = [System.IO.Path]::GetFullPath($OutputRoot)
if ($OutputRoot -cne $expectedOutputRoot) {
    throw "OutputRoot must resolve to $expectedOutputRoot"
}

$IntentNames = @(
    "miaia_help",
    "miaia_model_list",
    "miaia_model_create",
    "miaia_model_select",
    "miaia_network_create",
    "miaia_training_start",
    "miaia_training_run",
    "miaia_training_pause",
    "miaia_training_resume",
    "miaia_training_status",
    "miaia_project_new",
    "miaia_project_open",
    "miaia_project_save"
)

function New-EntityPart {
    param(
        [Parameter(Mandatory)] [string]$Entity,
        [Parameter(Mandatory)] [string]$Text
    )

    return [pscustomobject]@{
        Entity = $Entity
        Text = $Text
    }
}

function New-Utterance {
    param(
        [AllowNull()] [string]$Intent,
        [Parameter(Mandatory)] [object[]]$Parts
    )

    $text = ""
    $entities = @()

    foreach ($part in $Parts) {
        if ($part -is [string]) {
            $text += $part
            continue
        }

        $start = $text.Length
        $text += $part.Text
        $entities += [ordered]@{
            entity = $part.Entity
            start = $start
            end = $text.Length
            body = $part.Text
            entities = @()
        }
    }

    $utterance = [ordered]@{
        text = $text
        entities = $entities
        traits = @()
    }

    if (-not [string]::IsNullOrWhiteSpace($Intent)) {
        $utterance["intent"] = $Intent
    }

    return $utterance
}

function New-PlainUtterance {
    param(
        [AllowNull()] [string]$Intent,
        [Parameter(Mandatory)] [string]$Text
    )

    return New-Utterance -Intent $Intent -Parts @($Text)
}

function Add-PlainGroup {
    param(
        [Parameter(Mandatory)] [AllowEmptyCollection()] [System.Collections.ArrayList]$Target,
        [AllowNull()] [string]$Intent,
        [Parameter(Mandatory)] [string[]]$Texts
    )

    foreach ($text in $Texts) {
        [void]$Target.Add((New-PlainUtterance -Intent $Intent -Text $text))
    }
}

function Add-Utterance {
    param(
        [Parameter(Mandatory)] [AllowEmptyCollection()] [System.Collections.ArrayList]$Target,
        [Parameter(Mandatory)] [string]$Intent,
        [Parameter(Mandatory)] [object[]]$Parts
    )

    [void]$Target.Add((New-Utterance -Intent $Intent -Parts $Parts))
}

function Assert-Utterances {
    param([Parameter(Mandatory)] [System.Collections.ArrayList]$Utterances)

    $seen = @{}

    foreach ($utterance in $Utterances) {
        $key = $utterance.text.ToLowerInvariant()
        if ($seen.ContainsKey($key)) {
            throw "Duplicate utterance: $($utterance.text)"
        }
        $seen[$key] = $true

        foreach ($entity in $utterance.entities) {
            if ($entity.start -lt 0 -or $entity.end -le $entity.start -or
                $entity.end -gt $utterance.text.Length) {
                throw "Invalid entity range in: $($utterance.text)"
            }

            $body = $utterance.text.Substring(
                $entity.start,
                $entity.end - $entity.start)
            if ($body -cne $entity.body) {
                throw "Entity body mismatch in: $($utterance.text)"
            }
        }
    }
}

function Get-EnglishUtterances {
    $items = [System.Collections.ArrayList]::new()

    Add-PlainGroup $items "miaia_help" @(
        "Show help", "Help me", "Display command help", "List available commands",
        "What can I do?", "What commands are available?", "Open the command guide",
        "Explain the console commands", "How do I use MiaIA?", "Show me the command list"
    )
    Add-PlainGroup $items "miaia_model_list" @(
        "Show all models", "Show models", "List all models", "Show the available models",
        "Which models are available?", "Display model contexts", "Let me see the models",
        "What models are in this project?", "Display every model context", "Give me all models"
    )

    foreach ($name in @("Atlas", "Iris", "Experiment One", "Classifier", "Vision Model", "Sandbox", "Baseline", "Candidate A", "Demo Network", "Research Model")) {
        Add-Utterance $items "miaia_model_create" @("Create a model named ", (New-EntityPart "model_name:model_name" $name))
    }

    foreach ($id in 1..10) {
        Add-Utterance $items "miaia_model_select" @("Select model ", (New-EntityPart 'wit$number:model_id' ([string]$id)))
    }

    $networkValues = @(
        @(2, 4, 1, 1), @(4, 8, 2, 2), @(10, 32, 2, 3), @(8, 16, 3, 4), @(3, 12, 2, 1),
        @(16, 64, 4, 8), @(6, 24, 3, 2), @(12, 48, 2, 6), @(5, 20, 1, 5), @(20, 40, 5, 10)
    )
    foreach ($values in $networkValues) {
        Add-Utterance $items "miaia_network_create" @(
            "Create a network with ", (New-EntityPart 'wit$number:inputs' ([string]$values[0])),
            " inputs, ", (New-EntityPart 'wit$number:hidden_width' ([string]$values[1])),
            " neurons per hidden layer, ", (New-EntityPart 'wit$number:hidden_layers' ([string]$values[2])),
            " hidden layers and ", (New-EntityPart 'wit$number:outputs' ([string]$values[3])), " outputs"
        )
    }

    $trainingValues = @(
        @(100, "0.01", $null, $null), @(250, "0.005", $null, $null), @(500, "0.001", $null, $null),
        @(50, "0.02", $null, $null), @(1000, "0.0005", $null, $null),
        @(100, "0.01", "shuffle", 42), @(200, "0.005", "shuffled", 7),
        @(300, "0.002", "shuffle", 123), @(750, "0.001", "shuffled", 99), @(120, "0.015", "shuffle", 0)
    )
    foreach ($values in $trainingValues) {
        $parts = @(
            "Start a training session for ", (New-EntityPart 'wit$number:epochs' ([string]$values[0])),
            " epochs with learning rate ", (New-EntityPart 'wit$number:learning_rate' ([string]$values[1]))
        )
        if ($null -ne $values[2]) {
            $parts += @(
                " using ", (New-EntityPart "sample_order:sample_order" ([string]$values[2])),
                " order and seed ", (New-EntityPart 'wit$number:seed' ([string]$values[3]))
            )
        }
        Add-Utterance $items "miaia_training_start" $parts
    }

    Add-PlainGroup $items "miaia_training_run" @(
        "Run the complete training session", "Run all training steps", "Continue training until completion",
        "Execute the whole training session", "Run the remaining training steps"
    )
    foreach ($steps in @(1, 5, 10, 25, 100)) {
        Add-Utterance $items "miaia_training_run" @("Run ", (New-EntityPart 'wit$number:steps' ([string]$steps)), " training steps")
    }

    Add-PlainGroup $items "miaia_training_pause" @(
        "Pause training", "Pause the training session", "Temporarily stop training", "Hold the training process",
        "Suspend training", "Pause model training", "Stop training for now", "Put training on hold",
        "Interrupt the current training session", "Pause the active training run"
    )
    Add-PlainGroup $items "miaia_training_resume" @(
        "Resume training", "Resume the training session", "Continue the paused training", "Restart the paused training",
        "Carry on with training", "Continue model training", "Resume the active session", "Start training again",
        "Proceed with the paused training", "Continue the training process"
    )
    Add-PlainGroup $items "miaia_training_status" @(
        "Show training status", "What is the training status?", "Is training currently running?", "Show the training progress",
        "How is the training going?", "Tell me the current training state", "Display training information",
        "Check the training session", "Is the training session active?", "Report the current training status"
    )
    Add-PlainGroup $items "miaia_project_new" @(
        "Create a new project", "Start a new project", "Make a blank project", "Begin a new MiaIA project",
        "Open a fresh project", "Initialize a new project", "Create an empty project", "Start from a clean project",
        "Make a fresh MiaIA project", "Reset to a new project"
    )

    foreach ($path in @("C:\Projects\iris.mai", "D:\Models\atlas.mai", "C:\MiaIA\demo.mai", "D:\Experiments\baseline.mai", "C:\Networks\classifier.mai", "D:\AI\candidate.mai", "C:\Data\vision.mai", "D:\Work\sandbox.mai", "C:\Tests\sample.mai", "D:\Projects\research.mai")) {
        Add-Utterance $items "miaia_project_open" @("Open project ", (New-EntityPart "project_path:project_path" $path))
    }

    Add-PlainGroup $items "miaia_project_save" @(
        "Save the current project", "Save this project", "Save my work", "Write the project to disk", "Save project"
    )
    foreach ($path in @("C:\Projects\iris.mai", "D:\Models\atlas.mai", "C:\MiaIA\demo.mai", "D:\Experiments\baseline.mai", "C:\Networks\classifier.mai")) {
        Add-Utterance $items "miaia_project_save" @("Save the project as ", (New-EntityPart "project_path:project_path" $path))
    }

    Add-PlainGroup $items $null @(
        "What is the weather?", "Tell me a joke", "Play some music", "Send an email", "Read the latest news",
        "Book a restaurant", "What time is it?", "Open a web browser", "Turn off the computer", "Call my phone",
        "Find a nearby hotel", "Translate this sentence"
    )

    return $items
}

function Get-ItalianUtterances {
    $items = [System.Collections.ArrayList]::new()

    Add-PlainGroup $items "miaia_help" @(
        "Mostra l'aiuto", "Aiutami", "Visualizza la guida dei comandi", "Elenca i comandi disponibili",
        "Cosa posso fare?", "Quali comandi sono disponibili?", "Apri la guida dei comandi",
        "Spiegami i comandi della console", "Come si usa MiaIA?", "Mostrami la lista dei comandi"
    )
    Add-PlainGroup $items "miaia_model_list" @(
        "Mostra tutti i modelli", "Mostra i modelli", "Elenca tutti i modelli", "Mostra i modelli disponibili",
        "Quali modelli sono disponibili?", "Visualizza i contesti dei modelli", "Fammi vedere i modelli",
        "Quali modelli ci sono nel progetto?", "Visualizza ogni contesto del modello", "Dammi tutti i modelli"
    )

    foreach ($name in @("Atlante", "Iris", "Esperimento Uno", "Classificatore", "Modello Visione", "Sandbox", "Base", "Candidato A", "Rete Demo", "Modello Ricerca")) {
        Add-Utterance $items "miaia_model_create" @("Crea un modello chiamato ", (New-EntityPart "model_name:model_name" $name))
    }

    foreach ($id in 1..10) {
        Add-Utterance $items "miaia_model_select" @("Seleziona il modello ", (New-EntityPart 'wit$number:model_id' ([string]$id)))
    }

    $networkValues = @(
        @(2, 4, 1, 1), @(4, 8, 2, 2), @(10, 32, 2, 3), @(8, 16, 3, 4), @(3, 12, 2, 1),
        @(16, 64, 4, 8), @(6, 24, 3, 2), @(12, 48, 2, 6), @(5, 20, 1, 5), @(20, 40, 5, 10)
    )
    foreach ($values in $networkValues) {
        Add-Utterance $items "miaia_network_create" @(
            "Crea una rete con ", (New-EntityPart 'wit$number:inputs' ([string]$values[0])),
            " ingressi, ", (New-EntityPart 'wit$number:hidden_width' ([string]$values[1])),
            " neuroni per livello nascosto, ", (New-EntityPart 'wit$number:hidden_layers' ([string]$values[2])),
            " livelli nascosti e ", (New-EntityPart 'wit$number:outputs' ([string]$values[3])), " uscite"
        )
    }

    $trainingValues = @(
        @(100, "0.01", $null, $null), @(250, "0.005", $null, $null), @(500, "0.001", $null, $null),
        @(50, "0.02", $null, $null), @(1000, "0.0005", $null, $null),
        @(100, "0.01", "mescola", 42), @(200, "0.005", "mescolato", 7),
        @(300, "0.002", "mischia", 123), @(750, "0.001", "casuale", 99), @(120, "0.015", "mescola", 0)
    )
    foreach ($values in $trainingValues) {
        $parts = @(
            "Avvia una sessione di training per ", (New-EntityPart 'wit$number:epochs' ([string]$values[0])),
            " epoche con learning rate ", (New-EntityPart 'wit$number:learning_rate' ([string]$values[1]))
        )
        if ($null -ne $values[2]) {
            $parts += @(
                " in ordine ", (New-EntityPart "sample_order:sample_order" ([string]$values[2])),
                " con seed ", (New-EntityPart 'wit$number:seed' ([string]$values[3]))
            )
        }
        Add-Utterance $items "miaia_training_start" $parts
    }

    Add-PlainGroup $items "miaia_training_run" @(
        "Esegui tutta la sessione di training", "Esegui tutti i passi del training", "Continua il training fino alla fine",
        "Esegui il training completo", "Esegui i passi di training rimanenti"
    )
    foreach ($steps in @(1, 5, 10, 25, 100)) {
        Add-Utterance $items "miaia_training_run" @("Esegui ", (New-EntityPart 'wit$number:steps' ([string]$steps)), " passi di training")
    }

    Add-PlainGroup $items "miaia_training_pause" @(
        "Metti in pausa il training", "Metti in pausa la sessione di training", "Ferma temporaneamente il training",
        "Sospendi il training", "Arresta il training per ora", "Metti il training in attesa", "Interrompi la sessione corrente",
        "Ferma momentaneamente l'addestramento", "Sospendi l'addestramento del modello", "Metti in pausa il training attivo"
    )
    Add-PlainGroup $items "miaia_training_resume" @(
        "Riprendi il training", "Riprendi la sessione di training", "Continua il training in pausa", "Riavvia il training sospeso",
        "Prosegui con il training", "Continua l'addestramento del modello", "Riprendi la sessione attiva",
        "Fai ripartire il training", "Prosegui il training sospeso", "Continua il processo di training"
    )
    Add-PlainGroup $items "miaia_training_status" @(
        "Mostra lo stato del training", "Qual e lo stato del training?", "Il training e in esecuzione?",
        "Mostra il progresso del training", "Come sta andando il training?", "Dimmi lo stato corrente del training",
        "Visualizza le informazioni del training", "Controlla la sessione di training", "La sessione di training e attiva?",
        "Riporta lo stato corrente del training"
    )
    Add-PlainGroup $items "miaia_project_new" @(
        "Crea un nuovo progetto", "Avvia un nuovo progetto", "Crea un progetto vuoto", "Inizia un nuovo progetto MiaIA",
        "Apri un progetto nuovo", "Inizializza un nuovo progetto", "Prepara un progetto vuoto", "Parti da un progetto pulito",
        "Crea un progetto MiaIA nuovo", "Riparti con un nuovo progetto"
    )

    foreach ($path in @("C:\Progetti\iris.mai", "D:\Modelli\atlante.mai", "C:\MiaIA\demo.mai", "D:\Esperimenti\base.mai", "C:\Reti\classificatore.mai", "D:\AI\candidato.mai", "C:\Dati\visione.mai", "D:\Lavoro\sandbox.mai", "C:\Test\esempio.mai", "D:\Progetti\ricerca.mai")) {
        Add-Utterance $items "miaia_project_open" @("Apri il progetto ", (New-EntityPart "project_path:project_path" $path))
    }

    Add-PlainGroup $items "miaia_project_save" @(
        "Salva il progetto corrente", "Salva questo progetto", "Salva il mio lavoro", "Scrivi il progetto su disco", "Salva il progetto"
    )
    foreach ($path in @("C:\Progetti\iris.mai", "D:\Modelli\atlante.mai", "C:\MiaIA\demo.mai", "D:\Esperimenti\base.mai", "C:\Reti\classificatore.mai")) {
        Add-Utterance $items "miaia_project_save" @("Salva il progetto come ", (New-EntityPart "project_path:project_path" $path))
    }

    Add-PlainGroup $items $null @(
        "Che tempo fa?", "Raccontami una barzelletta", "Riproduci della musica", "Invia una email",
        "Leggi le ultime notizie", "Prenota un ristorante", "Che ore sono?", "Apri un browser web",
        "Spegni il computer", "Chiama il mio telefono", "Trova un albergo vicino", "Traduci questa frase"
    )

    return $items
}

function Write-JsonFile {
    param(
        [Parameter(Mandatory)] [string]$Path,
        [Parameter(Mandatory)] [object]$Value
    )

    $json = $Value | ConvertTo-Json -Depth 12
    [System.IO.File]::WriteAllText(
        $Path,
        $json + [Environment]::NewLine,
        [System.Text.UTF8Encoding]::new($false))
}

function Write-WitPackage {
    param(
        [Parameter(Mandatory)] [string]$Language,
        [Parameter(Mandatory)] [string]$FolderName,
        [Parameter(Mandatory)] [System.Collections.ArrayList]$Utterances
    )

    Assert-Utterances $Utterances

    $packageRoot = Join-Path $OutputRoot $FolderName
    if (Test-Path -LiteralPath $packageRoot) {
        Remove-Item -LiteralPath $packageRoot -Recurse -Force
    }

    $entitiesRoot = Join-Path $packageRoot "entities"
    $intentsRoot = Join-Path $packageRoot "intents"
    $utterancesRoot = Join-Path $packageRoot "utterances"
    [void](New-Item -ItemType Directory -Path $entitiesRoot -Force)
    [void](New-Item -ItemType Directory -Path $intentsRoot -Force)
    [void](New-Item -ItemType Directory -Path $utterancesRoot -Force)

    $zipCommand = "zip $FolderName.zip $FolderName/app.json " +
        "$FolderName/entities/*.json $FolderName/intents/*.json " +
        "$FolderName/traits/*.json $FolderName/utterances/*.json " +
        "$FolderName/composer/*.json"
    Write-JsonFile (Join-Path $packageRoot "app.json") ([ordered]@{
        version = 20200513
        "zip-command" = $zipCommand
        name = $FolderName
        lang = $Language
        timezone = "Europe/Rome"
    })

    foreach ($intent in $IntentNames) {
        Write-JsonFile (Join-Path $intentsRoot "$intent.json") ([ordered]@{
            name = $intent
            entities = @()
        })
    }

    Write-JsonFile (Join-Path $entitiesRoot 'wit$number.json') ([ordered]@{
        name = 'wit$number'
        roles = @(
            "model_id", "inputs", "hidden_width", "hidden_layers", "outputs",
            "epochs", "learning_rate", "seed", "steps"
        )
    })
    foreach ($entityName in @("model_name", "project_path", "sample_order")) {
        Write-JsonFile (Join-Path $entitiesRoot "$entityName.json") ([ordered]@{
            name = $entityName
            roles = @($entityName)
            lookups = @("free-text")
        })
    }

    $chunkSize = 100
    for ($offset = 0; $offset -lt $Utterances.Count; $offset += $chunkSize) {
        $last = [Math]::Min($offset + $chunkSize - 1, $Utterances.Count - 1)
        $chunk = @($Utterances[$offset..$last])
        $chunkNumber = [Math]::Floor($offset / $chunkSize) + 1
        Write-JsonFile (Join-Path $utterancesRoot "utterances-$chunkNumber.json") ([ordered]@{
            utterances = $chunk
        })
    }

    $zipPath = Join-Path $OutputRoot "$FolderName.zip"
    if (Test-Path -LiteralPath $zipPath) {
        Remove-Item -LiteralPath $zipPath -Force
    }
    Compress-Archive -LiteralPath $packageRoot -DestinationPath $zipPath -CompressionLevel Optimal

    return [pscustomobject]@{
        Language = $Language
        Folder = $packageRoot
        Zip = $zipPath
        Intents = $IntentNames.Count
        Utterances = $Utterances.Count
        OutOfScope = @($Utterances | Where-Object { -not $_.Contains("intent") }).Count
        Entities = 4
    }
}

[void](New-Item -ItemType Directory -Path $OutputRoot -Force)

$results = @(
    Write-WitPackage -Language "en" -FolderName "en_miaia_studio_assistant_complete" -Utterances (Get-EnglishUtterances)
    Write-WitPackage -Language "it" -FolderName "it_miaia_studio_assistant_complete" -Utterances (Get-ItalianUtterances)
)

$results | Format-Table -AutoSize
