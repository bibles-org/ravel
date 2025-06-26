#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include <platform/process_controller.h>
#include <util/expected.h>

namespace core {
    struct process_info {
        std::uint32_t pid;
        std::string name;
        std::filesystem::path executable_path;

        auto operator<=>(const process_info&) const = default;
    };

    struct memory_region {
        std::uintptr_t base_address;
        std::size_t size;
        std::string permission;
        std::string name;

        auto operator<=>(const memory_region&) const = default;
    };

    class process {
    public:
        process();

        [[nodiscard]] std::expected<std::vector<process_info>, error_code> enumerate_processes();
        [[nodiscard]] std::expected<void, error_code> attach_to(std::uint32_t pid);
        [[nodiscard]] std::expected<std::vector<memory_region>, error_code> get_memory_regions();

        [[nodiscard]] std::expected<void, error_code> read_memory(std::uintptr_t address, std::span<std::byte> buffer);

        void detach();
        [[nodiscard]] bool is_attached() const;
        [[nodiscard]] std::uint32_t get_attached_pid() const;

    private:
        std::unique_ptr<platform::process_controller> m_controller;
        std::uint32_t m_attached_pid = 0;
    };
} // namespace core
