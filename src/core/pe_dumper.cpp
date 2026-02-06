#include "pe_dumper.h"

#include <core/headers/pe.h>
#include <print>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <ranges>

namespace core {
    namespace {
        constexpr std::uint16_t image_dos_signature = 0x5a4d;
        constexpr std::uint32_t image_nt_signature = 0x00004550;

        std::expected<std::uintptr_t, error_code> find_module_base(target* t) {
            auto regions_exp = t->get_memory_regions();
            if (!regions_exp) {
                std::println(stderr, "failed to get memory regions");
                return std::unexpected(regions_exp.error());
            }

            std::println("scanning {} memory regions for pe header", regions_exp.value().size());

            std::vector<std::pair<std::uintptr_t, std::size_t>> candidates;
            std::size_t checked_count = 0;

            for (const auto& region : regions_exp.value()) {
                if (region.permission.find('r') == std::string::npos) {
                    continue;
                }

                ++checked_count;

                std::array<std::byte, 512> buffer{};
                auto read_exp = t->read_memory(region.base_address, buffer);
                if (!read_exp) {
                    continue;
                }

                if (buffer[0] != std::byte{'M'} || buffer[1] != std::byte{'Z'}) {
                    continue;
                }

                auto dos_offset_ptr = reinterpret_cast<const std::int32_t*>(buffer.data() + 0x3c);
                std::int32_t e_lfanew = *dos_offset_ptr;

                if (e_lfanew < 0x40 || e_lfanew > 0x800 || static_cast<std::size_t>(e_lfanew) + 4 > buffer.size()) {
                    continue;
                }

                auto pe_sig_ptr = reinterpret_cast<const std::uint32_t*>(buffer.data() + e_lfanew);
                std::uint32_t pe_signature = *pe_sig_ptr;

                if (pe_signature == image_nt_signature) {
                    std::println(
                            "found valid pe at 0x{:016X} (size=0x{:X}, e_lfanew=0x{:X}, name='{}')",
                            region.base_address, region.size, e_lfanew, region.name
                    );
                    candidates.emplace_back(region.base_address, region.size);
                }
            }

            std::println("checked {} readable regions, found {} pe candidates", checked_count, candidates.size());

            if (candidates.empty()) {
                return std::unexpected(error_code::not_found);
            }

            auto largest = std::ranges::max_element(candidates, {}, [](const auto& p) {
                return p.second;
            });
            std::println("selected largest pe at 0x{:016X} (size=0x{:X})", largest->first, largest->second);
            return largest->first;
        }
        template <typename T>
        std::expected<T, error_code> read_struct(target* t, std::uintptr_t address) {
            T data{};
            std::span<std::byte> buffer{reinterpret_cast<std::byte*>(&data), sizeof(T)};
            auto read_exp = t->read_memory(address, buffer);
            if (!read_exp) {
                return std::unexpected(read_exp.error());
            }
            return data;
        }
    } // namespace

    std::expected<void, error_code> dump_pe_image(target* t, const std::filesystem::path& output_path) {
        auto base_exp = find_module_base(t);
        if (!base_exp) {
            std::println(stderr, "failed to find module base (code={})", static_cast<int>(base_exp.error()));
            return std::unexpected(base_exp.error());
        }
        const std::uintptr_t base_address = base_exp.value();
        std::println("found module base at 0x{:016X}", base_address);

        auto dos_header_exp = read_struct<pe::image_dos_header>(t, base_address);
        if (!dos_header_exp) {
            std::println(stderr, "failed to read dos header");
            return std::unexpected(dos_header_exp.error());
        }
        const auto& dos_header = dos_header_exp.value();

        if (dos_header.e_magic != image_dos_signature) {
            std::println(stderr, "invalid dos signature: 0x{:04X}", dos_header.e_magic);
            return std::unexpected(error_code::invalid_format);
        }

        const std::uintptr_t nt_headers_address = base_address + static_cast<std::uintptr_t>(dos_header.e_lfanew);
        auto nt_headers_exp = read_struct<pe::image_nt_headers64>(t, nt_headers_address);
        if (!nt_headers_exp) {
            std::println(stderr, "failed to read nt headers");
            return std::unexpected(nt_headers_exp.error());
        }
        const auto& nt_headers = nt_headers_exp.value();

        if (nt_headers.signature != image_nt_signature) {
            std::println(stderr, "invalid pe signature: 0x{:08X}", nt_headers.signature);
            return std::unexpected(error_code::invalid_format);
        }

        const std::uint32_t size_of_image = nt_headers.optional_header.size_of_image;
        std::println("size of image: {} bytes", size_of_image);

        std::vector<std::byte> image_buffer(size_of_image);

        constexpr std::size_t chunk_size = 0x1000;
        for (std::size_t offset = 0; offset < size_of_image; offset += chunk_size) {
            const std::size_t current_chunk = std::min(chunk_size, size_of_image - offset);
            std::span<std::byte> chunk_buffer{image_buffer.data() + offset, current_chunk};
            auto read_exp = t->read_memory(base_address + offset, chunk_buffer);
            if (!read_exp) {
                std::memset(chunk_buffer.data(), 0, chunk_buffer.size());
            }
        }

        const std::uint16_t number_of_sections = nt_headers.file_header.number_of_sections;
        std::println("patching {} section headers", number_of_sections);

        const std::uintptr_t first_section_offset =
                static_cast<std::uintptr_t>(dos_header.e_lfanew) + sizeof(std::uint32_t) +
                sizeof(pe::image_file_header) +
                static_cast<std::uintptr_t>(nt_headers.file_header.size_of_optional_header);

        for (std::uint16_t i = 0; i < number_of_sections; ++i) {
            const std::uintptr_t section_header_offset = first_section_offset + i * sizeof(pe::image_section_header);

            if (section_header_offset + sizeof(pe::image_section_header) > size_of_image) {
                std::println(stderr, "section header {} is outside image bounds", i);
                continue;
            }

            auto* section_header =
                    reinterpret_cast<pe::image_section_header*>(image_buffer.data() + section_header_offset);

            std::println(
                    "  section {}: virtualaddress=0x{:08X} virtualsize=0x{:08X}, setting "
                    "pointertorawdata=virtualaddress, "
                    "sizeofrawdata=virtualsize",
                    i, section_header->virtual_address, section_header->misc.virtual_size
            );

            section_header->pointer_to_raw_data = section_header->virtual_address;
            section_header->size_of_raw_data = section_header->misc.virtual_size;
        }

        std::ofstream out_file{output_path, std::ios::binary};
        if (!out_file) {
            std::println(stderr, "failed to create output file '{}'", output_path.string());
            return std::unexpected(error_code::file_write_error);
        }

        out_file.write(
                reinterpret_cast<const char*>(image_buffer.data()), static_cast<std::streamsize>(image_buffer.size())
        );
        if (!out_file) {
            std::println(stderr, "failed to write to output file");
            return std::unexpected(error_code::file_write_error);
        }

        std::println("successfully dumped pe image to '{}'", output_path.string());
        return {};
    }
} // namespace core
