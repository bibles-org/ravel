#include <algorithm>
#include <app/ctx.h>
#include <cstring>
#include <format>
#include <numeric>
#include <ranges>
#include <ui/theme.h>
#include <ui/views/memory.h>
#include <charconv>

namespace ui {
    memory_view::memory_view() :
        view("Memory"), m_buffer_read_success(false), show_add_popup(false), auto_refresh(false), refresh_rate(1.0f) {
        std::memset(new_class_name, 0, sizeof(new_class_name));
        std::memset(addr_input, 0, sizeof(addr_input));
        add_class("Default Class");
    }

    void memory_view::render() {
        const float sidebar_width = 240.0f;

        ImGui::BeginChild("Sidebar", ImVec2(sidebar_width, 0), true);
        render_sidebar();
        ImGui::EndChild();

        ImGui::SameLine();

        ImGui::BeginChild("Content", ImVec2(0, 0), true);
        render_memory_view();
        ImGui::EndChild();

        render_add_popup();
    }

    void memory_view::render_sidebar() {
        ImGui::Text("Memory Classes");
        ImGui::Separator();

        if (ImGui::Button("+ Add Class", ImVec2(-1, 0))) {
            show_add_popup = true;
        }
        ImGui::Separator();

        render_class_list();
    }

    void memory_view::render_class_list() {
        for (std::size_t i = 0; i < classes.size(); ++i) {
            const auto& cls = classes[i];
            ImGui::PushID(static_cast<int>(i));

            bool is_selected = (selected_idx == i);

            if (is_selected) {
                ImGui::PushStyleColor(ImGuiCol_Header, theme::with_alpha(theme::colors::blue, 0.4f));
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, theme::with_alpha(theme::colors::blue, 0.6f));
            }

            if (ImGui::Selectable(cls->name.c_str(), is_selected, 0, ImVec2(0, 45))) {
                selected_idx = i;
                refresh_data();
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
                    // TODO: Implement rename functionality
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

            if (is_selected) {
                ImGui::PopStyleColor(2);
            }

            ImVec2 last_item_min = ImGui::GetItemRectMin();
            ImVec2 original_cursor_pos = ImGui::GetCursorPos();

            ImGui::SetCursorScreenPos(ImVec2(last_item_min.x + 10.0f, last_item_min.y + 22.0f));

            if (cls->addr != 0) {
                ImGui::PushStyleColor(ImGuiCol_Text, theme::colors::base);
                ImGui::TextUnformatted(std::format("0x{:016X}", cls->addr).c_str());
                ImGui::PopStyleColor();
            } else {
                ImGui::PushStyleColor(ImGuiCol_Text, theme::colors::red);
                ImGui::TextUnformatted("No address set");
                ImGui::PopStyleColor();
            }

            ImGui::SetCursorPos(original_cursor_pos);

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

            if (ImGui::Button("Create", ImVec2(120, 0))) {
                if (std::strlen(new_class_name) > 0) {
                    add_class(std::string_view(new_class_name));
                    std::memset(new_class_name, 0, sizeof(new_class_name));
                }
                show_add_popup = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();

            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                show_add_popup = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }

    void memory_view::render_memory_view() {
        if (!selected_idx) {
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 50);
            ImGui::PushStyleColor(ImGuiCol_Text, theme::colors::base);

            float window_width = ImGui::GetContentRegionAvail().x;
            const char* text = "Select a memory class to view its contents";
            float text_width = ImGui::CalcTextSize(text).x;
            ImGui::SetCursorPosX((window_width - text_width) * 0.5f);

            ImGui::Text("%s", text);
            ImGui::PopStyleColor();
            return;
        }

        auto& selected_class = classes[*selected_idx];

        ImGui::Text("Memory Inspector - %s", selected_class->name.c_str());
        ImGui::Separator();

        ImGui::SetNextItemWidth(220);
        if (ImGui::InputTextWithHint(
                    "##address", "Address (e.g., 0xDEADBEEF)", addr_input, sizeof(addr_input),
                    ImGuiInputTextFlags_CallbackCharFilter, [](ImGuiInputTextCallbackData* data) -> int {
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
        if (ImGui::Button("Refresh")) {
            refresh_data();
        }

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
        ImGui::Separator();

        render_memory_table();
    }

    std::span<const std::byte> memory_view::get_entry_data_span(const memory_entry& entry) const {
        if (!m_buffer_read_success || entry.data_offset >= memory_buffer.size() || entry.data_size == 0) {
            return {};
        }
        std::size_t actual_size = std::min(entry.data_size, memory_buffer.size() - entry.data_offset);
        if (actual_size == 0 && entry.type_size > 0)
            return {};
        return {memory_buffer.data() + entry.data_offset, actual_size};
    }

    void memory_view::render_memory_table() {
        if (!app::proc || !app::proc->is_attached()) {
            ImGui::PushStyleColor(ImGuiCol_Text, theme::colors::red);
            ImGui::Text("No process attached");
            ImGui::PopStyleColor();
            return;
        }

        if (entries.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, theme::colors::peach);
            if (!m_buffer_read_success && selected_idx.has_value() && classes[*selected_idx]->addr != 0) {
                ImGui::Text("Failed to read memory data");
            } else {
                ImGui::Text("Set an address to begin");
            }
            ImGui::PopStyleColor();
            return;
        }

        const ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable |
                                      ImGuiTableFlags_Sortable | ImGuiTableFlags_BordersInnerV;
        if (ImGui::BeginTable("MemoryTable", 6, flags)) {

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
                ImGui::TextUnformatted(std::format("+0x{:04X}", entry.offset).c_str());
                ImGui::TableSetColumnIndex(1);
                ImGui::PushStyleColor(ImGuiCol_Text, theme::colors::blue);
                ImGui::TextUnformatted(std::format("0x{:08X}", entry.addr).c_str());
                ImGui::PopStyleColor();

                ImGui::TableSetColumnIndex(2);
                if (entry.valid) {
                    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
                    ImGui::PushStyleColor(ImGuiCol_Text, theme::get_bytes_color());
                    ImGui::Text("%s", format_hex(get_entry_data_span(entry)).c_str());
                    ImGui::PopStyleColor();
                    ImGui::PopFont();
                } else {
                    ImGui::TextColored(theme::colors::red, "read error");
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

                    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                        ImGui::OpenPopup(popup_id);
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
                    ImGui::TextColored(theme::colors::red, "invalid");
                }

                ImGui::TableSetColumnIndex(4);
                if (entry.valid) {
                    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
                    ImGui::PushStyleColor(ImGuiCol_Text, theme::colors::green);
                    std::string ascii_repr = format_ascii(get_entry_data_span(entry));
                    ImGui::Text("'%s'", ascii_repr.c_str());
                    ImGui::PopStyleColor();
                    ImGui::PopFont();
                } else {
                    ImGui::TextColored(theme::colors::base, "---");
                }

                ImGui::TableSetColumnIndex(5);
                if (entry.valid && entry.dereferenced_string) {
                    ImGui::PushStyleColor(ImGuiCol_Text, theme::colors::yellow);
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
                    ImGui::TextColored(theme::colors::base, "---");
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
                entries.clear();
                memory_buffer.clear();
                m_buffer_read_success = false;
            } else if (*selected_idx > index) {
                --(*selected_idx);
            }
        }
    }

    void memory_view::refresh_data() {
        entries.clear();
        memory_buffer.clear();
        m_buffer_read_success = false;

        if (!selected_idx.has_value()) {
            return;
        }

        const auto& selected_class = classes[*selected_idx];
        if (selected_class->addr == 0 || selected_class->size == 0) {
            return;
        }

        auto read_result = read_memory(selected_class->addr, selected_class->size);
        if (read_result.has_value()) {
            memory_buffer = std::move(read_result.value());
            m_buffer_read_success = true;
        } else {
            // memory_buffer remains empty, m_buffer_read_success is false
        }

        std::uintptr_t current_addr_base = selected_class->addr;
        std::size_t class_processed_bytes = 0;

        while (class_processed_bytes < selected_class->size) {
            memory_entry entry;
            entry.offset = class_processed_bytes;
            entry.addr = current_addr_base + class_processed_bytes;
            entry.type = memory_type::int32;
            entry.type_size = get_type_size(entry.type);

            entry.data_offset = class_processed_bytes;

            std::size_t remaining_in_class_logical = selected_class->size - class_processed_bytes;
            std::size_t actual_buffer_available =
                    m_buffer_read_success
                            ? (memory_buffer.size() > entry.data_offset ? memory_buffer.size() - entry.data_offset : 0)
                            : 0;

            entry.data_size = std::min({entry.type_size, remaining_in_class_logical, actual_buffer_available});

            entry.valid = m_buffer_read_success && (entry.data_offset < memory_buffer.size()) &&
                          (entry.data_offset + entry.data_size <= memory_buffer.size());
            if (entry.type_size > 0 && entry.data_size == 0) {
                entry.valid = false;
            }
            if (!m_buffer_read_success && class_processed_bytes < selected_class->size) {
                entry.valid = false;
            }


            if (entry.valid) {
                entry.dereferenced_string = dereference_as_string(get_entry_data_span(entry));
            } else {
                entry.dereferenced_string = std::nullopt;
            }

            entries.emplace_back(std::move(entry));

            class_processed_bytes += entry.type_size;
            if (entry.type_size == 0)
                break;
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

        std::uintptr_t potential_addr = 0;

        if (data.size() >= sizeof(std::uintptr_t)) {
            potential_addr = bytes_to<std::uintptr_t>(data.first(sizeof(std::uintptr_t)));
        } else if (data.size() >= sizeof(std::uint32_t)) {
            potential_addr = static_cast<std::uintptr_t>(bytes_to<std::uint32_t>(data.first(sizeof(std::uint32_t))));
        } else {
            return std::nullopt;
        }

        if (!is_valid_addr(potential_addr)) {
            return std::nullopt;
        }

        return read_string(potential_addr);
    }

    std::optional<std::string> memory_view::read_string(std::uintptr_t addr, std::size_t max_len) {
        if (!app::proc || !app::proc->is_attached()) {
            return std::nullopt;
        }

        std::string result_str;
        bool success = false;

        result_str.resize_and_overwrite(max_len, [&](char* data, std::size_t requested_size) -> std::size_t {
            auto read_op = app::proc->read_memory(addr, std::as_writable_bytes(std::span(data, requested_size)));
            if (!read_op) {
                success = false;
                return 0;
            }
            success = true;
            auto null_terminator_in_buffer = std::find(data, data + requested_size, '\0');
            return static_cast<std::size_t>(std::distance(data, null_terminator_in_buffer));
        });

        if (!success) {
            return std::nullopt;
        }

        if (result_str.empty() && max_len > 0) {
            // if read was successful but string is empty
        }

        if (!std::ranges::all_of(result_str, [](char c) {
                return std::isprint(static_cast<unsigned char>(c));
            })) {
            return std::nullopt;
        }

        return result_str;
    }

    std::expected<std::vector<std::byte>, bool> memory_view::read_memory(std::uintptr_t addr, std::size_t size) {
        if (!app::proc || !app::proc->is_attached()) {
            return std::unexpected(false);
        }
        if (size == 0) {
            return std::vector<std::byte>();
        }

        std::vector<std::byte> buffer(size);
        auto operation_result = app::proc->read_memory(addr, std::span<std::byte>(buffer));

        if (operation_result.has_value()) {
            return buffer;
        }
        return std::unexpected(false);
    }

    std::string memory_view::format_hex(std::span<const std::byte> data) {
        if (data.empty())
            return "";
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
        std::span<const std::byte> data_span = get_entry_data_span(entry);
        if (!entry.valid || data_span.empty()) {
            return "invalid";
        }

        switch (entry.type) {
            case memory_type::int8:
                if (data_span.size() >= 1) {
                    return std::format("{}", bytes_to<std::int8_t>(data_span));
                }
                break;
            case memory_type::uint8:
                if (data_span.size() >= 1) {
                    return std::format("{}", bytes_to<std::uint8_t>(data_span));
                }
                break;
            case memory_type::int16:
                if (data_span.size() >= 2) {
                    return std::format("{}", bytes_to<std::int16_t>(data_span));
                }
                break;
            case memory_type::uint16:
                if (data_span.size() >= 2) {
                    return std::format("{}", bytes_to<std::uint16_t>(data_span));
                }
                break;
            case memory_type::int32:
                if (data_span.size() >= 4) {
                    return std::format("{}", bytes_to<std::int32_t>(data_span));
                }
                break;
            case memory_type::uint32:
                if (data_span.size() >= 4) {
                    return std::format("{}", bytes_to<std::uint32_t>(data_span));
                }
                break;
            case memory_type::int64:
                if (data_span.size() >= 8) {
                    return std::format("{}", bytes_to<std::int64_t>(data_span));
                }
                break;
            case memory_type::uint64:
                if (data_span.size() >= 8) {
                    return std::format("{}", bytes_to<std::uint64_t>(data_span));
                }
                break;
            case memory_type::float32:
                if (data_span.size() >= 4) {
                    return std::format("{:.6f}", bytes_to<float>(data_span));
                }
                break;
            case memory_type::float64:
                if (data_span.size() >= 8) {
                    return std::format("{:.6f}", bytes_to<double>(data_span));
                }
                break;
            case memory_type::pointer:
                if (data_span.size() >= sizeof(std::uintptr_t)) {
                    return std::format("0x{:X}", bytes_to<std::uintptr_t>(data_span));
                } else if (data_span.size() >= sizeof(std::uint32_t)) {
                    return std::format("0x{:X}", static_cast<std::uintptr_t>(bytes_to<std::uint32_t>(data_span)));
                }
                break;
            case memory_type::text:
                return format_ascii(data_span);
            case memory_type::bytes:
                return format_hex(data_span);
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
        if (entry_idx >= entries.size() || !selected_idx.has_value()) {
            return;
        }

        const auto& selected_class = classes[*selected_idx];
        memory_entry& current_entry = entries[entry_idx];

        current_entry.type = new_type;
        current_entry.type_size = get_type_size(new_type);

        std::size_t remaining_in_class_logical = selected_class->size - current_entry.offset;
        std::size_t actual_buffer_available =
                (m_buffer_read_success && current_entry.data_offset < memory_buffer.size())
                        ? (memory_buffer.size() - current_entry.data_offset)
                        : 0;

        current_entry.data_size =
                std::min({current_entry.type_size, remaining_in_class_logical, actual_buffer_available});

        current_entry.valid = m_buffer_read_success && (current_entry.data_offset < memory_buffer.size()) &&
                              (current_entry.data_offset + current_entry.data_size <= memory_buffer.size());
        if (current_entry.type_size > 0 && current_entry.data_size == 0) {
            current_entry.valid = false;
        }
        if (!m_buffer_read_success && current_entry.offset < selected_class->size) {
            current_entry.valid = false;
        }

        if (current_entry.valid) {
            current_entry.dereferenced_string = dereference_as_string(get_entry_data_span(current_entry));
        } else {
            current_entry.dereferenced_string = std::nullopt;
        }

        rebuild_entries_from_index(entry_idx + 1);
    }

    void memory_view::rebuild_entries_from_index(std::size_t start_idx) {
        if (!selected_idx.has_value() || start_idx > entries.size())
            return;

        const auto& selected_class = classes[*selected_idx];

        std::size_t class_processed_bytes = 0;
        std::uintptr_t current_addr_base = selected_class->addr;

        if (start_idx > 0 && start_idx <= entries.size()) {
            class_processed_bytes = std::accumulate(
                    entries.begin(), entries.begin() + static_cast<ptrdiff_t>(start_idx), 0ULL,
                    [](std::size_t sum, const memory_entry& e) {
                        return sum + e.type_size;
                    }
            );
        } else if (start_idx > entries.size()) {
            return;
        }

        entries.erase(entries.begin() + static_cast<ptrdiff_t>(start_idx), entries.end());

        const auto default_type =
                (start_idx > 0 && !entries.empty()) ? entries[start_idx - 1].type : memory_type::int32;

        while (class_processed_bytes < selected_class->size) {
            memory_entry entry;
            entry.offset = class_processed_bytes;
            entry.addr = current_addr_base + class_processed_bytes;
            entry.type = default_type;
            entry.type_size = get_type_size(entry.type);

            entry.data_offset = class_processed_bytes;

            std::size_t remaining_in_class_logical = selected_class->size - class_processed_bytes;
            std::size_t actual_buffer_available = (m_buffer_read_success && entry.data_offset < memory_buffer.size())
                                                          ? (memory_buffer.size() - entry.data_offset)
                                                          : 0;

            entry.data_size = std::min({entry.type_size, remaining_in_class_logical, actual_buffer_available});

            entry.valid = m_buffer_read_success && (entry.data_offset < memory_buffer.size()) &&
                          (entry.data_offset + entry.data_size <= memory_buffer.size());
            if (entry.type_size > 0 && entry.data_size == 0) {
                entry.valid = false;
            }
            if (!m_buffer_read_success && class_processed_bytes < selected_class->size) {
                entry.valid = false;
            }

            if (entry.data_size == 0 && class_processed_bytes < selected_class->size && entry.type_size > 0) {
                break;
            }

            if (entry.valid) {
                entry.dereferenced_string = dereference_as_string(get_entry_data_span(entry));
            } else {
                entry.dereferenced_string = std::nullopt;
            }

            entries.emplace_back(std::move(entry));

            class_processed_bytes += entry.type_size;
            if (entry.type_size == 0)
                break;
        }
    }

    bool memory_view::is_valid_addr(std::uintptr_t addr) {
        return addr != 0;
    }
} // namespace ui
