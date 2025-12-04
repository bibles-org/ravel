
#include "repl.h"
#include "commands.h"

#include <app/ctx.h>
#include <iostream>
#include <print>
#include <string>

namespace cli {
    repl::repl() {
        register_all_commands(m_dispatcher);
    }

    void repl::run() {
        std::println("Ravel Command Line Interface. Type 'help' for commands.");
        std::string line;
        bool running = true;

        while (running) {
            if (app::active_target) {
                std::print("(ravel: {})> ", app::active_target->get_name());
            } else {
                std::print("(ravel)> ");
            }

            if (!std::getline(std::cin, line)) {
                break; // eof (ctrl+d)
            }

            if (!line.empty()) {
                if (m_dispatcher.dispatch(line) == command_status::exit_requested) {
                    running = false;
                }
            }
        }
        std::println("\nExiting ravel.");
    }

} // namespace cli
