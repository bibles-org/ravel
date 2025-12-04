#pragma once

#include <core/parsers/binary_parser.h>
#include <core/parsers/pe.h>

namespace core {
    class pe_parser final : public binary_parser {
    public:
        static std::expected<pe_parser, error_code> create(std::filesystem::path path, std::span<const std::byte> data);

        [[nodiscard]] std::vector<memory_region> get_sections() const override;
        [[nodiscard]] std::optional<std::uintptr_t> get_entry_point() const override;
        [[nodiscard]] std::optional<std::uintptr_t> virtual_to_file_offset(std::uintptr_t virt_addr) const override;
        [[nodiscard]] const std::filesystem::path& get_path() const override {
            return m_path;
        }
        [[nodiscard]] std::string get_arch_name() const override;
        [[nodiscard]] std::string get_type_name() const override;

    private:
        pe_parser(
                std::filesystem::path path, const pe::image_nt_headers64& nt_header,
                std::vector<pe::image_section_header> sections
        );

        std::filesystem::path m_path;
        pe::image_nt_headers64 m_nt_header;
        std::vector<pe::image_section_header> m_sections;
    };
} // namespace core
