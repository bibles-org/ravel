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
        std::array<std::byte, 4> data;
        bool valid;

        memory_entry() : offset(0), addr(0), data{}, valid(false) {
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
        std::size_t entries_per_page;
        std::size_t current_page;

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

        std::expected<std::array<std::byte, 4>, bool> read_memory(std::uintptr_t addr);
        std::string format_hex(std::span<const std::byte, 4> data);
        std::string format_ascii(std::span<const std::byte, 4> data);

        std::expected<std::uintptr_t, bool> parse_address_input(std::string_view input);
        std::expected<std::uintptr_t, bool> dereference_pointer(std::uintptr_t ptr_addr);

        template <typename T>
        T bytes_to(std::span<const std::byte, 4> data)
            requires(sizeof(T) == 4 && std::is_trivially_copyable_v<T> && std::is_standard_layout_v<T>);

        bool is_valid_addr(std::uintptr_t addr);
    };
} // namespace ui
