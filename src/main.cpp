#include <app/application.h>
#include <core/process.h>
#include <print>

int main() {
    auto app_instance = app::application::create(1600, 900, "ravel");
    if (!app_instance) {
        return -1;
    }

    core::process proc;
    auto result = proc.enumerate_processes();

    if (!result.has_value()) {
        std::println("failed, error code {}", static_cast<int>(result.error()));
        return -1;
    }

    const auto& processes = result.value();
    std::println("found {} processes:", processes.size());
    std::println("");

    std::println("{:<10} {:<25} {}", "PID", "Name", "Executable Path");
    std::println("{}", std::string(80, '-'));

    for (const auto& process : processes) {
        std::println("{:<10} {:<25} {}", process.pid, process.name, process.executable_path.string());
    }

    app_instance->run();

    return 0;
}
