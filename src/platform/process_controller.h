#pragma once

#include <cstdint>
#include <expected>
#include <span>
#include <vector>

#include <util/expected.h>

namespace core {
    struct process_info;
    struct memory_region;
} // namespace core

namespace platform {
    class process_controller {
    public:
        virtual ~process_controller() = default;

        [[nodiscard]] virtual std::expected<std::vector<core::process_info>, core::error_code>
        enumerate_processes() = 0;

        [[nodiscard]] virtual std::expected<void, core::error_code> attach(std::uint32_t pid) = 0;

        virtual void detach(std::uint32_t pid) = 0;

        [[nodiscard]] virtual std::expected<std::vector<core::memory_region>, core::error_code>
        get_memory_regions(std::uint32_t pid) = 0;

        [[nodiscard]] virtual std::expected<void, core::error_code>
        read_memory(std::uint32_t pid, std::uintptr_t address, std::span<std::byte> buffer) = 0;

        [[nodiscard]] virtual std::expected<void, core::error_code>
        write_memory(std::uint32_t pid, std::uintptr_t address, std::span<const std::byte> buffer) = 0;
    };
} // namespace platform
