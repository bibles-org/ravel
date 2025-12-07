#pragma once

#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <core/target.h>
#include <ui/view.h>

namespace ui {
    enum class type_id : std::uint8_t {
        i8,
        u8,
        i16,
        u16,
        i32,
        u32,
        i64,
        u64,
        f32,
        f64,
        ptr,
        txt,
        hex
    };

    struct field {
        std::size_t offset = 0;
        type_id type = type_id::i32;
        std::size_t size = 4;
        bool valid = false;
        std::optional<std::string> meta;
    };

    struct block {
        std::string name;
        std::uintptr_t base = 0;
        std::size_t size = 256;
        std::vector<field> fields;

        explicit block(std::string_view n) : name(n) {
        }
    };

    class memory_view final : public view {
    public:
        memory_view();
        void render() override;

    private:
        void draw_nav();
        void draw_table();
        void draw_popups();

        void add_block(std::string_view name);
        void select_block(std::size_t idx);
        void refresh_cache();
        void update_layout(std::size_t start_idx);
        void set_field_type(std::size_t idx, type_id type);

        [[nodiscard]] std::span<const std::byte> read_field(const field& f) const;
        [[nodiscard]] std::string fmt_value(const field& f);
        [[nodiscard]] static std::string fmt_hex(std::span<const std::byte> data);
        [[nodiscard]] static std::string fmt_txt(std::span<const std::byte> data);

        [[nodiscard]] std::optional<std::string> resolve_ptr(std::span<const std::byte> data);
        [[nodiscard]] static std::size_t type_size(type_id type);
        [[nodiscard]] static const char* type_name(type_id type);

        std::vector<std::unique_ptr<block>> blocks;
        std::optional<std::size_t> active_idx;
        core::target* target = nullptr;

        std::vector<std::byte> cache;
        bool cache_valid = false;

        char addr_buf[64]{};
        char name_buf[64]{};
        bool show_create = false;
        bool auto_refresh = false;
        float refresh_rate = 1.0f;
    };
} // namespace ui
