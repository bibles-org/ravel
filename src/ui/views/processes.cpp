#include <ui/views/processes.h>

#include <app/ctx.h>
#include <core/process.h>
#include <format>
#include <ui/theme.h>

namespace ui {
    processes_view::processes_view() : view("Processes") {
    }

    void processes_view::render() {
        auto* live_target = dynamic_cast<core::process*>(app::active_target.get());
        if (!live_target) {
            ImGui::TextDisabled("Process features are disabled when a file is open.");
            return;
        }

        if (ImGui::Button("Refresh")) {
            auto result = live_target->enumerate_processes();
            if (result) {
                m_processes = std::move(result.value());
                m_selected_index.reset();
            } else {
                m_processes.clear();
            }
        }

        ImGui::SameLine();

        const bool can_attach = m_selected_index.has_value();
        if (!can_attach) {
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
        }
        if (ImGui::Button("Attach") && can_attach) {
            const auto& selected_process = m_processes[*m_selected_index];
            auto result = live_target->attach_to(selected_process.pid);
            if (!result) {
                // todo: log or show error popup
            }
        }
        if (!can_attach) {
            ImGui::PopStyleVar();
        }

        ImGui::SameLine();
        ImGui::Text("Filter:");
        ImGui::SameLine();
        m_filter.Draw("##process_filter", ImGui::GetContentRegionAvail().x);

        ImGui::Separator();

        process_table(*live_target, m_filter);
    }

    void processes_view::process_table(core::process& target, const ImGuiTextFilter& filter) {
        const ImGuiTableFlags flags =
                ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable | ImGuiTableFlags_ScrollY;

        if (ImGui::BeginTable("processes_table", 2, flags)) {
            ImGui::TableSetupColumn("PID", ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableHeadersRow();

            for (std::size_t i = 0; i < m_processes.size(); ++i) {
                const auto& process = m_processes[i];

                if (!filter.PassFilter(process.name.c_str()) &&
                    !filter.PassFilter(process.executable_path.string().c_str())) {
                    continue;
                }

                ImGui::TableNextRow();

                const bool is_selected = m_selected_index.has_value() && m_selected_index.value() == i;

                ImGui::TableSetColumnIndex(0);
                if (ImGui::Selectable(
                            std::format("{}", process.pid).c_str(), is_selected,
                            ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap
                    )) {
                    m_selected_index = i;
                }
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                    if (target.attach_to(process.pid)) {
                        m_selected_index = i;
                    }
                }

                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(process.name.c_str());
            }

            ImGui::EndTable();
        }
    }
} // namespace ui
