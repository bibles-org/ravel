#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <core/target.h>
#include <util/expected.h>

namespace core {
    class binary_parser {
    public:
        virtual ~binary_parser() = default;

        [[nodiscard]] virtual std::vector<memory_region> get_sections() const = 0;
        [[nodiscard]] virtual std::optional<std::uintptr_t> get_entry_point() const = 0;
        [[nodiscard]] virtual std::optional<std::uintptr_t> virtual_to_file_offset(std::uintptr_t virt_addr) const = 0;
        [[nodiscard]] virtual const std::filesystem::path& get_path() const = 0;
        [[nodiscard]] virtual std::string get_arch_name() const = 0;
        [[nodiscard]] virtual std::string get_type_name() const = 0;
    };
} // namespace core
