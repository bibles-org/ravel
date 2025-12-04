#include <ui/views/file_info.h>

#include <app/ctx.h>
#include <core/file_target.h>
#include <format>
#include <imgui.h>

namespace ui {
    file_info_view::file_info_view() : view("File Info") {
    }

    void file_info_view::render() {
        if (!app::active_target || app::active_target->is_live()) {
            ImGui::TextDisabled("No file loaded. Open a file from the File menu.");
            return;
        }

        auto* file_target = static_cast<core::file_target*>(app::active_target.get());
        const auto* parser = file_target->get_parser();

        if (!parser)
            return;

        ImGui::Text("File: %s", parser->get_path().string().c_str());
        ImGui::Separator();

        if (ImGui::BeginTable("file_info_table", 2, ImGuiTableFlags_Borders)) {
            ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 120.0f);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("Format");
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%s", parser->get_type_name().c_str());

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("Architecture");
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%s", parser->get_arch_name().c_str());

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("Entry Point");
            ImGui::TableSetColumnIndex(1);
            if (auto entry = parser->get_entry_point()) {
                ImGui::Text("0x%lX", *entry);
            } else {
                ImGui::Text("N/A");
            }

            ImGui::EndTable();
        }
    }
} // namespace ui
