#include <core/parsers/elf_parser.h>

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
            if (flags & elf::pf_r)
                s[0] = 'r';
            if (flags & elf::pf_w)
                s[1] = 'w';
            if (flags & elf::pf_x)
                s[2] = 'x';
            return s;
        }
    } // namespace

    std::expected<elf_parser, error_code>
    elf_parser::create(std::filesystem::path path, std::span<const std::byte> data) {
        if (data.size() < sizeof(elf::elf64_ehdr)) {
            return std::unexpected(error_code::read_failed);
        }
        auto header = read_from_span<elf::elf64_ehdr>(data, 0);

        if (header.e_ident[0] != 0x7f || header.e_ident[1] != 'E' || header.e_ident[2] != 'L' ||
            header.e_ident[3] != 'F') {
            return std::unexpected(error_code::read_failed); // not an elf file
        }

        std::vector<elf::elf64_phdr> segments;
        if (header.e_phnum > 0) {
            segments.reserve(header.e_phnum);
            for (std::uint16_t i = 0; i < header.e_phnum; ++i) {
                std::size_t offset = header.e_phoff + (i * header.e_phentsize);
                segments.push_back(read_from_span<elf::elf64_phdr>(data, offset));
            }
        }

        return elf_parser(std::move(path), header, std::move(segments));
    }

    elf_parser::elf_parser(
            std::filesystem::path path, const elf::elf64_ehdr& header, std::vector<elf::elf64_phdr> segments
    ) : m_path(std::move(path)), m_header(header), m_segments(std::move(segments)) {
    }

    std::vector<memory_region> elf_parser::get_sections() const {
        std::vector<memory_region> regions;
        for (const auto& segment : m_segments) {
            if (segment.p_type == elf::pt_load && segment.p_memsz > 0) {
                regions.emplace_back(
                        segment.p_vaddr, segment.p_memsz, perms_to_string(segment.p_flags),
                        "segment_" + std::to_string(regions.size())
                );
            }
        }
        return regions;
    }

    std::optional<std::uintptr_t> elf_parser::get_entry_point() const {
        return m_header.e_entry;
    }

    std::optional<std::uintptr_t> elf_parser::virtual_to_file_offset(std::uintptr_t virt_addr) const {
        for (const auto& segment : m_segments) {
            if (segment.p_type == elf::pt_load) {
                if (virt_addr >= segment.p_vaddr && virt_addr < (segment.p_vaddr + segment.p_memsz)) {
                    std::uintptr_t offset_in_segment = virt_addr - segment.p_vaddr;
                    if (offset_in_segment < segment.p_filesz) {
                        return segment.p_offset + offset_in_segment;
                    }
                }
            }
        }
        return std::nullopt;
    }

    std::string elf_parser::get_arch_name() const {
        switch (m_header.e_machine) {
            case 0x3E:
                return "x86-64";
            case 0xB7:
                return "AArch64";
            default:
                return "Unknown";
        }
    }
    std::string elf_parser::get_type_name() const {
        return "ELF";
    }

} // namespace core
