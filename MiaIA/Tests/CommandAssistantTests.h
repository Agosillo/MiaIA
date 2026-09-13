#pragma once

#include "TestHarness.h"
#include "../IDE/StudioCore/Include/CommandAssistant.h"
#include "../IDE/StudioCore/Include/LocalCommandAssistant.h"
#include "../IDE/StudioCore/Include/AssistantParameterCatalog.h"
#include "../IDE/StudioCore/Include/ConsoleSessionHistory.h"
#include "../IDE/StudioCore/Include/AssistantStudioActions.h"
#include <algorithm>
#include <array>
#include <limits>
#include <string>
#include <utility>
#include <vector>

inline void RunCommandAssistantTests(MiaIA::Tests::TestRunner& runner)
{
    runner.Run("Assistant example filtering is textual and restores all on empty input", [&]()
    {
        using MiaIA::Studio::LocalCommandAssistant;
        MIAIA_CHECK(LocalCommandAssistant::MatchesExample("Mostra tutte le connessioni", "mostra"));
        MIAIA_CHECK(LocalCommandAssistant::MatchesExample("Mostra tutte le connessioni", "  TUTTE   LE "));
        MIAIA_CHECK(!LocalCommandAssistant::MatchesExample("Crea una rete", "mostra"));
        MIAIA_CHECK(!LocalCommandAssistant::MatchesExample("Show all connections", "hide"));
        MIAIA_CHECK(LocalCommandAssistant::MatchesExample("Mostrar conexiones", "conex"));
        MIAIA_CHECK(LocalCommandAssistant::MatchesExample("Show all connections", ""));
        MIAIA_CHECK(LocalCommandAssistant::MatchesExample("Mostra le connessioni", "   "));
    });
    runner.Run("Studio view actions share bilingual proposals and strict host dispatch", [&]()
    {
        using namespace MiaIA::Studio;
        for (const auto& entry : AssistantStudioActions)
        {
            for (const auto language : {CommandAssistantLanguage::English, CommandAssistantLanguage::Italian,
                CommandAssistantLanguage::Automatic})
            {
                LocalCommandAssistant assistant(language);
                const auto phrase = language == CommandAssistantLanguage::Italian ? entry.Command.Italian : entry.Command.English;
                CommandAssistantUnderstanding understood;
                MIAIA_CHECK(assistant.Interpret(std::string(phrase), [&](auto result) { understood = std::move(result); }));
                CommandProposal proposal;
                MIAIA_CHECK(CommandAssistant::Propose(understood, proposal));
                MIAIA_CHECK(proposal.Command == entry.Command.Command);
                MIAIA_CHECK(proposal.FullyConfident);
                MIAIA_CHECK(ParseStudioViewAction(proposal.Command) == entry.Action);
                MIAIA_CHECK(!ParseStudioViewAction(proposal.Command + " extra"));
                understood.Entities.push_back({"value", "value", "1", 1.0});
                MIAIA_CHECK(!CommandAssistant::Propose(understood, proposal));
            }
        }
        MIAIA_CHECK(!ParseStudioViewAction("studio reset everything"));
        MIAIA_CHECK(!ParseStudioViewAction("studio view 3d; project new"));
        MIAIA_CHECK(!ParseStudioViewAction("help"));
        for (const auto& sample : {
            std::pair{"mostra neuron labels", "studio labels show"},
            std::pair{"nascondi connections", "studio connections hide"},
            std::pair{"passa in 3d", "studio view 3d"},
            std::pair{"imposta il layout compatto", "studio layout packed"},
            std::pair{"please fit the view", "studio view fit"}})
        {
            LocalCommandAssistant assistant;
            CommandAssistantUnderstanding understood;
            assistant.Interpret(sample.first, [&](auto result) { understood = std::move(result); });
            CommandProposal proposal;
            MIAIA_CHECK(CommandAssistant::Propose(understood, proposal));
            MIAIA_CHECK(proposal.Command == sample.second);
            MIAIA_CHECK(!proposal.FullyConfident);
        }
        for (const auto text : {"do not hide neuron labels", "non mostrare le connessioni",
            "show neuron labels and hide connections", "switch to 2d and 3d", "set layout packed 12",
            "hide connections 5", "switch to 4d", "reset all preferences", "show connections and connections"})
        {
            LocalCommandAssistant assistant;
            CommandAssistantUnderstanding understood;
            assistant.Interpret(text, [&](auto result) { understood = std::move(result); });
            CommandProposal proposal;
            MIAIA_CHECK(!CommandAssistant::Propose(understood, proposal));
        }
    });
    runner.Run("Studio actions can be translated and learned without extending allowlist", [&]()
    {
        using namespace MiaIA::Studio;
        const auto text = LocalCommandAssistant::ExportLanguageTemplate();
        for (const auto& entry : AssistantStudioActions)
            MIAIA_CHECK(text.find(entry.Command.Intent) != std::string::npos);
        LocalCommandAssistant assistant;
        std::string error;
        MIAIA_CHECK(assistant.ImportLanguagePack(
            "MIAIA_LOCAL_LANGUAGE_PACK\t2\nL\tes\tEspanol\nE\tmiaia_studio_view_3d\tSwitch to 3D\tver en tres dimensiones\n", error));
        MIAIA_CHECK(assistant.SetLanguageCode("es"));
        CommandAssistantUnderstanding understood;
        assistant.Interpret("ver en tres dimensiones", [&](auto result) { understood = std::move(result); });
        CommandProposal proposal;
        MIAIA_CHECK(CommandAssistant::Propose(understood, proposal));
        MIAIA_CHECK(proposal.Command == "studio view 3d");
        MIAIA_CHECK(assistant.LearnValidated("vista espacial", "miaia_studio_view_3d"));
        assistant.Interpret("vista espacial", [&](auto result) { understood = std::move(result); });
        MIAIA_CHECK(CommandAssistant::Propose(understood, proposal));
        MIAIA_CHECK(proposal.Command == "studio view 3d");
        MIAIA_CHECK(!assistant.LearnValidated("execute anything", "miaia_studio_arbitrary"));
    });
    runner.Run("Console output remains continuous across recall filters", [&]()
    {
        using namespace MiaIA::Studio;
        ConsoleSessionHistory h;
        h.Append(ConsoleHistorySource::Console, "command\n");
        h.Append(ConsoleHistorySource::Assistant, "phrase\n");
        h.RecordInput(ConsoleHistorySource::Console, "help");
        h.RecordInput(ConsoleHistorySource::Assistant, "show models");
        for (auto filter : {ConsoleHistoryFilter::Automatic, ConsoleHistoryFilter::All,
            ConsoleHistoryFilter::Console, ConsoleHistoryFilter::Assistant})
        {
            h.Recall(ConsoleHistorySource::Console, true, filter);
            MIAIA_CHECK(h.Output() == "command\nphrase\n");
        }
        h.Append(ConsoleHistorySource::Assistant, "late result\n");
        MIAIA_CHECK(h.Output() == "command\nphrase\nlate result\n");
    });
    runner.Run("Console input recall and drafts are mode-specific", [&]()
    {
        using namespace MiaIA::Studio;
        using S = ConsoleHistorySource;
        ConsoleSessionHistory history;
        MIAIA_CHECK(!history.Recall(S::Console, true));
        MIAIA_CHECK(!history.Recall(S::Assistant, false));
        history.RecordInput(S::Console, "help");
        history.RecordInput(S::Assistant, "mostra modelli");
        history.RecordInput(S::Console, "summary");
        history.RecordInput(S::Console, "summary"); // deduplicate within a mode
        history.Append(S::Assistant, "> model list\nmodels\n"); // never an input entry
        history.EditInput(S::Console, "create 2 ");
        history.EditInput(S::Assistant, "crea una rete ");
        MIAIA_CHECK(history.Recall(S::Console, true) == "summary");
        MIAIA_CHECK(history.Recall(S::Assistant, true) == "mostra modelli");
        MIAIA_CHECK(history.CurrentInput(S::Console) == "summary");
        MIAIA_CHECK(history.CurrentInput(S::Assistant) == "mostra modelli");
        MIAIA_CHECK(history.Recall(S::Console, true) == "help");
        MIAIA_CHECK(history.Recall(S::Console, true) == "help");
        MIAIA_CHECK(history.Recall(S::Console, false) == "summary");
        MIAIA_CHECK(history.Recall(S::Console, false) == "create 2 ");
        MIAIA_CHECK(history.Recall(S::Assistant, false) == "crea una rete ");
        MIAIA_CHECK(!history.Recall(S::Assistant, false));
        history.EditInput(S::Assistant, "show network");
        MIAIA_CHECK(history.Recall(S::Assistant, true) == "mostra modelli");
        MIAIA_CHECK(history.Recall(S::Assistant, false) == "show network");
        MIAIA_CHECK(history.CurrentInput(S::Console) == "create 2 ");
    });
    runner.Run("Recall filters select commands phrases or both without clearing output", [&]()
    {
        using namespace MiaIA::Studio;
        using S = ConsoleHistorySource;
        using F = ConsoleHistoryFilter;
        ConsoleSessionHistory h;
        h.RecordInput(S::Console, "help");
        h.RecordInput(S::Assistant, "show models");
        h.RecordInput(S::Console, "summary");
        h.EditInput(S::Console, "draft");
        MIAIA_CHECK(h.Recall(S::Console, true, F::All) == "summary");
        MIAIA_CHECK(h.Recall(S::Console, true, F::All) == "show models");
        MIAIA_CHECK(h.Recall(S::Console, true, F::All) == "help");
        MIAIA_CHECK(h.Recall(S::Console, false, F::All) == "show models");
        MIAIA_CHECK(h.Recall(S::Console, false, F::All) == "summary");
        MIAIA_CHECK(h.Recall(S::Console, false, F::All) == "draft");
        MIAIA_CHECK(h.Recall(S::Console, true, F::Assistant) == "show models");
        MIAIA_CHECK(h.Recall(S::Assistant, true, F::Console) == "summary");
        h.Append(S::Console, "output");
        h.ClearOutput();
        MIAIA_CHECK(h.Output().empty());
        MIAIA_CHECK(h.Recall(S::Assistant, true, F::Console) == "help");
        MIAIA_CHECK(h.Recall(S::Console, false, F::Assistant) == "draft");
    });
    runner.Run("Studio command assistant proposals", [&]()
    {
        using namespace MiaIA::Studio;

        CommandProposal proposal;
        CommandAssistantUnderstanding understanding{
            "Create a model named XOR experiment",
            "miaia_model_create",
            0.98,
            {
                { "model_name", "model_name", "XOR experiment", 0.99 }
            }
        };

        MIAIA_CHECK(CommandAssistant::Propose(understanding, proposal));
        MIAIA_CHECK(proposal.Command == "model create \"XOR experiment\"");
        MIAIA_CHECK(proposal.SourceText == understanding.Text);
        MIAIA_CHECK(proposal.Intent == understanding.Intent);
        MIAIA_CHECK(proposal.Confidence == understanding.Confidence);
        MIAIA_CHECK(!proposal.FullyConfident);

        understanding = {
            "Show every model",
            "miaia_model_list",
            1.0,
            {}
        };
        MIAIA_CHECK(CommandAssistant::Propose(understanding, proposal));
        MIAIA_CHECK(proposal.Command == "model list");
        MIAIA_CHECK(proposal.FullyConfident);

        understanding = {
            "Select model two",
            "miaia_model_select",
            1.0,
            {
                { "wit$number", "model_id", "2", 0.999999 }
            }
        };
        MIAIA_CHECK(CommandAssistant::Propose(understanding, proposal));
        MIAIA_CHECK(proposal.Command == "model select 2");
        MIAIA_CHECK(!proposal.FullyConfident);

        understanding = {
            "Create a network with two inputs, four hidden neurons, "
                "three hidden layers, and one output",
            "miaia_network_create",
            0.96,
            {
                { "inputs", {}, "2", 1.0 },
                { "hidden_width", {}, "4", 1.0 },
                { "hidden_layers", {}, "3", 1.0 },
                { "outputs", {}, "1", 1.0 }
            }
        };
        MIAIA_CHECK(CommandAssistant::Propose(understanding, proposal));
        MIAIA_CHECK(proposal.Command == "create 2 4 3 1");

        understanding = {
            "Start twenty epochs at point zero one with shuffle seed 42",
            "miaia_training_start",
            0.94,
            {
                { "epochs", {}, "20", 1.0 },
                { "learning_rate", {}, "0.01", 1.0 },
                { "sample_order", {}, "shuffle", 1.0 },
                { "seed", {}, "42", 1.0 }
            }
        };
        MIAIA_CHECK(CommandAssistant::Propose(understanding, proposal));
        MIAIA_CHECK(proposal.Command ==
            "train session start 20 0.01 mse shuffle 42");

        understanding = {
            "Avvia venti epoche con ordine casuale e seed 42",
            "miaia_training_start",
            0.94,
            {
                { "epochs", {}, "20", 1.0 },
                { "learning_rate", {}, "0.01", 1.0 },
                { "sample_order", {}, "casuale", 1.0 },
                { "seed", {}, "42", 1.0 }
            }
        };
        MIAIA_CHECK(CommandAssistant::Propose(understanding, proposal));
        MIAIA_CHECK(proposal.Command ==
            "train session start 20 0.01 mse shuffle 42");

        understanding = {
            "Run every remaining training step",
            "miaia_training_run",
            0.91,
            {}
        };
        MIAIA_CHECK(CommandAssistant::Propose(understanding, proposal));
        MIAIA_CHECK(proposal.Command == "train session run all");

        understanding = {
            "Esegui tutti i passi rimanenti",
            "miaia_training_run",
            0.91,
            {
                { "steps", {}, "tutti", 1.0 }
            }
        };
        MIAIA_CHECK(CommandAssistant::Propose(understanding, proposal));
        MIAIA_CHECK(proposal.Command == "train session run all");

        understanding = {
            "Open my project",
            "miaia_project_open",
            0.93,
            {
                { "project_path", {}, "C:\\Models\\xor.mai", 1.0 }
            }
        };
        MIAIA_CHECK(CommandAssistant::Propose(understanding, proposal));
        MIAIA_CHECK(proposal.Command ==
            "project open \"C:\\Models\\xor.mai\"");

        understanding = {
            "Maybe start training",
            "miaia_training_start",
            0.42,
            {
                { "epochs", {}, "20", 1.0 },
                { "learning_rate", {}, "0.01", 1.0 }
            }
        };
        MIAIA_CHECK(!CommandAssistant::Propose(understanding, proposal));
        MIAIA_CHECK(proposal.Command.empty());
        MIAIA_CHECK(proposal.Error.find("confidence") != std::string::npos);

        understanding = {
            "Create an unsafe model name",
            "miaia_model_create",
            0.99,
            {
                { "model_name", {}, "bad\"\nproject new", 1.0 }
            }
        };
        MIAIA_CHECK(!CommandAssistant::Propose(understanding, proposal));
        MIAIA_CHECK(proposal.Command.empty());

        understanding = {
            "Do something unsupported",
            "miaia_unknown",
            0.99,
            {}
        };
        MIAIA_CHECK(!CommandAssistant::Propose(understanding, proposal));
        MIAIA_CHECK(proposal.Error.find("not supported") != std::string::npos);
    });

    runner.Run("Local command assistant English and Italian", [&]()
    {
        using namespace MiaIA::Studio;

        const auto expect = [](LocalCommandAssistant& assistant,
            const std::string& text,
            const std::string& intent,
            const std::string& command)
        {
            CommandAssistantUnderstanding understanding;
            bool completed{};
            MIAIA_CHECK(assistant.Interpret(text,
                [&](CommandAssistantUnderstanding value)
                {
                    understanding = std::move(value);
                    completed = true;
                }));
            MIAIA_CHECK(completed);
            MIAIA_CHECK(understanding.Error.empty());
            if (understanding.Intent != intent)
            {
                throw std::runtime_error(
                    "For '" + text + "' expected intent '" + intent +
                    "' but received '" + understanding.Intent + "'.");
            }
            CommandProposal proposal;
            MIAIA_CHECK(CommandAssistant::Propose(understanding, proposal));
            if (proposal.Command != command)
            {
                throw std::runtime_error(
                    "For '" + text + "' expected '" + command +
                    "' but received '" + proposal.Command + "'.");
            }
        };

        LocalCommandAssistant english(CommandAssistantLanguage::English);
        MIAIA_CHECK(english.IsAvailable());
        expect(english, "Show all models",
            "miaia_model_list", "model list");
        expect(english, "Create a model named Vision Model",
            "miaia_model_create", "model create \"Vision Model\"");
        expect(english, "Switch to model 3",
            "miaia_model_select", "model select 3");
        expect(english,
            "Create a network with 2 inputs, 4 neurons per hidden layer, "
            "1 hidden layer and 1 output",
            "miaia_network_create", "create 2 4 1 1");
        expect(english, "Create a network",
            "miaia_network_create", "create");
        expect(english,
            "Start training for 200 epochs with learning rate 0.005 "
            "using shuffle order and seed 7",
            "miaia_training_start",
            "train session start 200 0.005 mse shuffle 7");
        expect(english, "Run 25 training steps",
            "miaia_training_run", "train session run 25");
        expect(english, "Pause model training",
            "miaia_training_pause", "train session pause");
        expect(english, "Interrupt the current training session",
            "miaia_training_pause", "train session pause");
        expect(english, "Resume the training session",
            "miaia_training_resume", "train session resume");
        expect(english, "Continue training until completion",
            "miaia_training_run", "train session run all");
        expect(english, "How is the training going?",
            "miaia_training_status", "train session status");
        expect(english, "Create a new project",
            "miaia_project_new", "project new");
        expect(english, "Open a fresh project",
            "miaia_project_new", "project new");
        expect(english, "Open project C:\\Projects\\iris.mai",
            "miaia_project_open", "project open \"C:\\Projects\\iris.mai\"");
        expect(english, "Save the current project",
            "miaia_project_save", "project save");

        CommandAssistantUnderstanding unknown;
        MIAIA_CHECK(english.Interpret("Tell me a joke",
            [&](CommandAssistantUnderstanding value)
            {
                unknown = std::move(value);
            }));
        MIAIA_CHECK(unknown.Intent.empty());
        MIAIA_CHECK(!unknown.Error.empty());

        LocalCommandAssistant italian(CommandAssistantLanguage::Italian);
        expect(italian, "Mostra tutti i modelli",
            "miaia_model_list", "model list");
        expect(italian, "Mostrami i modelli",
            "miaia_model_list", "model list");
        expect(italian, "Elencami i modelli disponibili",
            "miaia_model_list", "model list");
        expect(italian, "Crea un modello chiamato Modello Ricerca",
            "miaia_model_create", "model create \"Modello Ricerca\"");
        expect(italian, "Seleziona il modello 4",
            "miaia_model_select", "model select 4");
        expect(italian,
            "Crea una rete con 8 ingressi, 16 neuroni per livello nascosto, "
            "3 livelli nascosti e 4 uscite",
            "miaia_network_create", "create 8 16 3 4");
        expect(italian, "Crea una rete",
            "miaia_network_create", "create");
        CommandAssistantUnderstanding incompleteNetwork;
        MIAIA_CHECK(italian.Interpret(
            "Crea una rete 2 input 4 layer hidden 1 output",
            [&](CommandAssistantUnderstanding value)
            {
                incompleteNetwork = std::move(value);
            }));
        MIAIA_CHECK(incompleteNetwork.Intent == "miaia_network_create");
        CommandProposal incompleteProposal;
        MIAIA_CHECK(!CommandAssistant::Propose(
            incompleteNetwork,
            incompleteProposal));
        MIAIA_CHECK(!incompleteProposal.Error.empty());
        expect(italian,
            "Avvia il training per 120 epoche con learning rate 0.015 "
            "in ordine casuale con seed 0",
            "miaia_training_start",
            "train session start 120 0.015 mse shuffle 0");
        expect(italian, "Esegui tutti i passi del training",
            "miaia_training_run", "train session run all");
        expect(italian, "Metti in pausa il training",
            "miaia_training_pause", "train session pause");
        expect(italian, "Interrompi la sessione corrente di training",
            "miaia_training_pause", "train session pause");
        expect(italian, "Riprendi il training",
            "miaia_training_resume", "train session resume");
        expect(italian, "Mostra lo stato del training",
            "miaia_training_status", "train session status");
        expect(italian, "Qual è lo stato del training?",
            "miaia_training_status", "train session status");
        expect(italian, "Crea un nuovo progetto",
            "miaia_project_new", "project new");
        expect(italian, "Apri il progetto D:\\Modelli\\atlante.mai",
            "miaia_project_open", "project open \"D:\\Modelli\\atlante.mai\"");
        expect(italian, "Salva il progetto corrente",
            "miaia_project_save", "project save");
    });

    runner.Run("Local command assistant automatic language and learning", [&]()
    {
        using namespace MiaIA::Studio;

        const auto interpret = [](LocalCommandAssistant& assistant,
            const std::string& text)
        {
            CommandAssistantUnderstanding result;
            bool completed{};
            MIAIA_CHECK(assistant.Interpret(text,
                [&](CommandAssistantUnderstanding value)
                {
                    result = std::move(value);
                    completed = true;
                }));
            MIAIA_CHECK(completed);
            return result;
        };

        LocalCommandAssistant automatic(CommandAssistantLanguage::Automatic);
        auto english = interpret(automatic, "Show all models");
        MIAIA_CHECK(english.Intent == "miaia_model_list");
        auto italian = interpret(automatic, "Mostrami i modelli");
        MIAIA_CHECK(italian.Intent == "miaia_model_list");
        auto mixed = interpret(automatic, "Show lo stato del training");
        MIAIA_CHECK(mixed.Intent == "miaia_training_status");

        auto proposal = interpret(automatic, "Visualizza i modelli");
        MIAIA_CHECK(proposal.Intent == "miaia_model_list");
        MIAIA_CHECK(proposal.Confidence >= 0.70 && proposal.Confidence < 1.0);
        MIAIA_CHECK(automatic.LearnValidated(
            proposal.Text,
            proposal.Intent));
        const auto learned = interpret(automatic, "Visualizza i modelli");
        MIAIA_CHECK(learned.Intent == "miaia_model_list");
        MIAIA_CHECK(learned.Confidence == 1.0);
        MIAIA_CHECK(automatic.LearnedExamples().size() == 1);

        const std::string unknownText =
            "Presentami l'architettura quantistica";
        const auto unknown = interpret(automatic, unknownText);
        MIAIA_CHECK(unknown.Intent.empty());
        MIAIA_CHECK(!unknown.Error.empty());
        MIAIA_CHECK(automatic.RecordUnknown(unknownText));
        MIAIA_CHECK(automatic.PendingPhrases().size() == 1);
        MIAIA_CHECK(automatic.RecordUnknown(unknownText));
        MIAIA_CHECK(automatic.PendingPhrases().size() == 1);

        const std::string corpus = automatic.ExportCorpus();
        LocalCommandAssistant restored(CommandAssistantLanguage::Automatic);
        std::string error;
        MIAIA_CHECK(restored.ImportCorpus(corpus, error));
        MIAIA_CHECK(error.empty());
        MIAIA_CHECK(restored.LearnedExamples().size() == 1);
        MIAIA_CHECK(restored.PendingPhrases().size() == 1);
        MIAIA_CHECK(interpret(restored, "Visualizza i modelli").Confidence == 1.0);

        MIAIA_CHECK(!restored.ImportCorpus("not a corpus", error));
        MIAIA_CHECK(!error.empty());
        MIAIA_CHECK(restored.LearnedExamples().size() == 1);
        MIAIA_CHECK(restored.PendingPhrases().size() == 1);

        MIAIA_CHECK(restored.ClassifyPending(
            unknownText,
            "miaia_help"));
        MIAIA_CHECK(restored.PendingPhrases().empty());
        const auto classified = interpret(restored, unknownText);
        MIAIA_CHECK(classified.Intent == "miaia_help");
        MIAIA_CHECK(classified.Confidence == 1.0);

        MIAIA_CHECK(restored.MarkIncorrect(unknownText));
        MIAIA_CHECK(restored.PendingPhrases().size() == 1);
        MIAIA_CHECK(interpret(restored, unknownText).Intent.empty());
        MIAIA_CHECK(restored.RemovePending(unknownText));
        MIAIA_CHECK(restored.PendingPhrases().empty());

        MIAIA_CHECK(restored.RecordUnknown("Elimina rete"));
        MIAIA_CHECK(restored.MarkIncorrect(
            "Mi crei una rete 2 input 4 layer hidden 1 output"));
        MIAIA_CHECK(restored.PendingPhrases().size() == 2);
        MIAIA_CHECK(restored.PendingPhrases().back().Text ==
            "Mi crei una rete 2 input 4 layer hidden 1 output");

        LocalCommandAssistant editable(CommandAssistantLanguage::Automatic);
        MIAIA_CHECK(editable.LearnValidated(
            "Comando personale alpha",
            "miaia_help"));
        MIAIA_CHECK(editable.LearnValidated(
            "Comando personale alpha",
            "miaia_model_list"));
        MIAIA_CHECK(editable.LearnedExamples().size() == 2);
        MIAIA_CHECK(editable.ReassignValidated(
            "Comando personale alpha",
            "miaia_training_status"));
        MIAIA_CHECK(editable.LearnedExamples().size() == 1);
        MIAIA_CHECK(interpret(editable, "Comando personale alpha").Intent ==
            "miaia_training_status");
        MIAIA_CHECK(editable.RemoveValidated("Comando personale alpha"));
        MIAIA_CHECK(editable.LearnedExamples().empty());
        MIAIA_CHECK(editable.RecordUnknown("Comando personale beta"));
        editable.ClearCorpus();
        MIAIA_CHECK(editable.LearnedExamples().empty());
        MIAIA_CHECK(editable.PendingPhrases().empty());

        LocalCommandAssistant italianOnly(CommandAssistantLanguage::Italian);
        MIAIA_CHECK(italianOnly.LearnValidated(
            "Comando personale zeta",
            "miaia_help"));
        const std::string italianCorpus = italianOnly.ExportCorpus();
        LocalCommandAssistant englishOnly(CommandAssistantLanguage::English);
        MIAIA_CHECK(englishOnly.ImportCorpus(italianCorpus, error));
        MIAIA_CHECK(interpret(englishOnly, "Comando personale zeta").Intent.empty());
        englishOnly.SetLanguage(CommandAssistantLanguage::Automatic);
        MIAIA_CHECK(interpret(englishOnly, "Comando personale zeta").Intent ==
            "miaia_help");
    });

    runner.Run("Italian sidebar network example preserves mixed parameter labels", [&]()
    {
        using namespace MiaIA::Studio;
        for (const auto language : {CommandAssistantLanguage::Italian, CommandAssistantLanguage::Automatic})
        {
            LocalCommandAssistant assistant(language);
            for (const auto phrase : {
                "Crea una rete con 2 input, 4 neuroni per hidden layer, 1 hidden layer e 1 output",
                "Crea una rete con 2 input, 4 neuroni per livello nascosto, 1 livello nascosto e 1 output",
                "Crea una rete con 1 output, 1 hidden layer, 4 neuroni per hidden layer e 2 input"})
            {
                CommandAssistantUnderstanding result;
                MIAIA_CHECK(assistant.Interpret(phrase, [&](auto value) { result = std::move(value); }));
                CommandProposal proposal;
                MIAIA_CHECK(CommandAssistant::Propose(result, proposal));
                MIAIA_CHECK(proposal.Command == "create 2 4 1 1");
                MIAIA_CHECK(result.Entities.size() == 4);
            }
        }
    });
    runner.Run("Local network labelled parameters", [&]()
    {
        using namespace MiaIA::Studio;
        LocalCommandAssistant assistant(CommandAssistantLanguage::English);
        const auto interpret = [&](const std::string& text)
        {
            CommandAssistantUnderstanding result;
            MIAIA_CHECK(assistant.Interpret(text, [&](auto value) { result = std::move(value); }));
            return result;
        };
        const std::array<std::string, 4> parts{
            "2 inputs", "4 neurons per hidden layer", "3 hidden layers", "1 output"};
        std::array<int, 4> order{0, 1, 2, 3};
        do
        {
            const auto result = interpret("Create a network with " + parts[order[0]] + ", " +
                parts[order[1]] + ", " + parts[order[2]] + " and " + parts[order[3]]);
            CommandProposal proposal;
            MIAIA_CHECK(CommandAssistant::Propose(result, proposal));
            MIAIA_CHECK(proposal.Command == "create 2 4 3 1");
        } while (std::next_permutation(order.begin(), order.end()));
        CommandProposal proposal;
        MIAIA_CHECK(CommandAssistant::Propose(interpret(
            "Create a network: outputs: 1, hidden layers: 3, inputs: 2, hidden_width: 4"), proposal));
        MIAIA_CHECK(proposal.Command == "create 2 4 3 1");
        MIAIA_CHECK(CommandAssistant::Propose(interpret("Create a network"), proposal));
        MIAIA_CHECK(proposal.Command == "create");
        for (const std::string text : {
            "Create a network with 2 inputs and 1 output",
            "Create a network with 2 4 3 1",
            "Create a network with two inputs",
            "Create a network with five layers",
            "Create a network with v2 inputs, 4 neurons per layer, 3 hidden layers, 1 output",
            "Create a network with inputs: 2foo, hidden_width: 4, hidden_layers: 3, outputs: 1",
            "Create a network with -2 inputs, 4 neurons per layer, 3 hidden layers, 1 output",
            "Create a network with 2.5 inputs, 4 neurons per layer, 3 hidden layers, 1 output",
            "Create a network with 2,5 inputs, 4 neurons per layer, 3 hidden layers, 1 output",
            "Create a network with 0 inputs, 4 neurons per layer, 3 hidden layers, 1 output",
            "Create a network with 2 inputs, 4 neurons per layer, 3 hidden layers, 1 output, 5 inputs",
            "Create a network with 2 inputs, 4 neurons per layer, 3 hidden layers, 1 output, seed 5"})
        {
            const auto result = interpret(text);
            MIAIA_CHECK(result.Intent == "miaia_network_create");
            MIAIA_CHECK(!result.Error.empty());
            MIAIA_CHECK(!CommandAssistant::Propose(result, proposal));
        }
        const std::string custom = "Please assemble this: 1 output, 3 hidden layers, 4 neurons per layer, 2 inputs";
        MIAIA_CHECK(assistant.RecordUnknown(custom));
        MIAIA_CHECK(assistant.ClassifyPending(custom, "miaia_network_create"));
        MIAIA_CHECK(CommandAssistant::Propose(interpret(custom), proposal));
        MIAIA_CHECK(proposal.Command == "create 2 4 3 1");
        const std::string incomplete = "Please assemble this: 2 inputs";
        MIAIA_CHECK(assistant.LearnValidated(incomplete, "miaia_network_create"));
        MIAIA_CHECK(!CommandAssistant::Propose(interpret(incomplete), proposal));
        MIAIA_CHECK(proposal.Error.find("missing") != std::string::npos);
    });

    runner.Run("Local command assistant language packs", [&]()
    {
        using namespace MiaIA::Studio;

        const auto interpret = [](LocalCommandAssistant& assistant,
            const std::string& text)
        {
            CommandAssistantUnderstanding result;
            bool completed{};
            MIAIA_CHECK(assistant.Interpret(text,
                [&](CommandAssistantUnderstanding value)
                {
                    result = std::move(value);
                    completed = true;
                }));
            MIAIA_CHECK(completed);
            return result;
        };

        const std::string languageTemplate =
            LocalCommandAssistant::ExportLanguageTemplate();
        MIAIA_CHECK(languageTemplate.starts_with(
            "MIAIA_LOCAL_LANGUAGE_PACK\t2\nL\txx\tLanguage name\n"));
        for (const auto intent : LocalCommandAssistant::SupportedIntents())
            MIAIA_CHECK(languageTemplate.find(intent) != std::string::npos);

        const std::string spanishPack =
            "MIAIA_LOCAL_LANGUAGE_PACK\t1\n"
            "L\tes\tEspanol\n"
            "E\tmiaia_model_list\tShow all models\tMostrar todos los modelos\n"
            "E\tmiaia_neuron_inspect\tInspect neuron 2\tInspeccionar neurona 2\n";

        LocalCommandAssistant spanish;
        MIAIA_CHECK(!spanish.SetLanguageCode("es"));
        std::string error;
        MIAIA_CHECK(!spanish.ImportLanguagePack(languageTemplate, error));
        MIAIA_CHECK(error.find("placeholders") != std::string::npos);
        MIAIA_CHECK(!spanish.ImportLanguagePack(
            "MIAIA_LOCAL_LANGUAGE_PACK\t1\nL\tes\tSpagna\n"
            "E\tmiaia_help\tmostrar ayuda\t\n"
            "E\tmiaia_help\tlistar los comandos disponibles\t\n", error));
        MIAIA_CHECK(error.find("column 4") != std::string::npos);
        MIAIA_CHECK(spanish.LanguagePacks().empty());
        MIAIA_CHECK(spanish.ImportLanguagePack(spanishPack +
            "E\tmiaia_help\tShow help\n", error));
        MIAIA_CHECK(spanish.LanguagePacks()[0].Examples.size() == 2);
        MIAIA_CHECK(spanish.ImportLanguagePack(spanishPack, error));
        MIAIA_CHECK(error.empty());
        MIAIA_CHECK(spanish.LanguagePacks().size() == 1);
        MIAIA_CHECK(spanish.LanguagePacks()[0].Code == "es");
        MIAIA_CHECK(spanish.LanguagePacks()[0].DisplayName == "Espanol");
        MIAIA_CHECK(spanish.SetLanguageCode("ES"));
        MIAIA_CHECK(spanish.LanguageCode() == "es");

        auto result = interpret(spanish, "Mostrar todos los modelos");
        MIAIA_CHECK(result.Intent == "miaia_model_list");
        MIAIA_CHECK(result.Confidence == 1.0);
        CommandProposal proposal;
        MIAIA_CHECK(CommandAssistant::Propose(result, proposal));
        MIAIA_CHECK(proposal.Command == "model list");

        result = interpret(spanish, "Inspeccionar neurona 2");
        MIAIA_CHECK(result.Intent == "miaia_neuron_inspect");
        MIAIA_CHECK(result.Confidence == 1.0);
        MIAIA_CHECK(CommandAssistant::Propose(result, proposal));
        MIAIA_CHECK(proposal.Command == "inspect neuron 2");

        spanish.SetLanguage(CommandAssistantLanguage::Automatic);
        MIAIA_CHECK(interpret(spanish, "Mostrar todos los modelos").Intent ==
            "miaia_model_list");
        MIAIA_CHECK(spanish.SetLanguageCode("es"));
        MIAIA_CHECK(spanish.LearnValidated(
            "Presentar ayuda personalizada",
            "miaia_help"));
        MIAIA_CHECK(spanish.LearnedExamples().back().LanguageCode == "es");

        LocalCommandAssistant restored;
        MIAIA_CHECK(restored.ImportLanguagePack(
            spanish.ExportLanguagePack("es"), error));
        MIAIA_CHECK(restored.ImportCorpus(spanish.ExportCorpus(), error));
        MIAIA_CHECK(restored.SetLanguageCode("es"));
        MIAIA_CHECK(interpret(restored,
            "Presentar ayuda personalizada").Intent == "miaia_help");
        restored.SetLanguage(CommandAssistantLanguage::English);
        MIAIA_CHECK(interpret(restored,
            "Presentar ayuda personalizada").Intent.empty());

        const std::size_t installed = restored.LanguagePacks().size();
        const std::string savedCorpus = restored.ExportCorpus();
        const std::string previousPack = restored.ExportLanguagePack("es");
        LocalCommandAssistant preview;
        MIAIA_CHECK(preview.ImportLanguagePack(previousPack, error));
        MIAIA_CHECK(restored.ExportCorpus() == savedCorpus);
        MIAIA_CHECK(restored.ExportLanguagePack("es") == previousPack);
        MIAIA_CHECK(restored.ImportLanguagePack(
            "MIAIA_LOCAL_LANGUAGE_PACK\t1\nL\tes\tEspanol\n"
            "E\tmiaia_help\tShow help\tAyuda\n", error));
        MIAIA_CHECK(restored.LanguagePacks().size() == installed);
        MIAIA_CHECK(restored.LanguagePacks().back().Examples.size() == 1);
        MIAIA_CHECK(restored.ExportCorpus() == savedCorpus);
        // Rebuilding without the removed pack must preserve the separate corpus.
        LocalCommandAssistant withoutPack;
        MIAIA_CHECK(withoutPack.ImportCorpus(savedCorpus, error));
        MIAIA_CHECK(withoutPack.LanguagePacks().empty());
        MIAIA_CHECK(withoutPack.ExportCorpus() == savedCorpus);
        MIAIA_CHECK(restored.ImportLanguagePack(previousPack, error));
        MIAIA_CHECK(!restored.ImportLanguagePack(
            "MIAIA_LOCAL_LANGUAGE_PACK\t1\n"
            "L\tfr\tFrancais\n"
            "E\tmiaia_not_supported\tUnknown\tInconnu\n",
            error));
        MIAIA_CHECK(!error.empty());
        MIAIA_CHECK(restored.LanguagePacks().size() == installed);
        MIAIA_CHECK(!restored.ImportLanguagePack(
            "MIAIA_LOCAL_LANGUAGE_PACK\t1\n"
            "L\ten\tReplacement\n"
            "E\tmiaia_help\tShow help\tHelp\n",
            error));
        MIAIA_CHECK(restored.LanguagePacks().size() == installed);
    });

    runner.Run("Complete create parameters and validation", [&]()
    {
        using namespace MiaIA::Studio;
        LocalCommandAssistant local;
        auto understand = [&](const std::string& text)
        {
            CommandAssistantUnderstanding result;
            MIAIA_CHECK(local.Interpret(text, [&](auto value) { result = std::move(value); }));
            return result;
        };
        auto expect = [&](const std::string& text, const std::string& command)
        {
            CommandProposal proposal;
            const auto result = understand(text);
            if (!CommandAssistant::Propose(result, proposal) || proposal.Command != command)
                throw std::runtime_error(text + " -> " + proposal.Command + " / " + proposal.Error);
        };
        expect("crea una rete 2 input 4 neuroni per layer 10 layer e 3 output", "create 2 4 10 3");
        expect("crea una rete 2 input 4 neuroni per layer 10 layer hidden 3 output", "create 2 4 10 3");
        expect("Create a network with 2 inputs 4 neurons per layer 0 hidden layers 1 output",
            "create 2 4 0 1");
        expect("Crea una rete: input 2, hidden_width 4, hidden_layers 0, output 1, attivazione hidden relu, attivazione output linear, peso iniziale -0.125, bias 0",
            "create 2 4 0 1 --hidden-activation relu --output-activation linear --weight -0.125 --bias 0");
        expect("Create a network 2 inputs 4 neurons per layer 0 hidden layers 1 output --hidden-activation tanh --output-activation sigmoid --weight +1e-3 --bias -2.5",
            "create 2 4 0 1 --hidden-activation tanh --output-activation sigmoid --weight +1e-3 --bias -2.5");
        expect("Create a network 2 inputs 4 neurons per layer 0 hidden layers 1 output weight -.5 bias +.25",
            "create 2 4 0 1 --weight -.5 --bias +.25");
        std::vector<std::string> parts{"2 inputs", "4 neurons per layer", "0 hidden layers", "1 output",
            "hidden activation relu", "output activation linear", "initial weight -0.5", "bias 0.01"};
        for (std::size_t i = 0; i < parts.size(); ++i)
        {
            std::string phrase = "Create a network with ";
            for (const auto& part : parts) phrase += part + ", ";
            expect(phrase, "create 2 4 0 1 --hidden-activation relu --output-activation linear --weight -0.5 --bias 0.01");
            std::rotate(parts.begin(), parts.begin() + 1, parts.end());
        }
        const std::string base = "Create a network 2 inputs 4 neurons per layer 0 hidden layers 1 output";
        for (const auto& suffix : {" hidden activation swish", " output activation unknown", " bias nan",
            " weight inf", " weight 1e999", " bias -0,5", " weight 1.2.3", " bias 2foo", " weight ++2", " weight 2.foo",
            " bias 0 bias 0", " hidden activation relu hidden activation tanh", " --dropout foo",
            " dropout 0.2", " activation relu"})
        {
            CommandProposal proposal;
            MIAIA_CHECK(!CommandAssistant::Propose(understand(base + suffix), proposal));
        }
        for (const auto& text : {"Create a network hidden activation relu", "Create a network with weight 0.1",
            "Create a network 2 inputs 4 neurons per layer -1 hidden layers 1 output",
            "Create a network 2 inputs 4 neurons per layer 1.5 hidden layers 1 output",
            "Create a network 2147483648 inputs 4 neurons per layer 0 hidden layers 1 output"})
        {
            CommandProposal proposal;
            MIAIA_CHECK(!CommandAssistant::Propose(understand(text), proposal));
        }
        auto external = understand(base);
        external.Entities.push_back({"bias", "bias", "-0.5", 1.0});
        CommandProposal proposal;
        MIAIA_CHECK(CommandAssistant::Propose(external, proposal));
        MIAIA_CHECK(proposal.Command == "create 2 4 0 1 --bias -0.5");
        external.Entities.push_back({"bias", "bias", "1", 1.0});
        MIAIA_CHECK(!CommandAssistant::Propose(external, proposal));
        external = understand(base);
        external.Entities.push_back({"dropout", "dropout", "0.5", 1.0});
        MIAIA_CHECK(!CommandAssistant::Propose(external, proposal));
    });

    runner.Run("Editable parameter synonyms across intents and languages", [&]()
    {
        using namespace MiaIA::Studio;
        LocalCommandAssistant local;
        std::string error;
        const std::string pack = "MIAIA_LOCAL_LANGUAGE_PACK\t2\nL\ten\tEnglish\n"
            "P\tmiaia_network_create\thidden_width\tneurons per layer\tunits per tier\n"
            "P\tmiaia_network_create\thidden_layers\thidden layers\tdepth\n"
            "P\tmiaia_network_create\tweight\tweight\tconnection start\n"
            "P\tmiaia_training_start\tepochs\tepochs\trounds\n"
            "P\tmiaia_training_start\tlearning_rate\tlearning rate\tspeed\n"
            "P\tmiaia_model_create\tmodel_name\tname\ttitle\n"
            "P\tmiaia_project_save\tproject_path\tpath\tdestination\n"
            "P\tmiaia_neuron_inspect\tneuron_id\tneuron id\tunit\n"
            "P\tmiaia_neuron_inspect\tmaximum_connections\tmaximum connections\tcap\n";
        MIAIA_CHECK(local.ImportLanguagePack(pack, error));
        auto interpret = [&](const std::string& text)
        {
            CommandAssistantUnderstanding result;
            MIAIA_CHECK(local.Interpret(text, [&](auto value) { result = std::move(value); }));
            return result;
        };
        auto expect = [&](const std::string& text, const std::string& command)
        {
            CommandProposal proposal;
            if (!CommandAssistant::Propose(interpret(text), proposal) || proposal.Command != command)
                throw std::runtime_error(text + " -> " + proposal.Command + " / " + proposal.Error);
        };
        expect("Create a network with 2 inputs 4 units per tier depth 0 outputs 1 connection start -0.25",
            "create 2 4 0 1 --weight -0.25");
        expect("Start training speed 0.005 rounds 200 seed 7 sample order shuffle", "train session start 200 0.005 mse shuffle 7");
        expect("Create a model title \"Vision Model 2\"", "model create \"Vision Model 2\"");
        expect("Save project destination \"C:\\My Projects\\model 2.mai\"", "project save \"C:\\My Projects\\model 2.mai\"");
        expect("Open project C:\\--Models\\test.mai", "project open \"C:\\--Models\\test.mai\"");
        expect("Inspect neuron cap 5 unit 2", "inspect neuron 2 5");
        CommandProposal proposal;
        for (const auto& phrase : {"Start training speed 0.01 rounds 10 using shuffle",
            "Start training speed 0.01 rounds 10 seed 3",
            "Inspect neuron cap 5 unit -2", "Create a model title \"unterminated",
            "Create a network 2 inputs 4 units per tier 1 outputs"})
            if (CommandAssistant::Propose(interpret(phrase), proposal))
                throw std::runtime_error(std::string("Unexpected proposal for ") + phrase + ": " + proposal.Command);

        const auto snapshot = local.ExportLanguagePack("en");
        LocalCommandAssistant restored;
        MIAIA_CHECK(restored.ImportLanguagePack(snapshot, error));
        MIAIA_CHECK(restored.ExportLanguagePack("en") == snapshot);
        LocalCommandAssistant bounded;
        std::string atLimit = "MIAIA_LOCAL_LANGUAGE_PACK\t2\nL\ten\tEnglish\n";
        for (int i = 0; i < 256; ++i)
            atLimit += "P\tmiaia_network_create\tinputs\tsource\tinput alias " +
                std::string(1, static_cast<char>('a' + i / 26)) + static_cast<char>('a' + i % 26) + "\n";
        MIAIA_CHECK(bounded.ImportLanguagePack(atLimit, error));
        LocalCommandAssistant boundedRoundtrip;
        MIAIA_CHECK(boundedRoundtrip.ImportLanguagePack(bounded.ExportParameterTemplate("en"), error));
        MIAIA_CHECK(boundedRoundtrip.LanguagePacks().back().ParameterAliases.size() == 256);
        MIAIA_CHECK(!bounded.ImportLanguagePack(atLimit + "P\tmiaia_network_create\tinputs\tsource\tone too many\n", error));
        for (const auto code : {"en", "it"})
        {
            LocalCommandAssistant check;
            MIAIA_CHECK(check.ImportLanguagePack(local.ExportParameterTemplate(code), error));
            MIAIA_CHECK(check.LanguagePacks().back().Code == code);
        }
        LocalCommandAssistant supplemented;
        MIAIA_CHECK(supplemented.ImportLanguagePack(supplemented.ExportParameterTemplate("en"), error));
        CommandAssistantUnderstanding unchanged;
        supplemented.Interpret("Start training for 200 epochs with learning rate 0.005 using shuffle order and seed 7",
            [&](auto value) { unchanged = std::move(value); });
        MIAIA_CHECK(CommandAssistant::Propose(unchanged, proposal));
        MIAIA_CHECK(proposal.Command == "train session start 200 0.005 mse shuffle 7");
        const std::string spanish = "MIAIA_LOCAL_LANGUAGE_PACK\t2\nL\tes\tEspanol\n"
            "E\tmiaia_network_create\tcreate network\tcrear una red 2 entradas 4 neuronas por capa 0 capas ocultas 1 salida\n"
            "P\tmiaia_network_create\tinputs\tinputs\tentradas\n"
            "P\tmiaia_network_create\thidden_width\tneurons per layer\tneuronas por capa\n"
            "P\tmiaia_network_create\thidden_layers\thidden layers\tcapas ocultas\n"
            "P\tmiaia_network_create\toutputs\toutputs\tsalida\n";
        MIAIA_CHECK(local.ImportLanguagePack(spanish, error));
        expect("crear una red 2 entradas 4 neuronas por capa 0 capas ocultas 1 salida", "create 2 4 0 1");
        expect("crea una rete 2 entradas 4 units per tier 0 livelli nascosti 1 output", "create 2 4 0 1");
        MIAIA_CHECK(local.SetLanguageCode("it"));
        MIAIA_CHECK(!CommandAssistant::Propose(interpret("crea una rete 2 inputs 4 units per tier 0 hidden layers 1 output"), proposal));
        local.SetLanguage(CommandAssistantLanguage::Automatic);
        const auto spanishSnapshot = local.ExportLanguagePack("es");
        for (const auto& invalid : {
            "P\tmiaia_network_create\toutputs\toutputs\tdepth\n", // another installed pack
            "P\tmiaia_network_create\toutputs\toutputs\tinputs\n", // built-in vocabulary
            "P\tmiaia_network_create\tbad_role\tsource\tnew label\n",
            "P\tmiaia_arbitrary_command\tinputs\tsource\tnew label\n",
            "P\tmiaia_network_create\tinputs\tsource\t.*\n",
            "P\tmiaia_network_create\tinputs\tsource\tlabel 123\n"})
        {
            MIAIA_CHECK(!local.ImportLanguagePack(spanish + invalid, error));
            MIAIA_CHECK(local.ExportLanguagePack("es") == spanishSnapshot);
        }
        // Replacing a pack removes its old custom synonyms; learned intents do
        // not freeze previously extracted values or override updated vocabulary.
        const std::string oldPhrase = "Create a network 2 inputs 4 units per tier 0 depth 1 output";
        MIAIA_CHECK(local.LearnValidated(oldPhrase, "miaia_network_create"));
        expect(oldPhrase, "create 2 4 0 1");
        MIAIA_CHECK(local.ImportLanguagePack("MIAIA_LOCAL_LANGUAGE_PACK\t2\nL\ten\tEnglish\n"
            "P\tmiaia_network_create\thidden_width\tsource\tcells per tier\n", error));
        MIAIA_CHECK(!CommandAssistant::Propose(interpret(oldPhrase), proposal));
        expect("Create a network 2 inputs 4 cells per tier 0 hidden layers 1 output", "create 2 4 0 1");
    });

    runner.Run("Diagnostic options use the shared typed catalog", [&]()
    {
        using namespace MiaIA::Studio;
        LocalCommandAssistant local;
        const auto propose = [&](const std::string& phrase, CommandProposal& proposal)
        {
            CommandAssistantUnderstanding result;
            local.Interpret(phrase, [&](auto value) { result = std::move(value); });
            return CommandAssistant::Propose(result, proposal);
        };
        CommandProposal proposal;
        MIAIA_CHECK(propose("Diagnose dataset sample_limit 0 --inactive-magnitude 0 --inactive-ratio 0.9 --saturation-margin 0.5 --saturation-ratio 1 --vanishing-magnitude 0 --vanishing-ratio 0 --exploding-magnitude 10 --exploding-ratio 0.5 --max-items 3", proposal));
        MIAIA_CHECK(proposal.Command == "dataset diagnose 0 --exploding-magnitude 10 --exploding-ratio 0.5 --inactive-magnitude 0 --inactive-ratio 0.9 --max-items 3 --saturation-margin 0.5 --saturation-ratio 1 --vanishing-magnitude 0 --vanishing-ratio 0");
        for (const auto& option : {"--inactive-magnitude -0.1", "--inactive-ratio 1.1", "--saturation-margin 0.51",
            "--saturation-ratio -1", "--vanishing-magnitude -1", "--vanishing-ratio nan",
            "--exploding-magnitude 0", "--exploding-ratio 2", "--max-items 0", "--max-items 3.5",
            "--vanishing-magnitude 100", "--exploding-magnitude 1 --vanishing-magnitude 2",
            "--unknown 0.5", "--unknown foo", "--max-items 2 --max-items 3"})
            MIAIA_CHECK(!propose(std::string("Diagnose dataset ") + option, proposal));
        std::string error;
        MIAIA_CHECK(local.ImportLanguagePack("MIAIA_LOCAL_LANGUAGE_PACK\t2\nL\ten\tEnglish\n"
            "P\tmiaia_dataset_diagnose\tmax_items\tmax items\treport size\n"
            "P\tmiaia_training_run\tsteps\tsteps\trounds\n", error));
        MIAIA_CHECK(propose("Diagnose dataset report size 5", proposal));
        MIAIA_CHECK(proposal.Command == "dataset diagnose --max-items 5");
        MIAIA_CHECK(propose("Run training rounds all", proposal));
        MIAIA_CHECK(proposal.Command == "train session run all");
    });

    runner.Run("Assistant bilingual inspection commands", [&]()
    {
        using namespace MiaIA::Studio;
        const auto interpret = [](LocalCommandAssistant& local, const std::string& text)
        {
            CommandAssistantUnderstanding result;
            local.Interpret(text, [&](auto value) { result = std::move(value); });
            return result;
        };
        struct Case { const char* En; const char* It; const char* Intent; const char* Command; };
        const Case cases[] = {
            {"Show project information", "Mostra informazioni sul progetto", "miaia_project_info", "project info"},
            {"Show network summary", "Mostra riepilogo della rete", "miaia_network_summary", "summary"},
            {"Inspect network values", "Ispeziona valori della rete", "miaia_network_inspect", "inspect"},
            {"Inspect neuron 2", "Ispeziona neurone 2", "miaia_neuron_inspect", "inspect neuron 2"},
            {"Inspect connection 2", "Ispeziona connessione 2", "miaia_connection_inspect", "inspect connection 2"},
            {"Compare models 1 and 2", "Confronta modelli 1 e 2", "miaia_model_compare", "model compare 1 2"},
            {"List checkpoints", "Elenca checkpoint", "miaia_checkpoint_list", "checkpoint list"},
            {"Inspect checkpoint 1", "Ispeziona checkpoint 1", "miaia_checkpoint_inspect", "checkpoint inspect 1"},
            {"Compare checkpoints 1 and 2", "Confronta checkpoint 1 e 2", "miaia_checkpoint_compare", "checkpoint compare 1 2"},
            {"Show dataset summary", "Mostra riepilogo del dataset", "miaia_dataset_summary", "dataset summary"},
            {"Inspect sample 0", "Ispeziona campione 0", "miaia_dataset_inspect", "dataset inspect 0"},
            {"Diagnose dataset", "Diagnostica dataset", "miaia_dataset_diagnose", "dataset diagnose"},
            {"Show training history", "Mostra cronologia del training", "miaia_training_history", "train session history"},
            {"Inspect training step 0", "Ispeziona passo training 0", "miaia_training_inspect", "train session inspect 0"},
            {"Compare training steps 0 and 1", "Confronta passi training 0 e 1", "miaia_training_compare", "train session compare 0 1"},
            {"Show debug status", "Mostra stato del debug", "miaia_debug_status", "train debug status"},
            {"List breakpoints", "Elenca breakpoint", "miaia_breakpoint_list", "train breakpoint list"}
        };
        for (const auto& sample : cases)
        {
            for (const auto language : {CommandAssistantLanguage::English,
                CommandAssistantLanguage::Italian, CommandAssistantLanguage::Automatic})
            {
                LocalCommandAssistant local(language);
                const auto result = interpret(local,
                    language == CommandAssistantLanguage::Italian ? sample.It : sample.En);
                MIAIA_CHECK(result.Intent == sample.Intent);
                CommandProposal proposal;
                MIAIA_CHECK(CommandAssistant::Propose(result, proposal));
                MIAIA_CHECK(proposal.Command == sample.Command);
                MIAIA_CHECK(local.LearnValidated(result.Text, result.Intent));
                LocalCommandAssistant restored(language);
                std::string error;
                MIAIA_CHECK(restored.ImportCorpus(local.ExportCorpus(), error));
                MIAIA_CHECK(interpret(restored, result.Text).Intent == sample.Intent);
            }
        }
        LocalCommandAssistant local;
        for (const auto text : {"Inspect neuron", "Inspect neuron -2", "Inspect neuron 1.5",
            "Inspect neuron 0", "Inspect connection 2 3", "Compare models 1",
            "Inspect sample -1", "Compare training steps 0",
            "Inspect neuron 999999999999999999999999999999"})
        {
            CommandProposal proposal;
            MIAIA_CHECK(!CommandAssistant::Propose(interpret(local, text), proposal));
        }
        CommandProposal limited;
        MIAIA_CHECK(CommandAssistant::Propose(interpret(local, "Compare models 1 and 2 limit 5"), limited));
        MIAIA_CHECK(limited.Command == "model compare 1 2 5");
        MIAIA_CHECK(CommandAssistant::Propose(interpret(local, "Mostrami riepilogo della rete"), limited));
        MIAIA_CHECK(limited.Command == "summary");
        MIAIA_CHECK(CommandAssistant::Propose(interpret(local, "Inspect neurone 2"), limited));
        MIAIA_CHECK(limited.Command == "inspect neuron 2");
        MIAIA_CHECK(CommandAssistant::Propose(interpret(local, "Diagnostica dataset 10"), limited));
        MIAIA_CHECK(limited.Command == "dataset diagnose 10");
    });

}
