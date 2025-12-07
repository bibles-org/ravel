#include <ui/views/memory.h>

#include <algorithm>
#include <charconv>
#include <cstring>
#include <format>
#include <ranges>

#include <app/ctx.h>
#include <imgui.h>
#include <ui/theme.h>

namespace ui {
    namespace {
        template <typename T>
        T cast_bytes(std::span<const std::byte> data) {
            if (data.size() < sizeof(T))
                return T{};
            T val;
            std::memcpy(&val, data.data(), sizeof(T));
            return val;
        }

        std::expected<std::uintptr_t, bool> parse_addr(std::string_view s) {
            auto start = s.find_first_not_of(" \t");
            if (start == std::string_view::npos)
                return std::unexpected(false);
            s.remove_prefix(start);

            auto end = s.find_last_not_of(" \t");
            if (end != std::string_view::npos)
                s = s.substr(0, end + 1);

            if (s.starts_with("0x") || s.starts_with("0X"))
                s.remove_prefix(2);
            if (s.empty())
                return std::unexpected(false);

            std::uintptr_t val = 0;
            auto res = std::from_chars(s.data(), s.data() + s.size(), val, 16);
            if (res.ec != std::errc{})
                return std::unexpected(false);
            return val;
        }
    } // namespace

    memory_view::memory_view() : view("Memory") {
        add_block("Default");
        select_block(0);
    }

    void memory_view::render() {
        if (app::active_target.get() != target) {
            target = app::active_target.get();
            refresh_cache();
        }

        draw_nav();
        ImGui::Separator();
        draw_table();
        draw_popups();

        if (auto_refresh) {
            static double last = 0.0;
            if (ImGui::GetTime() - last > refresh_rate) {
                refresh_cache();
                last = ImGui::GetTime();
            }
        }
    }

