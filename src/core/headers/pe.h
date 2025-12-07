#pragma once

#include <cstdint>

namespace core::pe {
    struct image_dos_header {
        std::uint16_t e_magic;
        std::uint16_t e_cblp;
        std::uint16_t e_cp;
        std::uint16_t e_crlc;
        std::uint16_t e_cparhdr;
        std::uint16_t e_minalloc;
        std::uint16_t e_maxalloc;
        std::uint16_t e_ss;
        std::uint16_t e_sp;
        std::uint16_t e_csum;
        std::uint16_t e_ip;
        std::uint16_t e_cs;
        std::uint16_t e_lfarlc;
        std::uint16_t e_ovno;
        std::uint16_t e_res[4];
        std::uint16_t e_oemid;
        std::uint16_t e_oeminfo;
        std::uint16_t e_res2[10];
        std::int32_t e_lfanew;
    };

    struct image_file_header {
        std::uint16_t machine;
        std::uint16_t number_of_sections;
        std::uint32_t time_date_stamp;
        std::uint32_t pointer_to_symbol_table;
        std::uint32_t number_of_symbols;
        std::uint16_t size_of_optional_header;
        std::uint16_t characteristics;
    };

    struct image_data_directory {
        std::uint32_t virtual_address;
        std::uint32_t size;
    };

    constexpr int image_numberof_directory_entries = 16;

    struct image_optional_header64 {
        std::uint16_t magic;
        std::uint8_t major_linker_version;
        std::uint8_t minor_linker_version;
        std::uint32_t size_of_code;
        std::uint32_t size_of_initialized_data;
        std::uint32_t size_of_uninitialized_data;
        std::uint32_t address_of_entry_point;
        std::uint32_t base_of_code;
        std::uint64_t image_base;
        std::uint32_t section_alignment;
        std::uint32_t file_alignment;
        std::uint16_t major_operating_system_version;
        std::uint16_t minor_operating_system_version;
        std::uint16_t major_image_version;
        std::uint16_t minor_image_version;
        std::uint16_t major_subsystem_version;
        std::uint16_t minor_subsystem_version;
        std::uint32_t win32_version_value;
        std::uint32_t size_of_image;
        std::uint32_t size_of_headers;
        std::uint32_t check_sum;
        std::uint16_t subsystem;
        std::uint16_t dll_characteristics;
        std::uint64_t size_of_stack_reserve;
        std::uint64_t size_of_stack_commit;
        std::uint64_t size_of_heap_reserve;
        std::uint64_t size_of_heap_commit;
        std::uint32_t loader_flags;
        std::uint32_t number_of_rva_and_sizes;
        image_data_directory data_directory[image_numberof_directory_entries];
    };

    struct image_nt_headers64 {
        std::uint32_t signature;
        image_file_header file_header;
        image_optional_header64 optional_header;
    };

    constexpr int image_sizeof_short_name = 8;

    struct image_section_header {
        std::uint8_t name[image_sizeof_short_name];
        union {
            std::uint32_t physical_address;
            std::uint32_t virtual_size;
        } misc;
        std::uint32_t virtual_address;
        std::uint32_t size_of_raw_data;
        std::uint32_t pointer_to_raw_data;
        std::uint32_t pointer_to_relocations;
        std::uint32_t pointer_to_linenumbers;
        std::uint16_t number_of_relocations;
        std::uint16_t number_of_linenumbers;
        std::uint32_t characteristics;
    };

    enum section_flags : std::uint32_t {
        image_scn_mem_execute = 0x20000000,
        image_scn_mem_read = 0x40000000,
        image_scn_mem_write = 0x80000000,
    };

} // namespace core::pe
