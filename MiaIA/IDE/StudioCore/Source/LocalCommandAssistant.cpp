#include "../Include/LocalCommandAssistant.h"
#include "../Include/AssistantInspectionCatalog.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <limits>
#include <optional>
#include <regex>
#include <set>
#include <sstream>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
    using MiaIA::Studio::CommandAssistantEntity;
    using MiaIA::Studio::CommandAssistantLanguage;
    using MiaIA::Studio::CommandAssistantUnderstanding;

    struct Example
    {
        CommandAssistantLanguage Language;
        std::string_view Intent;
        std::string_view Text;
    };

    constexpr std::array Examples{
        Example{CommandAssistantLanguage::English, "miaia_help", "show help"},
        Example{CommandAssistantLanguage::English, "miaia_help", "list available commands"},
        Example{CommandAssistantLanguage::English, "miaia_help", "what can i do"},
        Example{CommandAssistantLanguage::English, "miaia_help", "explain the console commands"},
        Example{CommandAssistantLanguage::English, "miaia_model_list", "show all models"},
        Example{CommandAssistantLanguage::English, "miaia_model_list", "list the available models"},
        Example{CommandAssistantLanguage::English, "miaia_model_list", "what models are in this project"},
        Example{CommandAssistantLanguage::English, "miaia_model_create", "create a model named atlas"},
        Example{CommandAssistantLanguage::English, "miaia_model_create", "make a new model called iris"},
        Example{CommandAssistantLanguage::English, "miaia_model_select", "select model 2"},
        Example{CommandAssistantLanguage::English, "miaia_model_select", "switch to model 3"},
        Example{CommandAssistantLanguage::English, "miaia_model_select", "use model 1"},
        Example{CommandAssistantLanguage::English, "miaia_network_create", "create a network with 2 inputs 4 neurons per hidden layer 1 hidden layer and 1 output"},
        Example{CommandAssistantLanguage::English, "miaia_network_create", "set up a network with 8 inputs 16 neurons in each hidden layer 3 hidden layers and 4 outputs"},
        Example{CommandAssistantLanguage::English, "miaia_training_start", "start a training session for 100 epochs with learning rate 0.01"},
        Example{CommandAssistantLanguage::English, "miaia_training_start", "start training for 100 epochs with learning rate 0.01 using shuffle order and seed 42"},
        Example{CommandAssistantLanguage::English, "miaia_training_run", "run all training steps"},
        Example{CommandAssistantLanguage::English, "miaia_training_run", "run 5 training steps"},
        Example{CommandAssistantLanguage::English, "miaia_training_pause", "pause training"},
        Example{CommandAssistantLanguage::English, "miaia_training_pause", "temporarily stop training"},
        Example{CommandAssistantLanguage::English, "miaia_training_resume", "resume training"},
        Example{CommandAssistantLanguage::English, "miaia_training_resume", "continue the paused training"},
        Example{CommandAssistantLanguage::English, "miaia_training_status", "show training status"},
        Example{CommandAssistantLanguage::English, "miaia_training_status", "how is the training going"},
        Example{CommandAssistantLanguage::English, "miaia_project_new", "create a new project"},
        Example{CommandAssistantLanguage::English, "miaia_project_new", "start from a clean project"},
        Example{CommandAssistantLanguage::English, "miaia_project_open", "open project c projects iris mai"},
        Example{CommandAssistantLanguage::English, "miaia_project_save", "save the current project"},
        Example{CommandAssistantLanguage::English, "miaia_project_save", "save the project as c projects iris mai"},

        Example{CommandAssistantLanguage::Italian, "miaia_help", "mostra l aiuto"},
        Example{CommandAssistantLanguage::Italian, "miaia_help", "elenca i comandi disponibili"},
        Example{CommandAssistantLanguage::Italian, "miaia_help", "cosa posso fare"},
        Example{CommandAssistantLanguage::Italian, "miaia_help", "spiegami i comandi della console"},
        Example{CommandAssistantLanguage::Italian, "miaia_model_list", "mostra tutti i modelli"},
        Example{CommandAssistantLanguage::Italian, "miaia_model_list", "elenca i modelli disponibili"},
        Example{CommandAssistantLanguage::Italian, "miaia_model_list", "quali modelli ci sono nel progetto"},
        Example{CommandAssistantLanguage::Italian, "miaia_model_create", "crea un modello chiamato atlante"},
        Example{CommandAssistantLanguage::Italian, "miaia_model_create", "crea un nuovo modello chiamato iris"},
        Example{CommandAssistantLanguage::Italian, "miaia_model_select", "seleziona il modello 2"},
        Example{CommandAssistantLanguage::Italian, "miaia_model_select", "passa al modello 3"},
        Example{CommandAssistantLanguage::Italian, "miaia_network_create", "crea una rete con 2 ingressi 4 neuroni per livello nascosto 1 livello nascosto e 1 uscita"},
        Example{CommandAssistantLanguage::Italian, "miaia_network_create", "prepara una rete con 8 ingressi 16 neuroni per livello nascosto 3 livelli nascosti e 4 uscite"},
        Example{CommandAssistantLanguage::Italian, "miaia_training_start", "avvia una sessione di training per 100 epoche con learning rate 0.01"},
        Example{CommandAssistantLanguage::Italian, "miaia_training_start", "avvia il training per 100 epoche con learning rate 0.01 in ordine casuale con seed 42"},
        Example{CommandAssistantLanguage::Italian, "miaia_training_run", "esegui tutti i passi del training"},
        Example{CommandAssistantLanguage::Italian, "miaia_training_run", "esegui 5 passi di training"},
        Example{CommandAssistantLanguage::Italian, "miaia_training_pause", "metti in pausa il training"},
        Example{CommandAssistantLanguage::Italian, "miaia_training_pause", "sospendi l addestramento"},
        Example{CommandAssistantLanguage::Italian, "miaia_training_resume", "riprendi il training"},
        Example{CommandAssistantLanguage::Italian, "miaia_training_resume", "continua il training in pausa"},
        Example{CommandAssistantLanguage::Italian, "miaia_training_status", "mostra lo stato del training"},
        Example{CommandAssistantLanguage::Italian, "miaia_training_status", "come sta andando il training"},
        Example{CommandAssistantLanguage::Italian, "miaia_project_new", "crea un nuovo progetto"},
        Example{CommandAssistantLanguage::Italian, "miaia_project_new", "parti da un progetto pulito"},
        Example{CommandAssistantLanguage::Italian, "miaia_project_open", "apri il progetto c progetti iris mai"},
        Example{CommandAssistantLanguage::Italian, "miaia_project_save", "salva il progetto corrente"},
        Example{CommandAssistantLanguage::Italian, "miaia_project_save", "salva il progetto come c progetti iris mai"}
    };

    constexpr std::array<std::string_view, 13> IntentNames{
        "miaia_help",
        "miaia_model_create",
        "miaia_model_list",
        "miaia_model_select",
        "miaia_network_create",
        "miaia_project_new",
        "miaia_project_open",
        "miaia_project_save",
        "miaia_training_pause",
        "miaia_training_resume",
        "miaia_training_run",
        "miaia_training_start",
        "miaia_training_status"};

    constexpr std::size_t MaximumSerializedCorpusBytes = 1024 * 1024;
    constexpr std::size_t MaximumCorpusEntries = 2000;
    constexpr std::size_t MaximumPhraseBytes = 2048;

    bool IsSupportedIntent(const std::string_view intent)
    {
        const auto& intents = MiaIA::Studio::LocalCommandAssistant::SupportedIntents();
        return std::find(intents.begin(), intents.end(), intent) != intents.end();
    }

    bool AcceptsLanguage(
        const CommandAssistantLanguage selected,
        const CommandAssistantLanguage candidate)
    {
        return selected == CommandAssistantLanguage::Automatic ||
            selected == candidate;
    }

    std::string_view LanguageCode(const CommandAssistantLanguage language)
    {
        switch (language)
        {
        case CommandAssistantLanguage::English:
            return "en";
        case CommandAssistantLanguage::Italian:
            return "it";
        default:
            return "auto";
        }
    }

    std::optional<CommandAssistantLanguage> ParseLanguage(
        const std::string_view code)
    {
        if (code == "en") return CommandAssistantLanguage::English;
        if (code == "it") return CommandAssistantLanguage::Italian;
        if (code == "auto") return CommandAssistantLanguage::Automatic;
        return std::nullopt;
    }

    char HexDigit(const unsigned int value)
    {
        return static_cast<char>(value < 10 ? '0' + value : 'a' + value - 10);
    }

    std::string HexEncode(const std::string_view value)
    {
        std::string result;
        result.reserve(value.size() * 2);
        for (const unsigned char character : value)
        {
            result.push_back(HexDigit(character >> 4));
            result.push_back(HexDigit(character & 0x0f));
        }
        return result;
    }

    int HexValue(const char value)
    {
        if (value >= '0' && value <= '9') return value - '0';
        if (value >= 'a' && value <= 'f') return value - 'a' + 10;
        if (value >= 'A' && value <= 'F') return value - 'A' + 10;
        return -1;
    }

    bool HexDecode(const std::string_view value, std::string& result)
    {
        if ((value.size() % 2) != 0 ||
            value.size() > MaximumPhraseBytes * 2)
        {
            return false;
        }

        result.clear();
        result.reserve(value.size() / 2);
        for (std::size_t index = 0; index < value.size(); index += 2)
        {
            const int high = HexValue(value[index]);
            const int low = HexValue(value[index + 1]);
            if (high < 0 || low < 0)
            {
                result.clear();
                return false;
            }
            result.push_back(static_cast<char>((high << 4) | low));
        }
        return true;
    }

    std::vector<std::string_view> SplitTabs(const std::string_view line)
    {
        std::vector<std::string_view> fields;
        std::size_t begin{};
        while (begin <= line.size())
        {
            const std::size_t end = line.find('\t', begin);
            fields.push_back(line.substr(
                begin,
                end == std::string_view::npos ? line.size() - begin : end - begin));
            if (end == std::string_view::npos) break;
            begin = end + 1;
        }
        return fields;
    }

    void ReplaceAll(
        std::string& value,
        const std::string_view from,
        const std::string_view to)
    {
        std::size_t position{};
        while ((position = value.find(from, position)) != std::string::npos)
        {
            value.replace(position, from.size(), to);
            position += to.size();
        }
    }

    std::string Normalize(std::string value)
    {
        constexpr std::array<
            std::pair<std::string_view, std::string_view>, 16> accents{
                std::pair<std::string_view, std::string_view>{"à", "a"},
                std::pair<std::string_view, std::string_view>{"á", "a"},
                std::pair<std::string_view, std::string_view>{"è", "e"},
                std::pair<std::string_view, std::string_view>{"é", "e"},
                std::pair<std::string_view, std::string_view>{"ì", "i"},
                std::pair<std::string_view, std::string_view>{"ò", "o"},
                std::pair<std::string_view, std::string_view>{"ó", "o"},
                std::pair<std::string_view, std::string_view>{"ù", "u"},
                std::pair<std::string_view, std::string_view>{"À", "a"},
                std::pair<std::string_view, std::string_view>{"Á", "a"},
                std::pair<std::string_view, std::string_view>{"È", "e"},
                std::pair<std::string_view, std::string_view>{"É", "e"},
                std::pair<std::string_view, std::string_view>{"Ì", "i"},
                std::pair<std::string_view, std::string_view>{"Ò", "o"},
                std::pair<std::string_view, std::string_view>{"Ó", "o"},
                std::pair<std::string_view, std::string_view>{"Ù", "u"}};
        for (const auto& [from, to] : accents)
        {
            ReplaceAll(value, from, to);
        }

        std::string result;
        result.reserve(value.size());
        bool previousSpace = true;
        for (const unsigned char character : value)
        {
            if (std::isalnum(character) || character == '.' ||
                character == ':' || character == '\\' ||
                character == '/' || character == '_' || character == '-')
            {
                result.push_back(static_cast<char>(std::tolower(character)));
                previousSpace = false;
            }
            else if (!previousSpace)
            {
                result.push_back(' ');
                previousSpace = true;
            }
        }
        while (!result.empty() && result.back() == ' ')
        {
            result.pop_back();
        }

        // Italian commonly attaches the indirect-object pronoun to an
        // imperative ("mostrami", "elencami", ...).  Canonicalizing those
        // forms keeps the command corpus compact while accepting natural
        // requests that mean exactly the same thing.
        constexpr std::array italianImperatives{
            std::pair<std::string_view, std::string_view>{"mostrami", "mostra"},
            std::pair<std::string_view, std::string_view>{"elencami", "elenca"},
            std::pair<std::string_view, std::string_view>{"visualizzami", "visualizza"},
            std::pair<std::string_view, std::string_view>{"selezionami", "seleziona"},
            std::pair<std::string_view, std::string_view>{"creami", "crea"},
            std::pair<std::string_view, std::string_view>{"aprimi", "apri"},
            std::pair<std::string_view, std::string_view>{"salvami", "salva"},
            std::pair<std::string_view, std::string_view>{"dimmi", "dammi"}};

        std::istringstream stream(result);
        std::ostringstream canonical;
        std::string token;
        bool first = true;
        while (stream >> token)
        {
            for (const auto& [alias, command] : italianImperatives)
            {
                if (token == alias)
                {
                    token = command;
                    break;
                }
            }
            if (!first)
                canonical << ' ';
            canonical << token;
            first = false;
        }
        return canonical.str();
    }

    std::set<std::string> Tokens(const std::string& normalized)
    {
        static const std::set<std::string> stopWords{
            "a", "an", "the", "with", "in", "to", "of", "and",
            "all", "me", "my", "i", "is", "are", "for", "from",
            "un", "una", "uno", "il", "lo", "la", "i", "gli", "le",
            "con", "in", "di", "e", "al", "alla", "del", "della",
            "tutti", "tutte", "mi", "per", "da"};
        std::set<std::string> result;
        std::istringstream stream(normalized);
        std::string token;
        while (stream >> token)
        {
            if (!stopWords.contains(token))
            {
                result.insert(std::move(token));
            }
        }
        return result;
    }

    double Similarity(const std::set<std::string>& left,
        const std::set<std::string>& right)
    {
        if (left.empty() || right.empty())
        {
            return 0.0;
        }
        std::size_t common{};
        for (const auto& token : left)
        {
            common += right.contains(token) ? 1u : 0u;
        }
        return 2.0 * static_cast<double>(common) /
            static_cast<double>(left.size() + right.size());
    }

    bool ContainsAny(const std::string& text,
        std::initializer_list<std::string_view> values)
    {
        return std::any_of(values.begin(), values.end(),
            [&text](const std::string_view value)
            {
                std::size_t position{};
                while ((position = text.find(value, position)) !=
                    std::string::npos)
                {
                    const bool startsAtBoundary = position == 0 ||
                        !std::isalnum(static_cast<unsigned char>(
                            text[position - 1]));
                    const std::size_t after = position + value.size();
                    const bool endsAtBoundary = after == text.size() ||
                        !std::isalnum(static_cast<unsigned char>(text[after]));
                    if (startsAtBoundary && endsAtBoundary)
                        return true;
                    position = after;
                }
                return false;
            });
    }

    std::string RuleIntent(const std::string& text)
    {
        const bool inspect = ContainsAny(text, {"inspect", "ispeziona", "show", "mostra", "vedi"});
        const bool compare = ContainsAny(text, {"compare", "confronta"});
        const bool list = ContainsAny(text, {"list", "elenca", "show", "mostra"});
        const bool step = ContainsAny(text, {"step", "steps", "passo", "passi"});
        if (ContainsAny(text, {"checkpoint", "checkpoints"}))
        {
            if (compare) return "miaia_checkpoint_compare";
            if (ContainsAny(text, {"inspect", "ispeziona"})) return "miaia_checkpoint_inspect";
            if (list) return "miaia_checkpoint_list";
        }
        if (list && ContainsAny(text, {"breakpoint", "breakpoints"}))
            return "miaia_breakpoint_list";
        if (inspect && ContainsAny(text, {"debug"}) &&
            ContainsAny(text, {"status", "stato"})) return "miaia_debug_status";
        if (ContainsAny(text, {"training", "addestramento"}))
        {
            if (compare && step) return "miaia_training_compare";
            if (inspect && step) return "miaia_training_inspect";
            if (list && ContainsAny(text, {"history", "cronologia"}))
                return "miaia_training_history";
        }
        if (compare && ContainsAny(text, {"models", "modelli", "model", "modello"}))
            return "miaia_model_compare";
        if (inspect && ContainsAny(text, {"neuron", "neurone"})) return "miaia_neuron_inspect";
        if (inspect && ContainsAny(text, {"connection", "connessione"})) return "miaia_connection_inspect";
        if (inspect && ContainsAny(text, {"sample", "campione"})) return "miaia_dataset_inspect";
        if (ContainsAny(text, {"dataset"}))
        {
            if (ContainsAny(text, {"diagnose", "diagnostica"})) return "miaia_dataset_diagnose";
            if (list && ContainsAny(text, {"summary", "riepilogo"})) return "miaia_dataset_summary";
        }
        if (inspect && ContainsAny(text, {"project", "progetto"}) &&
            ContainsAny(text, {"info", "information", "informazioni"})) return "miaia_project_info";
        if (ContainsAny(text, {"network", "rete"}))
        {
            if (inspect && ContainsAny(text, {"summary", "riepilogo"})) return "miaia_network_summary";
            if (ContainsAny(text, {"inspect", "ispeziona"})) return "miaia_network_inspect";
        }
        const bool model = ContainsAny(text,
            {"model", "models", "modello", "modelli"});
        const bool project = ContainsAny(text, {"project", "progetto"});
        const bool training = ContainsAny(text,
            {"training", "addestramento", "epoche", "epochs"});

        if (ContainsAny(text, {"network", "rete"}) &&
            ContainsAny(text, {"create", "set up", "build", "crea", "prepara"}))
            return "miaia_network_create";
        if (model && ContainsAny(text,
            {"create", "make", "add", "crea", "aggiungi"}))
            return "miaia_model_create";
        if (model && ContainsAny(text,
            {"select", "switch", "use", "open model", "seleziona", "passa", "usa"}))
            return "miaia_model_select";
        if (model && ContainsAny(text,
            {"show", "list", "display", "which", "what models", "give", "see",
             "mostra", "elenca", "visualizza", "quali", "dammi", "vedere"}))
            return "miaia_model_list";
        if (training && ContainsAny(text,
            {"status", "progress", "running", "going", "state", "information",
             "stato", "progresso", "esecuzione", "andando", "informazioni", "attiva"}))
            return "miaia_training_status";
        if (training && ContainsAny(text,
            {"pause", "suspend", "hold", "stop", "interrupt", "pausa",
             "sospendi", "attesa", "momentaneamente", "ferma", "arresta",
             "interrompi"}))
            return "miaia_training_pause";
        if (training && ContainsAny(text,
            {"run", "execute", "until completion", "remaining", "steps", "whole",
             "esegui", "fino alla fine", "rimanenti", "passi", "completo", "tutta"}))
            return "miaia_training_run";
        if (training && ContainsAny(text,
            {"resume", "paused", "again", "continue", "carry on", "proceed",
             "riprendi", "sospeso", "ripartire", "in pausa", "continua",
             "prosegui", "riavvia"}))
            return "miaia_training_resume";
        if (training && ContainsAny(text,
            {"start", "begin", "avvia", "inizia"}))
            return "miaia_training_start";
        if (project && ContainsAny(text, {"save", "write", "salva", "scrivi"}))
            return "miaia_project_save";
        if (project && ContainsAny(text,
            {"new", "blank", "fresh", "clean", "nuovo", "vuoto", "pulito"}))
            return "miaia_project_new";
        if (project && ContainsAny(text, {"open", "load", "apri", "carica"}))
            return "miaia_project_open";
        if (ContainsAny(text,
            {"help", "commands", "what can i do", "how do i use", "aiuto",
             "comandi", "cosa posso fare", "come si usa", "guida"}))
            return "miaia_help";
        return {};
    }

    void AddEntity(CommandAssistantUnderstanding& result,
        std::string name, std::string value, const double confidence = 1.0)
    {
        CommandAssistantEntity entity;
        entity.Name = name;
        entity.Role = std::move(name);
        entity.Value = std::move(value);
        entity.Confidence = confidence;
        result.Entities.push_back(std::move(entity));
    }

    std::vector<std::string> Numbers(const std::string& text)
    {
        static const std::regex number(R"((?:^|[^A-Za-z0-9_.])([0-9]+(?:\.[0-9]+)?))");
        std::vector<std::string> result;
        for (std::sregex_iterator it(text.begin(), text.end(), number), end;
            it != end; ++it)
        {
            result.push_back((*it)[1].str());
        }
        return result;
    }

    std::string ValueAfter(const std::string& original,
        std::initializer_list<std::string_view> markers)
    {
        const std::string normalized = Normalize(original);
        for (const auto marker : markers)
        {
            const std::size_t position = normalized.find(marker);
            if (position == std::string::npos)
                continue;
            // Markers are ASCII and normalization preserves byte positions for
            // the command vocabulary used before the free-text value.
            std::string value = original.substr(position + marker.size());
            const auto first = value.find_first_not_of(" \t\"'");
            const auto last = value.find_last_not_of(" \t\"'.,?!");
            return first == std::string::npos || last == std::string::npos
                ? std::string{}
                : value.substr(first, last - first + 1);
        }
        return {};
    }

    std::string ProjectPath(const std::string& original)
    {
        static const std::regex windowsPath(
            R"(([A-Za-z]:\\[^\r\n\"]+?\.mai)(?:[\s\"']|$))",
            std::regex::icase);
        static const std::regex portablePath(
            R"(((?:/|\./|\.\./)?[^\s\"']+\.mai)(?:[\s\"']|$))",
            std::regex::icase);
        std::smatch match;
        if (std::regex_search(original, match, windowsPath))
            return match[1].str();
        if (std::regex_search(original, match, portablePath))
            return match[1].str();
        return {};
    }

    void ExtractEntities(CommandAssistantUnderstanding& result)
    {
        for (const auto& entry : MiaIA::Studio::AssistantInspectionCatalog)
        {
            if (entry.Intent != result.Intent) continue;
            // Retain signs and decimal points so validation rejects invalid
            // integer IDs instead of silently changing their meaning.
            static const std::regex numeric(R"([+-]?[0-9]+(?:\.[0-9]+)?)");
            std::size_t index{};
            for (std::sregex_iterator it(result.Text.begin(), result.Text.end(), numeric), end;
                it != end; ++it, ++index)
            {
                if (index >= entry.Roles.size() || entry.Roles[index].empty())
                {
                    result.Error = "Too many numeric arguments for this inspection command.";
                    return;
                }
                auto role = entry.Roles[index];
                if (role.front() == '?') role.remove_prefix(1);
                AddEntity(result, std::string(role), it->str());
            }
            return;
        }
        const auto numbers = Numbers(result.Text);
        if (result.Intent == "miaia_model_create")
        {
            const std::string name = ValueAfter(result.Text,
                {"named ", "called ", "chiamato ", "chiamata ", "nome "});
            if (!name.empty()) AddEntity(result, "model_name", name);
        }
        else if (result.Intent == "miaia_model_select")
        {
            if (!numbers.empty()) AddEntity(result, "model_id", numbers.front());
        }
        else if (result.Intent == "miaia_network_create")
        {
            // Topology values follow the Console create order. Keeping any
            // values that were supplied is important: a partially specified
            // topology must fail validation instead of silently falling back
            // to the bare `create` defaults.
            if (!numbers.empty()) AddEntity(result, "inputs", numbers[0]);
            if (numbers.size() >= 2)
                AddEntity(result, "hidden_width", numbers[1]);
            if (numbers.size() >= 3)
                AddEntity(result, "hidden_layers", numbers[2]);
            if (numbers.size() >= 4)
                AddEntity(result, "outputs", numbers[3]);
        }
        else if (result.Intent == "miaia_training_start")
        {
            if (numbers.size() >= 2)
            {
                AddEntity(result, "epochs", numbers[0]);
                AddEntity(result, "learning_rate", numbers[1]);
            }
            const std::string normalized = Normalize(result.Text);
            if (ContainsAny(normalized,
                {"shuffle", "shuffled", "random", "mescola", "mescolato", "mischia", "casuale"}))
            {
                AddEntity(result, "sample_order", "shuffle");
                const std::size_t seed = normalized.find("seed");
                if (seed != std::string::npos)
                {
                    const auto seedNumbers = Numbers(normalized.substr(seed));
                    if (!seedNumbers.empty()) AddEntity(result, "seed", seedNumbers.front());
                }
            }
        }
        else if (result.Intent == "miaia_training_run")
        {
            if (!numbers.empty()) AddEntity(result, "steps", numbers.front());
        }
        else if (result.Intent == "miaia_project_open" ||
            result.Intent == "miaia_project_save")
        {
            const std::string path = ProjectPath(result.Text);
            if (!path.empty()) AddEntity(result, "project_path", path);
        }
    }
}

