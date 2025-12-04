#include <core/file_target.h>

#include <core/parsers/elf_parser.h>
#include <core/parsers/pe_parser.h>

#include <cstring>
#include <fstream>
#include <iterator>

namespace core {
    namespace {
        std::expected<std::vector<std::byte>, error_code> read_file_bytes(const std::filesystem::path& path) {
            std::ifstream file(path, std::ios::binary | std::ios::ate);
            if (!file) {
                return std::unexpected(error_code::read_failed);
            }
            std::streamsize size = file.tellg();
            file.seekg(0, std::ios::beg);
            std::vector<std::byte> buffer(static_cast<std::size_t>(size));
            if (file.read(reinterpret_cast<char*>(buffer.data()), size)) {
                return buffer;
            }
            return std::unexpected(error_code::read_failed);
        }
    } // namespace

    std::expected<file_target, error_code> file_target::create(const std::filesystem::path& path) {
        auto file_data_res = read_file_bytes(path);
        if (!file_data_res) {
            return std::unexpected(file_data_res.error());
        }
        std::vector<std::byte> file_data = std::move(*file_data_res);
        std::span<const std::byte> data_span = file_data;

        // try pe parser
        if (auto pe_res = pe_parser::create(path, data_span)) {
            return file_target(std::move(file_data), std::make_unique<pe_parser>(std::move(*pe_res)));
        }

        // try elf parser
        if (auto elf_res = elf_parser::create(path, data_span)) {
            return file_target(std::move(file_data), std::make_unique<elf_parser>(std::move(*elf_res)));
        }

        return std::unexpected(error_code::read_failed); // unsupported format
    }

    file_target::file_target(std::vector<std::byte> data, std::unique_ptr<binary_parser> parser) :
        m_file_data(std::move(data)), m_parser(std::move(parser)) {
    }

    std::expected<void, error_code> file_target::read_memory(std::uintptr_t address, std::span<std::byte> buffer) {
        auto file_offset_opt = m_parser->virtual_to_file_offset(address);
        if (!file_offset_opt) {
            return std::unexpected(error_code::invalid_address);
        }
        std::size_t file_offset = *file_offset_opt;

        if (file_offset >= m_file_data.size()) {
            return std::unexpected(error_code::invalid_address);
        }

        std::size_t bytes_to_read = std::min(buffer.size(), m_file_data.size() - file_offset);

        std::memcpy(buffer.data(), m_file_data.data() + file_offset, bytes_to_read);

        if (bytes_to_read < buffer.size()) {
            return std::unexpected(error_code::partial_read);
        }
        return {};
    }

    std::expected<std::vector<memory_region>, error_code> file_target::get_memory_regions() {
        return m_parser->get_sections();
    }

    bool file_target::is_live() const {
        return false;
    }

    std::string file_target::get_name() const {
        return m_parser->get_path().filename().string();
    }

    std::optional<std::uintptr_t> file_target::get_entry_point() const {
        return m_parser->get_entry_point();
    }
} // namespace core
