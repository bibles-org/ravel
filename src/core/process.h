#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include <core/target.h>
#include <platform/process_controller.h>
#include <util/expected.h>

namespace core {
    struct process_info {
        std::uint32_t pid;
        std::string name;
        std::filesystem::path executable_path;

        auto operator<=>(const process_info&) const = default;
    };

    class process final : public target {
    public:
        process();

        [[nodiscard]] std::expected<void, error_code>
        read_memory(std::uintptr_t address, std::span<std::byte> buffer) override;
        [[nodiscard]] std::expected<std::vector<memory_region>, error_code> get_memory_regions() override;
        [[nodiscard]] bool is_live() const override;
        [[nodiscard]] std::string get_name() const override;
        [[nodiscard]] std::optional<std::uintptr_t> get_entry_point() const override;

        [[nodiscard]] std::expected<std::vector<process_info>, error_code> enumerate_processes();
        [[nodiscard]] std::expected<void, error_code> attach_to(std::uint32_t pid);
        void detach();
        [[nodiscard]] bool is_attached() const;
        [[nodiscard]] std::uint32_t get_attached_pid() const;

    private:
        std::unique_ptr<platform::process_controller> m_controller;
        std::uint32_t m_attached_pid = 0;
        std::string m_attached_process_name;
    };
} // namespace core