MiaIA::Studio::LocalCommandAssistant::LocalCommandAssistant(
    const CommandAssistantLanguage language)
    : language_(language)
{
}

bool MiaIA::Studio::LocalCommandAssistant::IsAvailable() const
{
    return true;
}

std::string MiaIA::Studio::LocalCommandAssistant::AvailabilityMessage() const
{
    if (language_ == CommandAssistantLanguage::Automatic)
        return "MiaIA Local is ready for English, Italian, or mixed requests.";
    return language_ == CommandAssistantLanguage::Italian
        ? "MiaIA Local e pronto."
        : "MiaIA Local is ready.";
}

bool MiaIA::Studio::LocalCommandAssistant::Interpret(
    const std::string& text,
    CommandAssistantCompletion completion)
{
    if (!completion)
        return false;

    CommandAssistantUnderstanding result;
    result.Text = text;
    const std::string normalized = Normalize(text);
    if (normalized.empty())
    {
        result.Error = language_ == CommandAssistantLanguage::Italian
            ? "Inserisci una richiesta MiaIA."
            : language_ == CommandAssistantLanguage::English
                ? "Enter a MiaIA request."
                : "Enter a MiaIA request / Inserisci una richiesta MiaIA.";
        completion(std::move(result));
        return true;
    }

    for (const auto& example : learnedExamples_)
    {
        if (AcceptsLanguage(language_, example.Language) &&
            Normalize(example.Text) == normalized)
        {
            result.Intent = example.Intent;
            result.Confidence = 1.0;
            ExtractEntities(result);
            completion(std::move(result));
            return true;
        }
    }

    // A phrase explicitly waiting for review must not keep producing the
    // same rule-based proposal after the user marked it as incorrect.
    for (const auto& pending : pendingPhrases_)
    {
        if (AcceptsLanguage(language_, pending.Language) &&
            Normalize(pending.Text) == normalized)
        {
            result.Error = language_ == CommandAssistantLanguage::Italian
                ? "Frase in attesa di classificazione."
                : language_ == CommandAssistantLanguage::English
                    ? "Phrase waiting for classification."
                    : "Phrase waiting for classification / Frase in attesa di classificazione.";
            completion(std::move(result));
            return true;
        }
    }

    double bestScore{};
    std::string bestIntent;
    const auto inputTokens = Tokens(normalized);
    for (const Example& example : Examples)
    {
        if (!AcceptsLanguage(language_, example.Language))
            continue;
        const std::string candidate = Normalize(std::string(example.Text));
        const double score = normalized == candidate
            ? 1.0
            : Similarity(inputTokens, Tokens(candidate));
        if (score > bestScore)
        {
            bestScore = score;
            bestIntent = example.Intent;
        }
    }

    for (const auto& example : learnedExamples_)
    {
        if (!AcceptsLanguage(language_, example.Language))
            continue;
        const std::string candidate = Normalize(example.Text);
        const double score = Similarity(inputTokens, Tokens(candidate));
        if (score > bestScore)
        {
            bestScore = score;
            bestIntent = example.Intent;
        }
    }

    const std::string ruleIntent = RuleIntent(normalized);
    for (const auto& entry : AssistantInspectionCatalog)
    {
        for (const auto language : {CommandAssistantLanguage::English, CommandAssistantLanguage::Italian})
        {
            if (!AcceptsLanguage(language_, language)) continue;
            const std::string candidate = Normalize(std::string(
                language == CommandAssistantLanguage::Italian ? entry.Italian : entry.English));
            const double score = normalized == candidate
                ? 1.0 : Similarity(inputTokens, Tokens(candidate));
            if (score > bestScore)
            {
                bestScore = score;
                bestIntent = entry.Intent;
            }
        }
    }
    if (!ruleIntent.empty())
    {
        result.Intent = ruleIntent;
        result.Confidence = bestIntent == ruleIntent
            ? std::max(0.85, bestScore)
            : 0.85;
    }
    else if (bestScore >= 0.74)
    {
        result.Intent = bestIntent;
        result.Confidence = std::min(0.90, bestScore);
    }
    else
    {
        result.Error = language_ == CommandAssistantLanguage::Italian
            ? "Nessun comando MiaIA riconosciuto."
            : language_ == CommandAssistantLanguage::English
                ? "No MiaIA command was recognized."
                : "No MiaIA command was recognized / Nessun comando MiaIA riconosciuto.";
        completion(std::move(result));
        return true;
    }

    ExtractEntities(result);
    completion(std::move(result));
    return true;
}