    void memory_view::draw_nav() {
        ImGui::SetNextItemWidth(180.0f);
        const auto preview = active_idx ? blocks[*active_idx]->name.c_str() : "Select...";

        if (ImGui::BeginCombo("##sel", preview)) {
            for (size_t i = 0; i < blocks.size(); ++i) {
                if (ImGui::Selectable(blocks[i]->name.c_str(), active_idx == i)) {
                    select_block(i);
                }
            }
            ImGui::EndCombo();
        }

        ImGui::SameLine();
        if (ImGui::Button("+"))
            show_create = true;

        ImGui::SameLine();
        ImGui::SetNextItemWidth(200.0f);
        if (ImGui::InputText("##addr", addr_buf, sizeof(addr_buf), ImGuiInputTextFlags_EnterReturnsTrue)) {
            if (active_idx) {
                if (auto val = parse_addr(addr_buf)) {
                    blocks[*active_idx]->base = *val;
                    refresh_cache();
                }
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Refresh"))
            refresh_cache();

        ImGui::SameLine();
        ImGui::Checkbox("Auto", &auto_refresh);

        if (auto_refresh) {
            ImGui::SameLine();
            ImGui::SetNextItemWidth(60.0f);
            ImGui::SliderFloat("##rate", &refresh_rate, 0.1f, 5.0f, "%.1fs");
        }
    }

    void memory_view::draw_table() {
        if (!active_idx || !target) {
            ImGui::TextDisabled(!target ? "No target attached." : "No block selected.");
            return;
        }

        const auto& blk = *blocks[*active_idx];
        if (blk.base == 0) {
            ImGui::TextDisabled("Set base address to view memory.");
            return;
        }

        if (!cache_valid) {
            ImGui::TextColored(theme::colors::red, "Read failed at 0x%lX", blk.base);
            return;
        }

        constexpr auto flags =
                ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY;

        if (ImGui::BeginTable("##mem_tbl", 4, flags)) {
            ImGui::TableSetupColumn("Offset", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Bytes", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("Meta", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            for (size_t i = 0; i < blk.fields.size(); ++i) {
                const auto& f = blk.fields[i];
                ImGui::PushID(static_cast<int>(i));
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::Text("+%03zX", f.offset);

                ImGui::TableSetColumnIndex(1);
                ImGui::PushStyleColor(ImGuiCol_Text, f.valid ? theme::colors::text : theme::colors::overlay0);
                if (ImGui::Selectable(
                            std::format("{}: {}", type_name(f.type), fmt_value(f)).c_str(), false,
                            ImGuiSelectableFlags_SpanAllColumns
                    )) {
                    ImGui::OpenPopup("##type_ctx");
                }
                ImGui::PopStyleColor();

                if (ImGui::BeginPopup("##type_ctx")) {
                    for (int t = 0; t <= static_cast<int>(type_id::hex); ++t) {
                        auto id = static_cast<type_id>(t);
                        if (ImGui::MenuItem(type_name(id), nullptr, f.type == id)) {
                            set_field_type(i, id);
                        }
                    }
                    ImGui::EndPopup();
                }

                ImGui::TableSetColumnIndex(2);
                if (f.valid) {
                    ImGui::TextColored(theme::colors::overlay1, "%s", fmt_hex(read_field(f)).c_str());
                }

                ImGui::TableSetColumnIndex(3);
                if (f.valid) {
                    ImGui::TextColored(theme::colors::green, "'%s'", fmt_txt(read_field(f)).c_str());
                    if (f.meta) {
                        ImGui::SameLine();
                        ImGui::TextColored(theme::colors::yellow, "-> %s", f.meta->c_str());
                    }
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
    }

    void memory_view::draw_popups() {
        if (show_create) {
            ImGui::OpenPopup("New Block");
            show_create = false;
            name_buf[0] = '\0';
        }

        if (ImGui::BeginPopupModal("New Block", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            if (ImGui::InputText("Name", name_buf, sizeof(name_buf), ImGuiInputTextFlags_EnterReturnsTrue)) {
                if (name_buf[0]) {
                    add_block(name_buf);
                    ImGui::CloseCurrentPopup();
                }
            }

            if (ImGui::Button("Create")) {
                if (name_buf[0]) {
                    add_block(name_buf);
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
    }

    void memory_view::add_block(std::string_view name) {
        blocks.push_back(std::make_unique<block>(name));
        select_block(blocks.size() - 1);
    }

    void memory_view::select_block(std::size_t idx) {
        if (idx >= blocks.size())
            return;
        active_idx = idx;

        if (blocks[idx]->base) {
            std::snprintf(addr_buf, sizeof(addr_buf), "0x%lX", blocks[idx]->base);
        } else {
            addr_buf[0] = '\0';
        }
        refresh_cache();
    }

    void memory_view::refresh_cache() {
        cache_valid = false;
        cache.clear();

        if (!active_idx || !target)
            return;
        auto& blk = *blocks[*active_idx];

        if (!blk.base)
            return;

        cache.resize(blk.size);
        if (target->read_memory(blk.base, cache)) {
            cache_valid = true;
        } else {
            cache.clear();
        }
        update_layout(0);
    }

    void memory_view::update_layout(std::size_t start_idx) {
        if (!active_idx)
            return;
        auto& blk = *blocks[*active_idx];

        if (start_idx < blk.fields.size()) {
            blk.fields.erase(blk.fields.begin() + static_cast<std::ptrdiff_t>(start_idx), blk.fields.end());
        }

        size_t off = (start_idx > 0) ? (blk.fields.back().offset + blk.fields.back().size) : 0;
        type_id type = (start_idx > 0) ? blk.fields.back().type : type_id::u32;

        while (off < blk.size) {
            field f{.offset = off, .type = type, .size = type_size(type)};

            if (off + f.size > blk.size)
                f.size = blk.size - off;

            f.valid = cache_valid && (off + f.size <= cache.size());
            if (f.valid) {
                f.meta = resolve_ptr(read_field(f));
            }

            blk.fields.push_back(f);
            off += f.size;
        }
    }

    void memory_view::set_field_type(std::size_t idx, type_id type) {
        if (!active_idx)
            return;
        auto& blk = *blocks[*active_idx];
        if (idx < blk.fields.size()) {
            blk.fields[idx].type = type;
            update_layout(idx);
        }
    }

    std::span<const std::byte> memory_view::read_field(const field& f) const {
        if (!f.valid)
            return {};
        return {cache.data() + f.offset, f.size};
    }

    std::string memory_view::fmt_value(const field& f) {
        auto d = read_field(f);
        if (d.empty())
            return "??";

        switch (f.type) {
            case type_id::i8:
                return std::format("{}", cast_bytes<std::int8_t>(d));
            case type_id::u8:
                return std::format("{}", cast_bytes<std::uint8_t>(d));
            case type_id::i16:
                return std::format("{}", cast_bytes<std::int16_t>(d));
            case type_id::u16:
                return std::format("{}", cast_bytes<std::uint16_t>(d));
            case type_id::i32:
                return std::format("{}", cast_bytes<std::int32_t>(d));
            case type_id::u32:
                return std::format("{}", cast_bytes<std::uint32_t>(d));
            case type_id::i64:
                return std::format("{}", cast_bytes<std::int64_t>(d));
            case type_id::u64:
                return std::format("{}", cast_bytes<std::uint64_t>(d));
            case type_id::f32:
                return std::format("{:.3f}", cast_bytes<float>(d));
            case type_id::f64:
                return std::format("{:.6f}", cast_bytes<double>(d));
            case type_id::ptr:
                return std::format("0x{:X}", cast_bytes<std::uintptr_t>(d));
            case type_id::txt:
                return fmt_txt(d);
            case type_id::hex:
                return fmt_hex(d);
        }
        return "?";
    }

    std::string memory_view::fmt_hex(std::span<const std::byte> data) {
        std::string out;
        out.reserve(data.size() * 3);
        for (auto b : data)
            std::format_to(std::back_inserter(out), "{:02X} ", std::to_integer<int>(b));
        if (!out.empty())
            out.pop_back();
        return out;
    }

    std::string memory_view::fmt_txt(std::span<const std::byte> data) {
        std::string out;
        out.reserve(data.size());
        for (auto b : data) {
            char c = static_cast<char>(b);
            out += std::isprint(c) ? c : '.';
        }
        return out;
    }

    std::optional<std::string> memory_view::resolve_ptr(std::span<const std::byte> data) {
        if (!target || data.size() < 4)
            return std::nullopt;

        uintptr_t ptr = (data.size() == 8) ? cast_bytes<uint64_t>(data) : cast_bytes<uint32_t>(data);
        if (ptr < 0x1000)
            return std::nullopt;

        char buf[64];
        if (auto res = target->read_memory(ptr, std::as_writable_bytes(std::span(buf)))) {
            buf[63] = 0;
            std::string_view sv(buf);
            if (auto end = sv.find('\0'); end != std::string_view::npos)
                sv = sv.substr(0, end);

            if (!sv.empty() && std::ranges::all_of(sv, [](char c) {
                    return std::isprint(c);
                })) {
                return std::string(sv);
            }
        }
        return std::nullopt;
    }

    size_t memory_view::type_size(type_id t) {
        switch (t) {
            case type_id::i8:
            case type_id::u8:
                return 1;
            case type_id::i16:
            case type_id::u16:
                return 2;
            case type_id::i32:
            case type_id::u32:
            case type_id::f32:
                return 4;
            case type_id::i64:
            case type_id::u64:
            case type_id::f64:
            case type_id::ptr:
                return 8;
            case type_id::txt:
            case type_id::hex:
                return 16;
        }
        return 4;
    }

    const char* memory_view::type_name(type_id t) {
        switch (t) {
            case type_id::i8:
                return "i8";
            case type_id::u8:
                return "u8";
            case type_id::i16:
                return "i16";
            case type_id::u16:
                return "u16";
            case type_id::i32:
                return "i32";
            case type_id::u32:
                return "u32";
            case type_id::i64:
                return "i64";
            case type_id::u64:
                return "u64";
            case type_id::f32:
                return "f32";
            case type_id::f64:
                return "f64";
            case type_id::ptr:
                return "ptr";
            case type_id::txt:
                return "txt";
            case type_id::hex:
                return "hex";
        }
        return "?";
    }
} // namespace ui
