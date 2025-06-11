#include <app/ctx.h>
#include <cstring>
#include <iomanip>
#include <sstream>
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
        if (!selected_idx.has_value()) {
            ImGui::Text("Select a class to view memory");
            return;
        }

        auto& selected_class = classes[*selected_idx];

        ImGui::Text("Address:");
        ImGui::SameLine();
        ImGui::PushItemWidth(200);
        if (ImGui::InputText("##address", addr_input, sizeof(addr_input), ImGuiInputTextFlags_CharsHexadecimal)) {
            std::uintptr_t addr = 0;
            if (std::sscanf(addr_input, "%lx", &addr) == 1) {
                selected_class->addr = addr;
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
            float current_time = ImGui::GetTime();
            if (current_time - last_refresh >= refresh_rate) {
                refresh_data();
                last_refresh = current_time;
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
                ImGui::Text("0x%016lX", entry.addr);

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
        std::stringstream ss;
        for (std::size_t i = 0; i < data.size(); ++i) {
            if (i > 0)
                ss << " ";
            ss << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << static_cast<unsigned int>(data[i]);
        }
        return ss.str();
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
