#include <algorithm>
#include <app/ctx.h>
#include <cstring>
#include <numeric>
#include <ranges>
#include <ui/views/memory.h>
#include <ui/theme.h>

namespace ui {
    memory_view::memory_view() : view("Memory"), show_add_popup(false), auto_refresh(false), refresh_rate(1.0f) {
        std::memset(new_class_name, 0, sizeof(new_class_name));
        std::memset(addr_input, 0, sizeof(addr_input));
        add_class("Default Class");
    }

    void memory_view::render() {
        ImGui::Columns(2, "MemoryViewerColumns", true);

        static bool first_time = true;
        if (first_time) {
            ImGui::SetColumnWidth(0, 280.0f);
            first_time = false;
        }

        render_sidebar();
        ImGui::NextColumn();
        render_memory_view();
        ImGui::Columns(1);

        render_add_popup();
    }

    void memory_view::render_sidebar() {
        ImGui::PushStyleColor(ImGuiCol_Text, theme::colors::FG);
        ImGui::Text("Memory Classes");
        ImGui::PopStyleColor();

        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("+ Add New Class", ImVec2(-1, 30))) {
            show_add_popup = true;
        }
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        render_class_list();
    }

    void memory_view::render_class_list() {
        for (std::size_t i = 0; i < classes.size(); ++i) {
            const auto& cls = classes[i];
            ImGui::PushID(static_cast<int>(i));

            bool is_selected = (selected_idx == i);

            if (is_selected) {
                ImGui::PushStyleColor(ImGuiCol_Header, theme::with_alpha(theme::colors::BLUE, 0.8f));
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, theme::with_alpha(theme::colors::BLUE, 0.9f));
            }

            if (ImGui::Selectable(cls->name.c_str(), is_selected, 0, ImVec2(0, 25))) {
                selected_idx = i;
                refresh_data();
            }

            if (is_selected) {
                ImGui::PopStyleColor(2);
            }

            if (ImGui::BeginPopupContextItem()) {
                ImGui::Text("Class Options");
                ImGui::Separator();

                if (ImGui::MenuItem("Delete Class")) {
                    remove_class(i);
                    ImGui::EndPopup();
                    ImGui::PopID();
                    break;
                }

                if (ImGui::MenuItem("Rename Class")) {
                }

                ImGui::Separator();
                ImGui::Text("Memory Size:");

                static const std::vector<std::size_t> sizes = {256, 512, 1024, 2048, 4096, 8192};
                for (auto sz : sizes) {
                    char label[64];
                    snprintf(label, sizeof(label), "%zu bytes", sz);
                    if (ImGui::MenuItem(label, nullptr, cls->size == sz)) {
                        cls->size = sz;
                        if (selected_idx == i) {
                            refresh_data();
                        }
                    }
                }
                ImGui::EndPopup();
            }

            if (cls->addr != 0) {
                ImGui::Indent(15.0f);
                ImGui::PushStyleColor(ImGuiCol_Text, theme::colors::FG_DARK);
                ImGui::Text("0x%016lX", cls->addr);
                ImGui::Text("%zu bytes", cls->size);
                ImGui::PopStyleColor();
                ImGui::Unindent(15.0f);
            } else {
                ImGui::Indent(15.0f);
                ImGui::PushStyleColor(ImGuiCol_Text, theme::colors::RED);
                ImGui::Text("No address set");
                ImGui::PopStyleColor();
                ImGui::Unindent(15.0f);
            }

            ImGui::Spacing();
            ImGui::PopID();
        }
    }

    void memory_view::render_add_popup() {
        if (show_add_popup) {
            ImGui::OpenPopup("Add Memory Class");
        }

        if (ImGui::BeginPopupModal("Add Memory Class", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Create a new memory class:");
            ImGui::Spacing();

            ImGui::Text("Class Name:");
            ImGui::SetNextItemWidth(250);
            ImGui::InputText("##class_name", new_class_name, sizeof(new_class_name));

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_Button, theme::colors::GREEN);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, theme::with_alpha(theme::colors::GREEN, 0.9f));
            if (ImGui::Button("Create", ImVec2(120, 30))) {
                if (std::strlen(new_class_name) > 0) {
                    add_class(std::string_view(new_class_name));
                    std::memset(new_class_name, 0, sizeof(new_class_name));
                }
                show_add_popup = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopStyleColor(2);

            ImGui::SameLine();

            ImGui::PushStyleColor(ImGuiCol_Button, theme::colors::RED);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, theme::with_alpha(theme::colors::RED, 0.9f));
            if (ImGui::Button("Cancel", ImVec2(120, 30))) {
                show_add_popup = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopStyleColor(2);

            ImGui::EndPopup();
        }
    }

    void memory_view::render_memory_view() {
        if (!selected_idx) {
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 50);
            ImGui::PushStyleColor(ImGuiCol_Text, theme::colors::FG_DARK);

            float window_width = ImGui::GetContentRegionAvail().x;
            const char* text = "Select a memory class to view its contents";
            float text_width = ImGui::CalcTextSize(text).x;
            ImGui::SetCursorPosX((window_width - text_width) * 0.5f);

            ImGui::Text("%s", text);
            ImGui::PopStyleColor();
            return;
        }

        auto& selected_class = classes[*selected_idx];

        ImGui::PushStyleColor(ImGuiCol_Text, theme::colors::FG);
        ImGui::Text("Memory Inspector - %s", selected_class->name.c_str());
        ImGui::PopStyleColor();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Text("Memory Address:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(220);

        if (ImGui::InputText(
                    "##address", addr_input, sizeof(addr_input), ImGuiInputTextFlags_CallbackCharFilter,
                    [](ImGuiInputTextCallbackData* data) -> int {
                        const char c = static_cast<char>(data->EventChar);
                        if (std::isxdigit(static_cast<unsigned char>(c)) || c == 'x' || c == 'X' || c == '[' ||
                            c == ']' || c == '+' || c == '-' || std::isspace(static_cast<unsigned char>(c))) {
                            return 0;
                        }
                        return 1;
                    }
            )) {
            if (auto parsed_address = parse_address_input(std::string_view{addr_input}); parsed_address) {
                selected_class->addr = *parsed_address;
                refresh_data();
            }
        }

        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, theme::colors::BLUE);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, theme::with_alpha(theme::colors::BLUE, 0.9f));
        if (ImGui::Button("Refresh")) {
            refresh_data();
        }
        ImGui::PopStyleColor(2);

        ImGui::SameLine();
        ImGui::Checkbox("Auto-refresh", &auto_refresh);

        if (auto_refresh) {
            ImGui::SameLine();
            ImGui::SetNextItemWidth(100);
            ImGui::SliderFloat("##refresh_rate", &refresh_rate, 0.1f, 5.0f, "%.1fs");

            static float last_refresh = 0.0f;
            float now = static_cast<float>(ImGui::GetTime());
            if (now - last_refresh >= refresh_rate) {
                refresh_data();
                last_refresh = now;
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        render_memory_table();
    }

    void memory_view::render_memory_table() {
        if (!app::proc || !app::proc->is_attached()) {
            ImGui::PushStyleColor(ImGuiCol_Text, theme::colors::RED);
            ImGui::Text("No process attached");
            ImGui::PopStyleColor();
            return;
        }

        if (entries.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, theme::colors::ORANGE);
            ImGui::Text("No memory data available - set an address to begin");
            ImGui::PopStyleColor();
            return;
        }

        if (ImGui::BeginTable(
                    "MemoryTable", 6,
                    ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
                            ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable
            )) {

            ImGui::TableSetupColumn("Offset", ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthFixed, 130.0f);
            ImGui::TableSetupColumn("Raw Bytes", ImGuiTableColumnFlags_WidthFixed, 160.0f);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("ASCII", ImGuiTableColumnFlags_WidthFixed, 120.0f);
            ImGui::TableSetupColumn("String ->", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            for (std::size_t i = 0; i < entries.size(); ++i) {
                const auto& entry = entries[i];
                ImGui::TableNextRow();
                ImGui::PushID(static_cast<int>(i));

                ImGui::TableSetColumnIndex(0);
                ImGui::Text("+0x%04lX", entry.offset);

                ImGui::TableSetColumnIndex(1);
                ImGui::PushStyleColor(ImGuiCol_Text, theme::colors::CYAN);
                ImGui::Text("0x%08lX", static_cast<unsigned long>(entry.addr));
                ImGui::PopStyleColor();

                ImGui::TableSetColumnIndex(2);
                if (entry.valid) {
                    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
                    ImGui::PushStyleColor(ImGuiCol_Text, theme::get_bytes_color());
                    ImGui::Text("%s", format_hex(entry.data).c_str());
                    ImGui::PopStyleColor();
                    ImGui::PopFont();
                } else {
                    ImGui::TextColored(theme::colors::RED, "read error");
                }

                ImGui::TableSetColumnIndex(3);
                if (entry.valid) {
                    std::string combined_display_str =
                            std::string(get_type_name(entry.type)) + ": " + format_typed_value(entry);
                    ImGui::PushStyleColor(ImGuiCol_Text, theme::get_value_color());
                    bool item_clicked = ImGui::Selectable(
                            combined_display_str.c_str(), false,
                            ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap
                    );
                    ImGui::PopStyleColor();

                    char popup_id[32];
                    snprintf(popup_id, sizeof(popup_id), "TypeMenu_%zu", i);

                    if (item_clicked) {
                        ImGui::OpenPopup(popup_id);
                    }

                    if (ImGui::BeginPopupContextItem(popup_id)) {
                        if (!ImGui::IsPopupOpen(popup_id)) {
                            ImGui::OpenPopup(popup_id);
                        }
                    }

                    if (ImGui::BeginPopup(popup_id)) {
                        ImGui::Text("Change Data Type:");
                        ImGui::Separator();

                        static const std::vector<std::pair<memory_type, const char*>> type_options = {
                                {memory_type::int8,    "Int8 (1 byte)"    },
                                {memory_type::uint8,   "UInt8 (1 byte)"   },
                                {memory_type::int16,   "Int16 (2 bytes)"  },
                                {memory_type::uint16,  "UInt16 (2 bytes)" },
                                {memory_type::int32,   "Int32 (4 bytes)"  },
                                {memory_type::uint32,  "UInt32 (4 bytes)" },
                                {memory_type::int64,   "Int64 (8 bytes)"  },
                                {memory_type::uint64,  "UInt64 (8 bytes)" },
                                {memory_type::float32, "Float (4 bytes)"  },
                                {memory_type::float64, "Double (8 bytes)" },
                                {memory_type::pointer, "Pointer (8 bytes)"},
                                {memory_type::text,    "Text"             },
                                {memory_type::bytes,   "Raw Bytes"        }
                        };

                        for (const auto& [type_val, name] : type_options) {
                            if (ImGui::MenuItem(name, nullptr, entry.type == type_val)) {
                                change_entry_type(i, type_val);
                            }
                        }
                        ImGui::EndPopup();
                    }
                } else {
                    ImGui::TextColored(theme::colors::RED, "invalid");
                }

                ImGui::TableSetColumnIndex(4);
                if (entry.valid) {
                    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
                    ImGui::PushStyleColor(ImGuiCol_Text, theme::colors::GREEN);
                    std::string ascii_repr = format_ascii(entry.data);
                    ImGui::Text("'%s'", ascii_repr.c_str());
                    ImGui::PopStyleColor();
                    ImGui::PopFont();
                } else {
                    ImGui::TextColored(theme::colors::FG_DARK, "---");
                }

                ImGui::TableSetColumnIndex(5);
                if (entry.valid && entry.dereferenced_string) {
                    ImGui::PushStyleColor(ImGuiCol_Text, theme::colors::YELLOW);
                    std::string display_str = *entry.dereferenced_string;
                    if (display_str.length() > 40) {
                        display_str = display_str.substr(0, 37) + "...";
                    }
                    ImGui::Text("\"%s\"", display_str.c_str());
                    ImGui::PopStyleColor();

                    if (ImGui::IsItemHovered() && entry.dereferenced_string->length() > 40) {
                        ImGui::BeginTooltip();
                        ImGui::Text("Full string: \"%s\"", entry.dereferenced_string->c_str());
                        ImGui::EndTooltip();
                    }
                } else {
                    ImGui::TextColored(theme::colors::FG_DARK, "---");
                }

                ImGui::PopID();
            }

            ImGui::EndTable();
        }
    }

    std::string memory_view::format_ascii(std::span<const std::byte> data) {
        std::string result;
        result.reserve(data.size());

        for (const auto& byte : data) {
            unsigned char c = static_cast<unsigned char>(byte);
            if (c >= 32 && c <= 126) {
                result += static_cast<char>(c);
            } else if (c == '\n') {
                result += "\\n";
            } else if (c == '\r') {
                result += "\\r";
            } else if (c == '\t') {
                result += "\\t";
            } else {
                result += ".";
            }
        }
        return result;
    }

    void memory_view::add_class(std::string_view name) {
        classes.emplace_back(std::make_unique<memory_class>(name));
    }

    void memory_view::remove_class(std::size_t index) {
        if (index >= classes.size())
            return;

        classes.erase(classes.begin() + static_cast<ptrdiff_t>(index));

        if (selected_idx.has_value()) {
            if (*selected_idx == index) {
                selected_idx.reset();
            } else if (*selected_idx > index) {
                --(*selected_idx);
            }
        }
    }

    void memory_view::refresh_data() {
        if (!selected_idx.has_value())
            return;

        const auto& selected_class = classes[*selected_idx];
        if (selected_class->addr == 0)
            return;

        entries.clear();

        std::uintptr_t current_addr = selected_class->addr;
        std::size_t bytes_processed = 0;

        while (bytes_processed < selected_class->size) {
            memory_entry entry;
            entry.offset = bytes_processed;
            entry.addr = current_addr;
            entry.type = memory_type::int32;
            entry.type_size = get_type_size(entry.type);

            std::size_t bytes_to_read = std::min(entry.type_size, selected_class->size - bytes_processed);

            auto result = read_memory(entry.addr, bytes_to_read);
            if (result.has_value()) {
                entry.data = std::move(result.value());
                entry.valid = true;
                entry.dereferenced_string = dereference_as_string(entry.data);
            } else {
                entry.data.resize(entry.type_size);
                entry.valid = false;
            }

            entries.emplace_back(std::move(entry));

            current_addr += entry.type_size;
            bytes_processed += entry.type_size;
        }
    }

    std::expected<std::uintptr_t, bool> memory_view::parse_address_input(std::string_view input) {
        if (input.empty()) {
            return std::unexpected(false);
        }

        bool do_deref = input.starts_with('[') && input.ends_with(']');
        if (do_deref) {
            input.remove_prefix(1);
            input.remove_suffix(1);
        }

        if (input.starts_with("0x") || input.starts_with("0X")) {
            input.remove_prefix(2);
        }

        std::uintptr_t addr = 0;
        auto [ptr, ec] = std::from_chars(input.data(), input.data() + input.size(), addr, 16);
        if (ec != std::errc()) {
            return std::unexpected(false);
        }

        if (do_deref) {
            if (auto pr = dereference_pointer(addr); pr.has_value()) {
                return *pr;
            }
            return std::unexpected(false);
        }

        return addr;
    }

    std::expected<std::uintptr_t, bool> memory_view::dereference_pointer(std::uintptr_t ptr_addr) {
        if (!app::proc || !app::proc->is_attached()) {
            return std::unexpected(false);
        }

        std::array<std::byte, sizeof(std::uintptr_t)> buffer{};
        auto result = app::proc->read_memory(ptr_addr, std::span(buffer));

        if (!result.has_value()) {
            return std::unexpected(false);
        }

        std::uintptr_t target_addr = 0;
        std::memcpy(&target_addr, buffer.data(), sizeof(target_addr));

        if (target_addr == 0) {
            return std::unexpected(false);
        }

        return target_addr;
    }

    std::optional<std::string> memory_view::dereference_as_string(std::span<const std::byte> data) {
        if (data.size() < 4) {
            return std::nullopt;
        }

        auto potential_addr = bytes_to<std::uint32_t>(data);

        if (!is_valid_addr(static_cast<std::uintptr_t>(potential_addr))) {
            return std::nullopt;
        }

        return read_string(static_cast<std::uintptr_t>(potential_addr));
    }

    std::optional<std::string> memory_view::read_string(std::uintptr_t addr, std::size_t max_len) {
        if (!app::proc || !app::proc->is_attached()) {
            return std::nullopt;
        }

        std::string result;
        bool success = false;

        result.resize_and_overwrite(max_len, [&](char* data, std::size_t size) -> std::size_t {
            auto read_op = app::proc->read_memory(addr, std::as_writable_bytes(std::span(data, size)));
            if (!read_op) {
                return 0;
            }
            success = true;
            return max_len;
        });

        if (!success) {
            return std::nullopt;
        }

        auto null_terminator = std::find(result.begin(), result.end(), '\0');
        if (null_terminator == result.end()) {
            return std::nullopt;
        }

        result.erase(null_terminator, result.end());

        if (!std::ranges::all_of(result, [](char c) {
                return std::isprint(static_cast<unsigned char>(c));
            })) {
            return std::nullopt;
        }

        return result;
    }

    std::expected<std::vector<std::byte>, bool> memory_view::read_memory(std::uintptr_t addr, std::size_t size) {
        if (!app::proc || !app::proc->is_attached()) {
            return std::unexpected(false);
        }

        std::vector<std::byte> buffer(size);
        auto result = app::proc->read_memory(addr, std::span<std::byte>(buffer));

        if (result.has_value()) {
            return buffer;
        }
        return std::unexpected(false);
    }

    std::string memory_view::format_hex(std::span<const std::byte> data) {
        return std::ranges::fold_left(
                data | std::ranges::views::transform([](std::byte b) {
                    return std::format("{:02X}", std::to_integer<unsigned int>(b));
                }),
                std::string{}, [](std::string acc, std::string val) {
                    if (!acc.empty())
                        acc += ' ';
                    acc += std::move(val);
                    return acc;
                }
        );
    }

    std::string memory_view::format_typed_value(const memory_entry& entry) {
        if (!entry.valid || entry.data.empty()) {
            return "invalid";
        }

        switch (entry.type) {
            case memory_type::int8:
                if (entry.data.size() >= 1) {
                    return std::format("{}", bytes_to<std::int8_t>(entry.data));
                }
                break;
            case memory_type::uint8:
                if (entry.data.size() >= 1) {
                    return std::format("{}", bytes_to<std::uint8_t>(entry.data));
                }
                break;
            case memory_type::int16:
                if (entry.data.size() >= 2) {
                    return std::format("{}", bytes_to<std::int16_t>(entry.data));
                }
                break;
            case memory_type::uint16:
                if (entry.data.size() >= 2) {
                    return std::format("{}", bytes_to<std::uint16_t>(entry.data));
                }
                break;
            case memory_type::int32:
                if (entry.data.size() >= 4) {
                    return std::format("{}", bytes_to<std::int32_t>(entry.data));
                }
                break;
            case memory_type::uint32:
                if (entry.data.size() >= 4) {
                    return std::format("{}", bytes_to<std::uint32_t>(entry.data));
                }
                break;
            case memory_type::int64:
                if (entry.data.size() >= 8) {
                    return std::format("{}", bytes_to<std::int64_t>(entry.data));
                }
                break;
            case memory_type::uint64:
                if (entry.data.size() >= 8) {
                    return std::format("{}", bytes_to<std::uint64_t>(entry.data));
                }
                break;
            case memory_type::float32:
                if (entry.data.size() >= 4) {
                    return std::format("{:.6f}", bytes_to<float>(entry.data));
                }
                break;
            case memory_type::float64:
                if (entry.data.size() >= 8) {
                    return std::format("{:.6f}", bytes_to<double>(entry.data));
                }
                break;
            case memory_type::pointer:
                if (entry.data.size() >= sizeof(std::uintptr_t)) {
                    return std::format("0x{:X}", bytes_to<std::uintptr_t>(entry.data));
                }
                break;
            case memory_type::text:
                return format_ascii(entry.data);
            case memory_type::bytes:
                return format_hex(entry.data);
        }

        return "invalid";
    }

    template <typename T>
    T memory_view::bytes_to(std::span<const std::byte> data)
        requires(std::is_trivially_copyable_v<T> && std::is_standard_layout_v<T>)
    {
        if (data.size() < sizeof(T)) {
            return T{};
        }

        T value;
        std::memcpy(&value, data.data(), sizeof(T));
        return value;
    }

    std::size_t memory_view::get_type_size(memory_type type) {
        switch (type) {
            case memory_type::int8:
            case memory_type::uint8:
                return 1;
            case memory_type::int16:
            case memory_type::uint16:
                return 2;
            case memory_type::int32:
            case memory_type::uint32:
            case memory_type::float32:
                return 4;
            case memory_type::int64:
            case memory_type::uint64:
            case memory_type::float64:
            case memory_type::pointer:
                return 8;
            case memory_type::text:
            case memory_type::bytes:
                return 16;
        }
        return 4;
    }

    const char* memory_view::get_type_name(memory_type type) {
        switch (type) {
            case memory_type::int8:
                return "Int8";
            case memory_type::uint8:
                return "UInt8";
            case memory_type::int16:
                return "Int16";
            case memory_type::uint16:
                return "UInt16";
            case memory_type::int32:
                return "Int32";
            case memory_type::uint32:
                return "UInt32";
            case memory_type::int64:
                return "Int64";
            case memory_type::uint64:
                return "UInt64";
            case memory_type::float32:
                return "Float";
            case memory_type::float64:
                return "Double";
            case memory_type::pointer:
                return "Pointer";
            case memory_type::text:
                return "Text";
            case memory_type::bytes:
                return "Bytes";
        }
        return "Unknown";
    }

    void memory_view::change_entry_type(std::size_t entry_idx, memory_type new_type) {
        if (entry_idx >= entries.size()) {
            return;
        }

        entries[entry_idx].type = new_type;
        entries[entry_idx].type_size = get_type_size(new_type);

        auto result = read_memory(entries[entry_idx].addr, entries[entry_idx].type_size);
        if (result.has_value()) {
            entries[entry_idx].data = std::move(result.value());
            entries[entry_idx].valid = true;
            entries[entry_idx].dereferenced_string = dereference_as_string(entries[entry_idx].data);
        } else {
            entries[entry_idx].data.resize(entries[entry_idx].type_size);
            entries[entry_idx].valid = false;
        }

        rebuild_entries_from_index(entry_idx + 1);
    }

    void memory_view::rebuild_entries_from_index(std::size_t start_idx) {
        if (!selected_idx || start_idx > entries.size())
            return;

        const auto& selected_class = classes[*selected_idx];
        std::size_t bytes_processed = std::accumulate(
                entries.begin(), entries.begin() + static_cast<ptrdiff_t>(start_idx), 0ULL,
                [](std::size_t sum, const memory_entry& e) {
                    return sum + e.type_size;
                }
        );

        entries.erase(entries.begin() + static_cast<ptrdiff_t>(start_idx), entries.end());

        std::uintptr_t current_addr = selected_class->addr + bytes_processed;
        const auto default_type = [&]() -> memory_type {
            if (start_idx > 0 && start_idx <= entries.size())
                return entries[start_idx - 1].type;
            return memory_type::int32;
        }();

        while (bytes_processed < selected_class->size) {
            memory_entry entry;
            entry.offset = bytes_processed;
            entry.addr = current_addr;
            entry.type = default_type;

            entry.type_size = get_type_size(entry.type);
            std::size_t bytes_to_read = std::min(entry.type_size, selected_class->size - bytes_processed);
            if (bytes_to_read == 0 && selected_class->size > bytes_processed)
                bytes_to_read = entry.type_size;

            if (bytes_to_read == 0)
                break;

            if (auto result = read_memory(entry.addr, bytes_to_read)) {
                entry.data = std::move(*result);
                entry.valid = true;
                entry.dereferenced_string = dereference_as_string(entry.data);
            } else {
                entry.data.resize(bytes_to_read);
                entry.valid = false;
            }

            entries.emplace_back(std::move(entry));

            current_addr += entry.type_size;
            bytes_processed += entry.type_size;
        }
    }

    bool memory_view::is_valid_addr(std::uintptr_t addr) {
        // todo: proper address validation from pantsir
        return addr != 0;
    }
} // namespace ui