MiaIA::Studio::CommandAssistantLanguage
MiaIA::Studio::LocalCommandAssistant::Language() const
{
    return language_;
}

void MiaIA::Studio::LocalCommandAssistant::SetLanguage(
    const CommandAssistantLanguage language)
{
    language_ = language;
}

MiaIA::Studio::CommandAssistantLanguage
MiaIA::Studio::LocalCommandAssistant::InferLanguage(
    const std::string_view text,
    const std::string_view intent) const
{
    if (language_ != CommandAssistantLanguage::Automatic)
        return language_;

    const std::string normalized = Normalize(std::string(text));
    const auto inputTokens = Tokens(normalized);
    double englishScore{};
    double italianScore{};

    for (const Example& example : Examples)
    {
        if (!intent.empty() && example.Intent != intent)
            continue;

        const std::string candidate = Normalize(std::string(example.Text));
        const double score = normalized == candidate
            ? 1.0
            : Similarity(inputTokens, Tokens(candidate));
        if (example.Language == CommandAssistantLanguage::Italian)
            italianScore = std::max(italianScore, score);
        else
            englishScore = std::max(englishScore, score);
    }

    constexpr double MinimumLanguageAdvantage = 0.10;
    if (englishScore >= italianScore + MinimumLanguageAdvantage)
        return CommandAssistantLanguage::English;
    if (italianScore >= englishScore + MinimumLanguageAdvantage)
        return CommandAssistantLanguage::Italian;
    return CommandAssistantLanguage::Automatic;
}

