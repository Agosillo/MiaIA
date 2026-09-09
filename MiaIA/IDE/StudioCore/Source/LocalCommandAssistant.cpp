#include "../Include/LocalCommandAssistant.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
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
        const bool model = ContainsAny(text, {"model", "modello", "modelli"});
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
            if (numbers.size() >= 4)
            {
                AddEntity(result, "inputs", numbers[0]);
                AddEntity(result, "hidden_width", numbers[1]);
                AddEntity(result, "hidden_layers", numbers[2]);
                AddEntity(result, "outputs", numbers[3]);
            }
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
            : "Enter a MiaIA request.";
        completion(std::move(result));
        return true;
    }

    double bestScore{};
    std::string bestIntent;
    const auto inputTokens = Tokens(normalized);
    for (const Example& example : Examples)
    {
        if (example.Language != language_)
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

    const std::string ruleIntent = RuleIntent(normalized);
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
            : "No MiaIA command was recognized.";
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
