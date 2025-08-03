#include "dispatcher.h"
#include <print>
#include <ranges>
#include <string_view>

namespace cli {

    void dispatcher::register_command(std::string_view name, command cmd) {
        m_commands.emplace(name, std::move(cmd));
    }

    command_status dispatcher::dispatch(std::string_view line) {
        auto is_space = [](char c) {
            return std::isspace(static_cast<unsigned char>(c));
        };
        auto first = std::ranges::find_if_not(line, is_space);
        if (first == line.end()) {
            return command_status::ok; // empty line
        }
        auto last = std::ranges::find_if_not(line | std::views::reverse, is_space).base();
        line = std::string_view(first, last);

        // split into command and arguments to filter out empty parts from multiple spaces
        auto view = line | std::views::split(' ') | std::views::filter([](auto&& r) {
            return !r.empty();
        });

        std::vector<std::string_view> tokens;
        for (auto&& subrange : view) {
            tokens.emplace_back(subrange.begin(), subrange.end());
        }

        if (tokens.empty()) {
            return command_status::ok;
        }

        const auto& command_name = tokens[0];
        auto it = m_commands.find(command_name);

        if (it != m_commands.end()) {
            std::vector<std::string_view> args(tokens.begin() + 1, tokens.end());
            return it->second.handler(args);
        } else {
            std::println(stderr, "Error: Unknown command '{}'. Type 'help' for a list of commands", command_name);
        }
        return command_status::ok;
    }

    const std::map<std::string, command, std::less<>>& dispatcher::get_commands() const {
        return m_commands;
    }

} // namespace cli