bool MiaIA::Studio::LocalCommandAssistant::LearnValidated(
    std::string text,
    std::string intent)
{
    const std::string normalized = Normalize(text);
    if (normalized.empty() || text.size() > MaximumPhraseBytes ||
        !IsSupportedIntent(intent))
    {
        return false;
    }

    const CommandAssistantLanguage language = InferLanguage(text, intent);
    for (auto& example : learnedExamples_)
    {
        if (example.Language == language && example.Intent == intent &&
            Normalize(example.Text) == normalized)
        {
            if (example.Confirmations <
                std::numeric_limits<std::uint32_t>::max())
            {
                ++example.Confirmations;
            }
            RemovePending(text);
            return true;
        }
    }

    if (learnedExamples_.size() >= MaximumCorpusEntries)
        return false;

    learnedExamples_.push_back(LocalCommandAssistantExample{
        language, std::move(text), std::move(intent), 1});
    RemovePending(learnedExamples_.back().Text);
    return true;
}

bool MiaIA::Studio::LocalCommandAssistant::RecordUnknown(std::string text)
{
    const std::string normalized = Normalize(text);
    if (normalized.empty() || text.size() > MaximumPhraseBytes)
        return false;

    for (const auto& example : learnedExamples_)
    {
        if (Normalize(example.Text) == normalized)
            return false;
    }
    for (const auto& pending : pendingPhrases_)
    {
        if (Normalize(pending.Text) == normalized)
            return true;
    }
    if (pendingPhrases_.size() >= MaximumCorpusEntries)
        return false;

    pendingPhrases_.push_back(LocalCommandAssistantPendingPhrase{
        InferLanguage(text), std::move(text)});
    return true;
}

