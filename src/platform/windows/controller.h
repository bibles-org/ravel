#pragma once

#include <platform/process_controller.h>

using HANDLE = void*;

namespace platform {
    class windows_controller final : public process_controller {
    public:
        windows_controller();
        ~windows_controller() override;

        windows_controller(const windows_controller&) = delete;
        windows_controller& operator=(const windows_controller&) = delete;
        windows_controller(windows_controller&&) = delete;
        windows_controller& operator=(windows_controller&&) = delete;

        [[nodiscard]] std::expected<std::vector<core::process_info>, core::error_code> enumerate_processes() override;

        [[nodiscard]] std::expected<void, core::error_code> attach(std::uint32_t pid) override;

        void detach(std::uint32_t pid) override;

        [[nodiscard]] std::expected<std::vector<core::memory_region>, core::error_code>
        get_memory_regions(std::uint32_t pid) override;

        [[nodiscard]] std::expected<void, core::error_code>
        read_memory(std::uint32_t pid, std::uintptr_t address, std::span<std::byte> buffer) override;

        [[nodiscard]] std::expected<void, core::error_code>
        write_memory(std::uint32_t pid, std::uintptr_t address, std::span<const std::byte> buffer) override;

    private:
        HANDLE m_process_handle = nullptr;
    };
} // namespace platform
