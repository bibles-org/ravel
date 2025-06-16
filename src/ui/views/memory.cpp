#include <algorithm>
#include <app/ctx.h>
#include <cstring>
#include <ranges>
#include <ui/views/memory.h>

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
            ImGui::SetColumnWidth(0, 250.0f);
            first_time = false;
        }

        render_sidebar();
        ImGui::NextColumn();
        render_memory_view();
        ImGui::Columns(1);

        render_add_popup();
    }

    void memory_view::render_sidebar() {
        ImGui::Text("Classes");
        ImGui::Separator();

        if (ImGui::Button("Add Class", ImVec2(-1, 0))) {
            show_add_popup = true;
        }

        ImGui::Spacing();
        render_class_list();
    }

    void memory_view::render_class_list() {
        for (std::size_t i = 0; i < classes.size(); ++i) {
            const auto& cls = classes[i];

            ImGui::PushID(static_cast<int>(i));

            bool is_selected = (selected_idx == i);
            if (ImGui::Selectable(cls->name.c_str(), is_selected)) {
                selected_idx = i;
                refresh_data();
            }

            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Delete")) {
                    remove_class(i);
                    ImGui::EndPopup();
                    ImGui::PopID();
                    break;
                }
                if (ImGui::MenuItem("Rename")) {
                    // todo: implement
                }
                ImGui::Separator();

                static const std::vector<std::size_t> sizes = {256, 512, 1024, 2048, 4096};
                for (auto sz : sizes) {
                    char label[32];
                    snprintf(label, sizeof(label), "Show %zu bytes", sz);
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
                ImGui::Text("  0x%016lX", cls->addr);
            }

            ImGui::PopID();
        }
    }

    void memory_view::render_add_popup() {
        if (show_add_popup) {
            ImGui::OpenPopup("Add Class");
        }

        if (ImGui::BeginPopupModal("Add Class", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Enter class name:");
            ImGui::InputText("##class_name", new_class_name, sizeof(new_class_name));

            ImGui::Spacing();

            if (ImGui::Button("Add", ImVec2(120, 0))) {
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
            ImGui::Text("Select a class to view memory");
            return;
        }

        auto& selected_class = classes[*selected_idx];

        ImGui::Text("Address:");
        ImGui::SameLine();
        ImGui::PushItemWidth(200);

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

        ImGui::PopItemWidth();

        ImGui::SameLine();
        if (ImGui::Button("Refresh")) {
            refresh_data();
        }

        ImGui::SameLine();
        ImGui::Checkbox("Auto-refresh", &auto_refresh);

        if (auto_refresh) {
            ImGui::SameLine();
            ImGui::PushItemWidth(80);
            ImGui::SliderFloat("##refresh_rate", &refresh_rate, 0.1f, 5.0f, "%.1fs");
            ImGui::PopItemWidth();

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

    void memory_view::render_memory_table() {
        if (!app::proc || !app::proc->is_attached()) {
            ImGui::Text("No process attached");
            return;
        }

        if (entries.empty()) {
            ImGui::Text("No memory data available");
            return;
        }

        if (ImGui::BeginTable(
                    "MemoryTable", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY
            )) {

            ImGui::TableSetupColumn("Offset", ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthFixed, 120.0f);
            ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableSetupColumn("Hex", ImGuiTableColumnFlags_WidthFixed, 140.0f);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("String ->", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            for (std::size_t i = 0; i < entries.size(); ++i) {
                const auto& entry = entries[i];

                ImGui::TableNextRow();
                ImGui::PushID(static_cast<int>(i));

                ImGui::TableSetColumnIndex(0);
                ImGui::Text("0x%lX", entry.offset);

                ImGui::TableSetColumnIndex(1);
                ImGui::Text("0x%lX", static_cast<unsigned long>(entry.addr));

                ImGui::TableSetColumnIndex(2);
                ImGui::Selectable(
                        get_type_name(entry.type), false,
                        ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap
                );

                char popup_id[32];
                snprintf(popup_id, sizeof(popup_id), "TypeMenu_%zu", i);
                if (ImGui::BeginPopupContextItem(popup_id)) {
                    ImGui::Text("Change Type:");
                    ImGui::Separator();

                    static const std::vector<std::pair<memory_type, const char*>> type_options = {
                            {memory_type::int8,    "Int8"   },
                            {memory_type::uint8,   "UInt8"  },
                            {memory_type::int16,   "Int16"  },
                            {memory_type::uint16,  "UInt16" },
                            {memory_type::int32,   "Int32"  },
                            {memory_type::uint32,  "UInt32" },
                            {memory_type::int64,   "Int64"  },
                            {memory_type::uint64,  "UInt64" },
                            {memory_type::float32, "Float"  },
                            {memory_type::float64, "Double" },
                            {memory_type::pointer, "Pointer"},
                            {memory_type::text,    "Text"   },
                            {memory_type::bytes,   "Bytes"  }
                    };

                    for (const auto& [type, name] : type_options) {
                        if (ImGui::MenuItem(name, nullptr, entry.type == type)) {
                            change_entry_type(i, type);
                        }
                    }

                    ImGui::EndPopup();
                }

                ImGui::TableSetColumnIndex(3);
                if (entry.valid) {
                    ImGui::Text("%s", format_hex(entry.data).c_str());
                } else {
                    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "??");
                }

                ImGui::TableSetColumnIndex(4);
                if (entry.valid) {
                    ImGui::Text("%s", format_typed_value(entry).c_str());
                } else {
                    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "????");
                }

                ImGui::TableSetColumnIndex(5);
                if (entry.valid && entry.dereferenced_string) {
                    ImGui::Text("\"%s\"", entry.dereferenced_string->c_str());
                }

                ImGui::PopID();
            }

            ImGui::EndTable();
        }
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

            // dont read beyond the class size
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

        // strip dereference brackets
        bool do_deref = input.starts_with('[') && input.ends_with(']');
        if (do_deref) {
            input.remove_prefix(1);
            input.remove_suffix(1);
        }

        if (input.starts_with("0x") || input.starts_with("0X")) {
            input.remove_prefix(2);
        }

        // get hex digits
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

    std::string memory_view::format_ascii(std::span<const std::byte> data) {
        std::string result;
        result.reserve(data.size());

        for (const auto& byte : data) {
            unsigned char c = static_cast<unsigned char>(byte);
            result += (c >= 32 && c <= 126) ? static_cast<char>(c) : '.';
        }
        return result;
    }

    std::string memory_view::format_typed_value(const memory_entry& entry) {
        if (!entry.valid || entry.data.empty()) {
            return "????";
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

        return "????";
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
                return 16; // default size for text/bytes
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

        // reread memory with the new size
        auto result = read_memory(entries[entry_idx].addr, entries[entry_idx].type_size);
        if (result.has_value()) {
            entries[entry_idx].data = std::move(result.value());
            entries[entry_idx].valid = true;
            entries[entry_idx].dereferenced_string = dereference_as_string(entries[entry_idx].data);
        } else {
            entries[entry_idx].data.resize(entries[entry_idx].type_size);
            entries[entry_idx].valid = false;
        }

        // rebuild subsequent entries since
        // the memory layout may have changed
        rebuild_entries_from_index(entry_idx + 1);
    }

    void memory_view::rebuild_entries_from_index(std::size_t start_idx) {
        if (!selected_idx.has_value() || start_idx >= entries.size()) {
            return;
        }

        const auto& selected_class = classes[*selected_idx];

        std::size_t bytes_processed = 0;
        for (std::size_t i = 0; i < start_idx; ++i) {
            bytes_processed += entries[i].type_size;
        }

        // remove entries that are now invalid
        entries.erase(entries.begin() + static_cast<ptrdiff_t>(start_idx), entries.end());

        std::uintptr_t current_addr = selected_class->addr + bytes_processed;

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

    bool memory_view::is_valid_addr(std::uintptr_t addr) {
        // todo: proper address validation from pantsir
        return addr != 0;
    }
} // namespace ui