bool MiaIA::Studio::LocalCommandAssistant::MarkIncorrect(std::string text)
{
    const std::string normalized = Normalize(text);
    if (normalized.empty())
        return false;

    std::erase_if(learnedExamples_,
        [&normalized](const LocalCommandAssistantExample& example)
        {
            return Normalize(example.Text) == normalized;
        });
    if (!RecordUnknown(text))
        return false;

    // Keep the phrase the user just rejected as the most recent pending item.
    // The review UI presents that item first, making correction feel like one
    // continuous operation while preserving all older unknown phrases.
    const auto pending = std::find_if(
        pendingPhrases_.begin(),
        pendingPhrases_.end(),
        [&normalized](const LocalCommandAssistantPendingPhrase& value)
        {
            return Normalize(value.Text) == normalized;
        });
    if (pending != pendingPhrases_.end() &&
        pending != std::prev(pendingPhrases_.end()))
    {
        LocalCommandAssistantPendingPhrase prioritized = std::move(*pending);
        pendingPhrases_.erase(pending);
        pendingPhrases_.push_back(std::move(prioritized));
    }
    return true;
}

bool MiaIA::Studio::LocalCommandAssistant::ClassifyPending(
    const std::string_view text,
    std::string intent)
{
    if (!IsSupportedIntent(intent))
        return false;

    const std::string normalized = Normalize(std::string(text));
    const auto pending = std::find_if(
        pendingPhrases_.begin(),
        pendingPhrases_.end(),
        [&normalized](const LocalCommandAssistantPendingPhrase& value)
        {
            return Normalize(value.Text) == normalized;
        });
    if (pending == pendingPhrases_.end() ||
        learnedExamples_.size() >= MaximumCorpusEntries)
    {
        return false;
    }

    const CommandAssistantLanguage language = pending->Language;
    std::string phrase = pending->Text;
    pendingPhrases_.erase(pending);
    learnedExamples_.push_back(LocalCommandAssistantExample{
        language, std::move(phrase), std::move(intent), 1});
    return true;
}

