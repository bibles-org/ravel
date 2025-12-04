#include <core/parsers/pe_parser.h>

#include <cstring>
#include <string>

namespace core {
    namespace {
        template <typename T>
        T read_from_span(std::span<const std::byte> data, std::size_t offset) {
            T result{};
            if (offset + sizeof(T) <= data.size()) {
                std::memcpy(&result, data.data() + offset, sizeof(T));
            }
            return result;
        }

        std::string perms_to_string(std::uint32_t flags) {
            std::string s = "---";
            if (flags & pe::image_scn_mem_read)
                s[0] = 'r';
            if (flags & pe::image_scn_mem_write)
                s[1] = 'w';
            if (flags & pe::image_scn_mem_execute)
                s[2] = 'x';
            return s;
        }

        std::string section_name_to_string(const std::uint8_t* name) {
            char buf[pe::image_sizeof_short_name + 1] = {0};
            std::memcpy(buf, name, pe::image_sizeof_short_name);
            return std::string(buf);
        }
    } // namespace

    std::expected<pe_parser, error_code>
    pe_parser::create(std::filesystem::path path, std::span<const std::byte> data) {
        if (data.size() < sizeof(pe::image_dos_header)) {
            return std::unexpected(error_code::read_failed);
        }
        auto dos_header = read_from_span<pe::image_dos_header>(data, 0);
        if (dos_header.e_magic != 0x5a4d) { // "MZ"
            return std::unexpected(error_code::read_failed);
        }

        auto nt_header = read_from_span<pe::image_nt_headers64>(data, dos_header.e_lfanew);
        if (nt_header.signature != 0x00004550) { // "PE\0\0"
            return std::unexpected(error_code::read_failed);
        }

        std::vector<pe::image_section_header> sections;
        std::size_t section_offset = dos_header.e_lfanew + sizeof(pe::image_nt_headers64) -
                                     sizeof(pe::image_optional_header64) +
                                     nt_header.file_header.size_of_optional_header;

        for (int i = 0; i < nt_header.file_header.number_of_sections; ++i) {
            sections.push_back(read_from_span<pe::image_section_header>(data, section_offset));
            section_offset += sizeof(pe::image_section_header);
        }

        return pe_parser(std::move(path), nt_header, std::move(sections));
    }

    pe_parser::pe_parser(
            std::filesystem::path path, const pe::image_nt_headers64& nt_header,
            std::vector<pe::image_section_header> sections
    ) : m_path(std::move(path)), m_nt_header(nt_header), m_sections(std::move(sections)) {
    }

    std::vector<memory_region> pe_parser::get_sections() const {
        std::vector<memory_region> regions;
        for (const auto& section : m_sections) {
            regions.emplace_back(
                    m_nt_header.optional_header.image_base + section.virtual_address, section.misc.virtual_size,
                    perms_to_string(section.characteristics), section_name_to_string(section.name)
            );
        }
        return regions;
    }

    std::optional<std::uintptr_t> pe_parser::get_entry_point() const {
        return m_nt_header.optional_header.image_base + m_nt_header.optional_header.address_of_entry_point;
    }

    std::optional<std::uintptr_t> pe_parser::virtual_to_file_offset(std::uintptr_t virt_addr) const {
        std::uintptr_t rva = virt_addr - m_nt_header.optional_header.image_base;
        for (const auto& section : m_sections) {
            if (rva >= section.virtual_address && rva < (section.virtual_address + section.misc.virtual_size)) {
                return (rva - section.virtual_address) + section.pointer_to_raw_data;
            }
        }
        return std::nullopt;
    }

    std::string pe_parser::get_arch_name() const {
        switch (m_nt_header.file_header.machine) {
            case 0x8664:
                return "x86-64";
            case 0xAA64:
                return "AArch64";
            default:
                return "Unknown";
        }
    }

    std::string pe_parser::get_type_name() const {
        return "PE";
    }

} // namespace core
