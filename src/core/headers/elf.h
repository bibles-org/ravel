#pragma once

#include <cstdint>

namespace core::elf {
    using elf64_addr = std::uint64_t;
    using elf64_off = std::uint64_t;
    using elf64_half = std::uint16_t;
    using elf64_word = std::uint32_t;
    using elf64_sword = std::int32_t;
    using elf64_xword = std::uint64_t;
    using elf64_sxword = std::int64_t;

    constexpr int ei_nident = 16;

    struct elf64_ehdr {
        unsigned char e_ident[ei_nident];
        elf64_half e_type;
        elf64_half e_machine;
        elf64_word e_version;
        elf64_addr e_entry;
        elf64_off e_phoff;
        elf64_off e_shoff;
        elf64_word e_flags;
        elf64_half e_ehsize;
        elf64_half e_phentsize;
        elf64_half e_phnum;
        elf64_half e_shentsize;
        elf64_half e_shnum;
        elf64_half e_shstrndx;
    };

    struct elf64_phdr {
        elf64_word p_type;
        elf64_word p_flags;
        elf64_off p_offset;
        elf64_addr p_vaddr;
        elf64_addr p_paddr;
        elf64_xword p_filesz;
        elf64_xword p_memsz;
        elf64_xword p_align;
    };

    enum phdr_type : std::uint32_t {
        pt_load = 1
    };

    enum phdr_flags : std::uint32_t {
        pf_x = (1 << 0),
        pf_w = (1 << 1),
        pf_r = (1 << 2),
    };

} // namespace core::elf
