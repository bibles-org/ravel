#include <app/application.h>
#include <app/ctx.h>
#include <cli/repl.h>
#include <core/file_target.h>
#include <core/process.h>
#include <print>
#include <string_view>
#include <vector>

int main(int argc, char** argv) {
    std::vector<std::string_view> args(argv + 1, argv + argc);

    bool cli_mode = false;
    std::string_view file_path;

    for (const auto& arg : args) {
        if (arg == "--cli" || arg == "-c") {
            cli_mode = true;
        } else if (!arg.starts_with("-")) {
            file_path = arg;
        }
    }

    if (!file_path.empty()) {
        auto file_target = core::file_target::create(file_path);
        if (file_target) {
            app::active_target = std::make_unique<core::file_target>(std::move(*file_target));
        } else {
            std::println(stderr, "Failed to open or parse file '{}'", file_path);
            return 1;
        }
    } else {
        app::active_target = std::make_unique<core::process>();
    }

    if (cli_mode) {
        cli::repl command_line_interface;
        command_line_interface.run();
    } else {
        auto app_instance = app::application::create(1600, 900, "ravel");
        if (!app_instance) {
            std::println(stderr, "Failed to create GUI application window");
            return -1;
        }
        app_instance->run();
    }

    return 0;
}
