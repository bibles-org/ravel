#pragma once

#include <core/process.h>
#include <cstdint>
#include <expected>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <ui/view.h>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

namespace ui {
    enum class memory_type {
        int8,
        uint8,
        int16,
        uint16,
        int32,
        uint32,
        int64,
        uint64,
        float32,
        float64,
        pointer,
        text,
        bytes
    };

    struct memory_class {
        std::string name;
        std::uintptr_t addr;
        std::size_t size;
        bool expanded;

        memory_class(std::string_view n, std::uintptr_t a = 0, std::size_t sz = 256) :
            name(n), addr(a), size(sz), expanded(true) {
        }
    };

    struct memory_entry {
        std::size_t offset;
        std::uintptr_t addr;
        std::size_t data_offset;
        std::size_t data_size;
        std::optional<std::string> dereferenced_string;
        memory_type type;
        std::size_t type_size;
        bool valid;

        memory_entry() :
            offset(0), addr(0), data_offset(0), data_size(0), type(memory_type::int32), type_size(4),
            valid(false) {
        }
    };

    class memory_view final : public view {
    public:
        memory_view();
        void render() override;

    private:
        std::vector<std::unique_ptr<memory_class>> classes;
        std::optional<std::size_t> selected_idx;
        std::vector<memory_entry> entries;
        std::vector<std::byte> memory_buffer;
        bool m_buffer_read_success;

        char new_class_name[256];
        char addr_input[32];
        bool show_add_popup;
        bool auto_refresh;
        float refresh_rate;

        void render_sidebar();
        void render_memory_view();
        void render_class_list();
        void render_add_popup();
        void render_memory_table();

        void add_class(std::string_view name);
        void remove_class(std::size_t index);
        void refresh_data();

        std::expected<std::vector<std::byte>, bool> read_memory(std::uintptr_t addr, std::size_t size);
        std::string format_hex(std::span<const std::byte> data);
        std::string format_ascii(std::span<const std::byte> data);
        std::string format_typed_value(const memory_entry& entry);

        std::expected<std::uintptr_t, bool> parse_address_input(std::string_view input);
        std::expected<std::uintptr_t, bool> dereference_pointer(std::uintptr_t ptr_addr);

        std::optional<std::string> dereference_as_string(std::span<const std::byte> data);
        std::optional<std::string> read_string(std::uintptr_t addr, std::size_t max_len = 64);

        template <typename T>
        T bytes_to(std::span<const std::byte> data)
            requires(std::is_trivially_copyable_v<T> && std::is_standard_layout_v<T>);

        bool is_valid_addr(std::uintptr_t addr);
        std::size_t get_type_size(memory_type type);
        const char* get_type_name(memory_type type);
        void change_entry_type(std::size_t entry_idx, memory_type new_type);
        void rebuild_entries_from_index(std::size_t start_idx);

        std::span<const std::byte> get_entry_data_span(const memory_entry& entry) const; // Added
    };
} // namespace ui