bool MiaIA::Studio::LocalCommandAssistant::RemovePending(
    const std::string_view text)
{
    const std::string normalized = Normalize(std::string(text));
    const std::size_t previousSize = pendingPhrases_.size();
    std::erase_if(pendingPhrases_,
        [&normalized](const LocalCommandAssistantPendingPhrase& pending)
        {
            return Normalize(pending.Text) == normalized;
        });
    return pendingPhrases_.size() != previousSize;
}

bool MiaIA::Studio::LocalCommandAssistant::ReassignValidated(
    const std::string_view text,
    std::string intent)
{
    if (!IsSupportedIntent(intent))
        return false;

    const std::string normalized = Normalize(std::string(text));
    const auto first = std::find_if(
        learnedExamples_.begin(),
        learnedExamples_.end(),
        [&normalized](const LocalCommandAssistantExample& example)
        {
            return Normalize(example.Text) == normalized;
        });
    if (first == learnedExamples_.end())
        return false;

    const std::size_t originalIndex = static_cast<std::size_t>(
        std::distance(learnedExamples_.begin(), first));
    LocalCommandAssistantExample reassigned = *first;
    reassigned.Intent = std::move(intent);
    std::erase_if(learnedExamples_,
        [&normalized](const LocalCommandAssistantExample& example)
        {
            return Normalize(example.Text) == normalized;
        });
    learnedExamples_.insert(
        learnedExamples_.begin() + std::min(
            originalIndex,
            learnedExamples_.size()),
        std::move(reassigned));
    RemovePending(text);
    return true;
}

