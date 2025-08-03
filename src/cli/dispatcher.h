#pragma once

#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace cli {
    enum class command_status {
        ok,
        exit_requested,
    };

    struct command {
        std::function<command_status(const std::vector<std::string_view>& args)> handler;
        std::string help_text;
        std::string usage_text;
    };

    class dispatcher {
    public:
        void register_command(std::string_view name, command cmd);
        [[nodiscard]] command_status dispatch(std::string_view line);
        [[nodiscard]] const std::map<std::string, command, std::less<>>& get_commands() const;

    private:
        std::map<std::string, command, std::less<>> m_commands;
    };

} // namespace cli
