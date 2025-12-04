#pragma once

#include <core/parsers/binary_parser.h>
#include <core/parsers/elf.h>

namespace core {
    class elf_parser final : public binary_parser {
    public:
        static std::expected<elf_parser, error_code>
        create(std::filesystem::path path, std::span<const std::byte> data);

        [[nodiscard]] std::vector<memory_region> get_sections() const override;
        [[nodiscard]] std::optional<std::uintptr_t> get_entry_point() const override;
        [[nodiscard]] std::optional<std::uintptr_t> virtual_to_file_offset(std::uintptr_t virt_addr) const override;
        [[nodiscard]] const std::filesystem::path& get_path() const override {
            return m_path;
        }
        [[nodiscard]] std::string get_arch_name() const override;
        [[nodiscard]] std::string get_type_name() const override;

    private:
        elf_parser(std::filesystem::path path, const elf::elf64_ehdr& header, std::vector<elf::elf64_phdr> segments);

        std::filesystem::path m_path;
        elf::elf64_ehdr m_header;
        std::vector<elf::elf64_phdr> m_segments;
    };
} // namespace core