bool MiaIA::Studio::LocalCommandAssistant::RemoveValidated(
    const std::string_view text)
{
    const std::string normalized = Normalize(std::string(text));
    const std::size_t previousSize = learnedExamples_.size();
    std::erase_if(learnedExamples_,
        [&normalized](const LocalCommandAssistantExample& example)
        {
            return Normalize(example.Text) == normalized;
        });
    return learnedExamples_.size() != previousSize;
}

void MiaIA::Studio::LocalCommandAssistant::ClearCorpus()
{
    learnedExamples_.clear();
    pendingPhrases_.clear();
}

const std::vector<MiaIA::Studio::LocalCommandAssistantExample>&
MiaIA::Studio::LocalCommandAssistant::LearnedExamples() const
{
    return learnedExamples_;
}

const std::vector<MiaIA::Studio::LocalCommandAssistantPendingPhrase>&
MiaIA::Studio::LocalCommandAssistant::PendingPhrases() const
{
    return pendingPhrases_;
}

std::string MiaIA::Studio::LocalCommandAssistant::ExportCorpus() const
{
    std::ostringstream stream;
    stream << "MIAIA_LOCAL_NLU\t1\n";
    for (const auto& example : learnedExamples_)
    {
        stream << "V\t" << LanguageCode(example.Language) << '\t'
            << example.Confirmations << '\t' << HexEncode(example.Text)
            << '\t' << HexEncode(example.Intent) << '\n';
    }
    for (const auto& pending : pendingPhrases_)
    {
        stream << "P\t" << LanguageCode(pending.Language) << '\t'
            << HexEncode(pending.Text) << '\n';
    }
    return stream.str();
}

