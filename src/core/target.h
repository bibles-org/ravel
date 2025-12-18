#pragma once

#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <util/expected.h>

namespace core {
    struct memory_region {
        std::uintptr_t base_address;
        std::size_t size;
        std::string permission;
        std::string name;

        auto operator<=>(const memory_region&) const = default;
    };

    class target {
    public:
        virtual ~target() = default;

        [[nodiscard]] virtual std::expected<void, error_code>
        read_memory(std::uintptr_t address, std::span<std::byte> buffer) = 0;
        [[nodiscard]] virtual std::expected<void, error_code>
        write_memory(std::uintptr_t address, std::span<const std::byte> buffer) = 0;
        [[nodiscard]] virtual std::expected<std::vector<memory_region>, error_code> get_memory_regions() = 0;
        [[nodiscard]] virtual bool is_live() const = 0;
        [[nodiscard]] virtual std::string get_name() const = 0;
        [[nodiscard]] virtual std::optional<std::uintptr_t> get_entry_point() const = 0;
    };
} // namespace core
