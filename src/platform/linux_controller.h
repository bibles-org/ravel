#pragma once

#include <platform/process_controller.h>

namespace platform {
    class linux_controller final : public process_controller {
    public:
        ~linux_controller() override;

        [[nodiscard]] std::expected<std::vector<core::process_info>, core::error_code> enumerate_processes() override;

        [[nodiscard]] std::expected<void, core::error_code> attach(std::uint32_t pid) override;

        void detach(std::uint32_t pid) override;

        [[nodiscard]] std::expected<std::vector<core::memory_region>, core::error_code>
        get_memory_regions(std::uint32_t pid) override;

        [[nodiscard]] std::expected<void, core::error_code>
        read_memory(std::uint32_t pid, std::uintptr_t address, std::span<std::byte> buffer) override;

    private:
        // /proc/[pid]/mem
        int m_mem_fd = -1;
    };
} // namespace platform
