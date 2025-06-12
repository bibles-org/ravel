#include <algorithm>
#include <app/ctx.h>
#include <cstring>
#include <ranges>
#include <ui/views/memory.h>

namespace ui {
    memory_view::memory_view() :
        view("Memory"), entries_per_page(64), current_page(0), show_add_popup(false), auto_refresh(false),
        refresh_rate(1.0f) {

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

        // todo: fix clang-format buddy...
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
            float now = ImGui::GetTime();
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
                    "MemoryTable", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY
            )) {

            ImGui::TableSetupColumn("Offset", ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthFixed, 120.0f);
            ImGui::TableSetupColumn("ASCII", ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableSetupColumn("Hex", ImGuiTableColumnFlags_WidthFixed, 120.0f);
            ImGui::TableSetupColumn("Int32 / Float", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            for (const auto& entry : entries) {
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::Text("0x%lX", entry.offset);

                ImGui::TableSetColumnIndex(1);
                ImGui::Text("0x%lX", static_cast<unsigned long>(entry.addr));

                ImGui::TableSetColumnIndex(2);
                if (entry.valid) {
                    ImGui::Text("%s", format_ascii(entry.data).c_str());
                } else {
                    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "????");
                }

                ImGui::TableSetColumnIndex(3);
                if (entry.valid) {
                    ImGui::Text("%s", format_hex(entry.data).c_str());
                } else {
                    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "?? ?? ?? ??");
                }

                ImGui::TableSetColumnIndex(4);
                if (entry.valid) {
                    std::int32_t int_val = bytes_to<std::int32_t>(entry.data);
                    float float_val = bytes_to<float>(entry.data);
                    ImGui::Text("%d / %.6f", int_val, float_val);
                } else {
                    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "???? / ????");
                }
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

        classes.erase(classes.begin() + index);

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

        std::size_t total_entries = selected_class->size / 4;
        entries.reserve(total_entries);

        for (std::size_t i = 0; i < total_entries; ++i) {
            memory_entry entry;
            entry.offset = i * 4;
            entry.addr = selected_class->addr + entry.offset;

            auto result = read_memory(entry.addr);
            if (result.has_value()) {
                entry.data = result.value();
                entry.valid = true;
            } else {
                entry.valid = false;
            }

            entries.emplace_back(std::move(entry));
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

        std::array<std::byte, sizeof(std::uintptr_t)> buffer;
        auto result = app::proc->read_memory(ptr_addr, std::span<std::byte>(buffer));

        if (!result.has_value()) {
            return std::unexpected(false);
        }

        std::uintptr_t target_addr = 0;
        std::memcpy(&target_addr, buffer.data(), sizeof(std::uintptr_t));

        if (target_addr == 0) {
            return std::unexpected(false);
        }

        return target_addr;
    }

    std::expected<std::array<std::byte, 4>, bool> memory_view::read_memory(std::uintptr_t addr) {
        if (!app::proc || !app::proc->is_attached()) {
            return std::unexpected(false);
        }

        std::array<std::byte, 4> buffer;
        auto result = app::proc->read_memory(addr, std::span<std::byte>(buffer));

        if (result.has_value()) {
            return buffer;
        }
        return std::unexpected(false);
    }

    std::string memory_view::format_hex(std::span<const std::byte, 4> data) {
        return std::ranges::fold_left(
                data | std::ranges::views::transform([](std::byte b) {
                    return std::format("{:02X}", std::to_integer<unsigned int>(b));
                }),
                std::string{},
                [](std::string acc, std::string val) {
                    if (!acc.empty())
                        acc += ' ';
                    acc += std::move(val);
                    return acc;
                }
        );
    }

    std::string memory_view::format_ascii(std::span<const std::byte, 4> data) {
        std::string result;
        result.reserve(4);

        for (const auto& byte : data) {
            unsigned char c = static_cast<unsigned char>(byte);
            result += (c >= 32 && c <= 126) ? static_cast<char>(c) : '.';
        }
        return result;
    }

    template <typename T>
    T memory_view::bytes_to(std::span<const std::byte, 4> data)
        requires(sizeof(T) == 4 && std::is_trivially_copyable_v<T> && std::is_standard_layout_v<T>)
    {
        T value;
        std::memcpy(&value, data.data(), sizeof(T));
        return value;
    }

    bool memory_view::is_valid_addr(std::uintptr_t addr) {
        // todo: proper address validation from pantsir
        return addr != 0;
    }
} // namespace ui
