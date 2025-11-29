#include "commands.h"
#include "dispatcher.h"

#include <app/ctx.h>
#include <core/process.h>
#include <print>
#include <charconv>
#include <ranges>

import zydis;

namespace cli {
    namespace {
        template <typename T>
        std::optional<T> parse_number(std::string_view s) {
            T value;
            bool is_hex = s.starts_with("0x") || s.starts_with("0X");
            if (is_hex) {
                s.remove_prefix(2);
            }
            auto result = std::from_chars(s.data(), s.data() + s.size(), value, is_hex ? 16 : 10);
            if (result.ec == std::errc()) {
                return value;
            }
            return std::nullopt;
        }

        command_status handle_help(const std::vector<std::string_view>& /*args*/, const dispatcher& d) {
            std::println("Available commands:");
            for (const auto& [name, cmd] : d.get_commands()) {
                std::println("  {:<15} {}", name, cmd.usage_text);
                std::println("    {}", cmd.help_text);
            }
            return command_status::ok;
        }

        command_status handle_quit(const std::vector<std::string_view>& /*args*/) {
            return command_status::exit_requested;
        }

        command_status handle_ps(const std::vector<std::string_view>& /*args*/) {
            auto result = app::proc->enumerate_processes();
            if (!result) {
                std::println(stderr, "Error: Failed to enumerate processes (code={}).", static_cast<int>(result.error()));
                return command_status::ok;
            }
            const auto& processes = result.value();
            std::println("{:<10} {:<30} {}", "PID", "Name", "Executable Path");
            std::println("{}", std::string(80, '-'));
            for (const auto& process : processes) {
                std::println("{:<10} {:<30} {}", process.pid, process.name, process.executable_path.string());
            }
            return command_status::ok;
        }

        command_status handle_attach(const std::vector<std::string_view>& args) {
            if (args.empty()) {
                std::println(stderr, "Usage: attach <pid>");
                return command_status::ok;
            }
            auto pid_opt = parse_number<std::uint32_t>(args[0]);
            if (!pid_opt) {
                std::println(stderr, "Error: Invalid PID '{}'.", args[0]);
                return command_status::ok;
            }
            auto result = app::proc->attach_to(*pid_opt);
            if (result) {
                std::println("Successfully attached to process {}.", *pid_opt);
            } else {
                std::println(stderr, "Error: Failed to attach to process {} (code={}).", *pid_opt, static_cast<int>(result.error()));
            }
            return command_status::ok;
        }

        command_status handle_detach(const std::vector<std::string_view>& /*args*/) {
            if (!app::proc->is_attached()) {
                std::println("Not attached to any process.");
                return command_status::ok;
            }
            std::println("Detaching from process {}.", app::proc->get_attached_pid());
            app::proc->detach();
            return command_status::ok;
        }

        command_status handle_regions(const std::vector<std::string_view>& /*args*/) {
            if (!app::proc->is_attached()) {
                std::println(stderr, "Error: Not attached. Use 'attach <pid>' first.");
                return command_status::ok;
            }
            auto result = app::proc->get_memory_regions();
            if (!result) {
                std::println(stderr, "Error: Failed to get memory regions (code={}).", static_cast<int>(result.error()));
                return command_status::ok;
            }
            std::println("{:<18} {:<18} {:<10} {:<10} {}", "Base Address", "End Address", "Size", "Perms", "Name");
            std::println("{}", std::string(100, '-'));
            for (const auto& region : result.value()) {
                std::println(
                    "0x{:016X} 0x{:016X} {:<10} {:<10} {}", region.base_address,
                    region.base_address + region.size, region.size, region.permission, region.name
                );
            }
            return command_status::ok;
        }

