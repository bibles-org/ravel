#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <vector>

#include <core/parsers/binary_parser.h>
#include <core/target.h>
#include <util/expected.h>

namespace core {
    class file_target final : public target {
    public:
        static std::expected<file_target, error_code> create(const std::filesystem::path& path);

        file_target(file_target&&) = default;
        file_target& operator=(file_target&&) = default;

        [[nodiscard]] std::expected<void, error_code>
        read_memory(std::uintptr_t address, std::span<std::byte> buffer) override;
        [[nodiscard]] std::expected<void, error_code>
        write_memory(std::uintptr_t address, std::span<const std::byte> buffer) override;
        [[nodiscard]] std::expected<std::vector<memory_region>, error_code> get_memory_regions() override;
        [[nodiscard]] bool is_live() const override;
        [[nodiscard]] std::string get_name() const override;
        [[nodiscard]] std::optional<std::uintptr_t> get_entry_point() const override;

        [[nodiscard]] const binary_parser* get_parser() const {
            return m_parser.get();
        }

    private:
        file_target(std::vector<std::byte> data, std::unique_ptr<binary_parser> parser);

        std::vector<std::byte> m_file_data;
        std::unique_ptr<binary_parser> m_parser;
    };
} // namespace core
