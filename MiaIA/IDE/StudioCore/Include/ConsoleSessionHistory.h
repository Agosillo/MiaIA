#pragma once

#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace MiaIA::Studio
{
    enum class ConsoleHistorySource { Console, Assistant };
    enum class ConsoleHistoryFilter { Automatic, All, Console, Assistant };

    // Session-local presentation state. Origin is assigned when a request is
    // submitted, not inferred from the UI mode when its result arrives.
    class ConsoleSessionHistory
    {
    public:
        static bool Includes(ConsoleHistoryFilter filter, ConsoleHistorySource active, ConsoleHistorySource origin)
        {
            switch (filter)
            {
            case ConsoleHistoryFilter::All: return true;
            case ConsoleHistoryFilter::Console: return origin == ConsoleHistorySource::Console;
            case ConsoleHistoryFilter::Assistant: return origin == ConsoleHistorySource::Assistant;
            default: return origin == active;
            }
        }

        void Append(ConsoleHistorySource origin, std::string text, bool sharedStatus = false)
        {
            if (!text.empty()) output_.push_back({origin, std::move(text), sharedStatus});
        }

        std::string Output() const
        {
            std::string text;
            for (const auto& entry : output_)
                text += entry.Text;
            return text;
        }

        void ClearOutput()
        {
            output_.clear();
        }

        void RecordInput(ConsoleHistorySource origin, std::string text)
        {
            auto& state = Input(origin);
            if (text.empty()) return;
            if (state.Entries.empty() || state.Entries.back() != text)
            {
                state.Entries.push_back(text);
                submitted_.push_back({origin, std::move(text), false});
            }
            EditInput(origin, {});
        }

        void EditInput(ConsoleHistorySource origin, std::string text)
        {
            auto& state = Input(origin);
            state.Text = std::move(text);
            state.Draft = state.Text;
            state.Cursor = state.Entries.size();
            state.Filter.reset();
        }

        const std::string& CurrentInput(ConsoleHistorySource origin) const
        {
            return input_[static_cast<std::size_t>(origin)].Text;
        }

        std::optional<std::string> Recall(ConsoleHistorySource origin, bool previous,
            ConsoleHistoryFilter filter = ConsoleHistoryFilter::Automatic)
        {
            auto& state = Input(origin);
            std::vector<std::string> entries;
            for (const auto& entry : submitted_)
                if (Includes(filter, origin, entry.Origin)) entries.push_back(entry.Text);
            if (state.Filter != filter)
            {
                state.Cursor = entries.size();
                state.Draft = state.Text;
                state.Filter = filter;
            }
            if (entries.empty()) return std::nullopt;
            if (previous)
            {
                if (state.Cursor == entries.size()) state.Draft = state.Text;
                if (state.Cursor > 0) --state.Cursor;
            }
            else
            {
                if (state.Cursor >= entries.size()) return std::nullopt;
                ++state.Cursor;
            }
            state.Text = state.Cursor == entries.size() ? state.Draft : entries[state.Cursor];
            return state.Text;
        }

    private:
        struct Entry { ConsoleHistorySource Origin; std::string Text; bool SharedStatus{}; };
        struct InputState
        {
            std::vector<std::string> Entries;
            std::size_t Cursor{};
            std::string Draft;
            std::string Text;
            std::optional<ConsoleHistoryFilter> Filter;
        };
        InputState& Input(ConsoleHistorySource origin) { return input_[static_cast<std::size_t>(origin)]; }
        std::vector<Entry> output_;
        std::vector<Entry> submitted_;
        std::array<InputState, 2> input_;
    };
}
