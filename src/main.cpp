#include <app/application.h>
#include <cli/repl.h>
#include <core/process.h>
#include <print>
#include <string_view>
#include <vector>
#include <print>

// forward declaration
namespace app {
    std::unique_ptr<core::process> proc = nullptr;
}

int main(int argc, char** argv) {
    app::proc = std::make_unique<core::process>();

    std::vector<std::string_view> args(argv, argv + argc);
    bool cli_mode = false;
    for (const auto& arg : args) {
        if (arg == "--cli" || arg == "-c") {
            cli_mode = true;
            break;
        }
    }

    if (cli_mode) {
        cli::repl command_line_interface;
        command_line_interface.run();
    } else {
        auto app_instance = app::application::create(1600, 900, "ravel");
        if (!app_instance) {
            std::println(stderr, "Fatal: Failed to create GUI application window");
            return -1;
        }
        app_instance->run();
    }

    return 0;
}