bool MiaIA::Studio::LocalCommandAssistant::ImportCorpus(
    const std::string_view serialized,
    std::string& error)
{
    error.clear();
    if (serialized.size() > MaximumSerializedCorpusBytes)
    {
        error = "The local corpus is larger than the supported limit.";
        return false;
    }

    std::istringstream stream{std::string(serialized)};
    std::string line;
    if (!std::getline(stream, line))
    {
        error = "The local corpus is empty.";
        return false;
    }
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line != "MIAIA_LOCAL_NLU\t1")
    {
        error = "The local corpus header or version is invalid.";
        return false;
    }

    std::vector<LocalCommandAssistantExample> learned;
    std::vector<LocalCommandAssistantPendingPhrase> pending;
    std::size_t lineNumber = 1;
    while (std::getline(stream, line))
    {
        ++lineNumber;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        const auto fields = SplitTabs(line);
        if (fields.empty()) continue;

        const auto fail = [&]()
        {
            error = "Invalid local corpus entry at line " +
                std::to_string(lineNumber) + '.';
            return false;
        };

        if (fields[0] == "V")
        {
            if (fields.size() != 5 ||
                learned.size() + pending.size() >= MaximumCorpusEntries)
                return fail();
            const auto language = ParseLanguage(fields[1]);
            std::uint32_t confirmations{};
            const auto parsed = std::from_chars(
                fields[2].data(),
                fields[2].data() + fields[2].size(),
                confirmations);
            std::string text;
            std::string intent;
            if (!language || parsed.ec != std::errc{} ||
                parsed.ptr != fields[2].data() + fields[2].size() ||
                confirmations == 0 || !HexDecode(fields[3], text) ||
                !HexDecode(fields[4], intent) || Normalize(text).empty() ||
                !IsSupportedIntent(intent))
            {
                return fail();
            }
            learned.push_back(LocalCommandAssistantExample{
                *language, std::move(text), std::move(intent), confirmations});
        }
        else if (fields[0] == "P")
        {
            if (fields.size() != 3 ||
                learned.size() + pending.size() >= MaximumCorpusEntries)
                return fail();
            const auto language = ParseLanguage(fields[1]);
            std::string text;
            if (!language || !HexDecode(fields[2], text) ||
                Normalize(text).empty())
            {
                return fail();
            }
            pending.push_back(LocalCommandAssistantPendingPhrase{
                *language, std::move(text)});
        }
        else
        {
            return fail();
        }
    }

    learnedExamples_ = std::move(learned);
    pendingPhrases_ = std::move(pending);
    return true;
}

const std::vector<std::string_view>&
MiaIA::Studio::LocalCommandAssistant::SupportedIntents()
{
    static const std::vector<std::string_view> intents = []
    {
        std::vector<std::string_view> result(IntentNames.begin(), IntentNames.end());
        for (const auto& entry : AssistantInspectionCatalog)
            result.push_back(entry.Intent);
        return result;
    }();
    return intents;
}