        command_status handle_read(const std::vector<std::string_view>& args) {
            if (args.size() < 1) {
                std::println(stderr, "Usage: read <address> [byte_count=256]");
                return command_status::ok;
            }
            if (!app::proc->is_attached()) {
                std::println(stderr, "Error: Not attached. Use 'attach <pid>' first.");
                return command_status::ok;
            }

            auto addr_opt = parse_number<std::uintptr_t>(args[0]);
            if (!addr_opt) {
                std::println(stderr, "Error: Invalid address '{}'.", args[0]);
                return command_status::ok;
            }

            std::size_t count = 256;
            if (args.size() > 1) {
                if (auto count_opt = parse_number<std::size_t>(args[1])) {
                    count = *count_opt;
                } else {
                    std::println(stderr, "Error: Invalid byte count '{}'.", args[1]);
                    return command_status::ok;
                }
            }

            std::vector<std::byte> buffer(count);
            auto result = app::proc->read_memory(*addr_opt, buffer);
            if (!result) {
                std::println(stderr, "Error: Failed to read memory at 0x{:X} (code={}).", *addr_opt, static_cast<int>(result.error()));
                return command_status::ok;
            }

            for (std::size_t i = 0; i < buffer.size(); i += 16) {
                std::print("0x{:016X} | ", *addr_opt + i);
                for (std::size_t j = 0; j < 16; ++j) {
                    if (i + j < buffer.size()) {
                        std::print("{:02X} ", std::to_integer<int>(buffer[i + j]));
                    } else {
                        std::print("   ");
                    }
                }
                std::print("| ");
                for (std::size_t j = 0; j < 16; ++j) {
                    if (i + j < buffer.size()) {
                        char c = static_cast<char>(buffer[i + j]);
                        std::print("{}", (std::isprint(c) ? c : '.'));
                    }
                }
                std::println("");
            }
            return command_status::ok;
        }

        command_status handle_disasm(const std::vector<std::string_view>& args) {
            if (args.size() < 1) {
                std::println(stderr, "Usage: disasm <address> [instruction_count=20]");
                return command_status::ok;
            }
            if (!app::proc->is_attached()) {
                std::println(stderr, "Error: Not attached. Use 'attach <pid>' first.");
                return command_status::ok;
            }

            auto addr_opt = parse_number<std::uintptr_t>(args[0]);
            if (!addr_opt) {
                std::println(stderr, "Error: Invalid address '{}'.", args[0]);
                return command_status::ok;
            }

            int instruction_count = 20;
            if (args.size() > 1) {
                if (auto count_opt = parse_number<int>(args[1])) {
                    instruction_count = *count_opt;
                } else {
                    std::println(stderr, "Error: Invalid instruction count '{}'.", args[1]);
                    return command_status::ok;
                }
            }

            std::vector<std::byte> buffer(static_cast<std::size_t>(instruction_count * 15));
            auto read_result = app::proc->read_memory(*addr_opt, buffer);
             if (!read_result) {
                std::println(stderr, "Error: Failed to read memory at 0x{:X} (code={}).", *addr_opt, static_cast<int>(read_result.error()));
                return command_status::ok;
            }

            zydis::init(MACHINE_MODE_LONG_64, STACK_WIDTH_64, FORMATTER_STYLE_INTEL);
            std::size_t offset = 0;
            for(int i = 0; i < instruction_count && offset < buffer.size(); ++i) {
                const auto* data = reinterpret_cast<const std::uint8_t*>(buffer.data() + offset);
                auto result = zydis::disassemble_format(data);
                if (!result) {
                    std::println("0x{:016X}: db {:02X}", *addr_opt + offset, std::to_integer<int>(buffer[offset]));
                    offset++;
                    continue;
                }
                const auto& [decoded, text] = *result;
                std::println("0x{:016X}: {}", *addr_opt + offset, text);
                offset += decoded.decoded.length;
            }
            return command_status::ok;
        }


    } // namespace

    void register_all_commands(dispatcher& d) {
        d.register_command("help", {
            .handler = [&](const auto& args) { return handle_help(args, d); },
            .help_text = "Displays this help message.",
            .usage_text = "help"
        });
        d.register_command("quit", {
            .handler = handle_quit,
            .help_text = "Exits the application.",
            .usage_text = "quit"
        });
        d.register_command("exit", {
            .handler = handle_quit,
            .help_text = "Exits the application.",
            .usage_text = "exit"
        });
        d.register_command("ps", {
            .handler = handle_ps,
            .help_text = "Lists all running processes on the system.",
            .usage_text = "ps"
        });
        d.register_command("attach", {
            .handler = handle_attach,
            .help_text = "Attaches to a process by its PID.",
            .usage_text = "attach <pid>"
        });
        d.register_command("detach", {
            .handler = handle_detach,
            .help_text = "Detaches from the currently attached process.",
            .usage_text = "detach"
        });
        d.register_command("regions", {
            .handler = handle_regions,
            .help_text = "Lists all memory regions of the attached process.",
            .usage_text = "regions"
        });
        d.register_command("read", {
            .handler = handle_read,
            .help_text = "Reads memory and displays it as a hexdump.",
            .usage_text = "read <address> [byte_count]"
        });
        d.register_command("disasm", {
            .handler = handle_disasm,
            .help_text = "Disassembles code at a given address.",
            .usage_text = "disasm <address> [instruction_count]"
        });
    }

} // namespace cli
