#include <app/ctx.h>
#include <format>
#include <ui/views/processes.h>

namespace ui {
    processes_view::processes_view() : view("Processes") {
    }

    void processes_view::render() {
        if (ImGui::Button("Refresh List")) {
            auto result = app::proc->enumerate_processes();
            if (result) {
                m_processes = std::move(result.value());
                m_selected_index.reset();
            } else {
                m_processes.clear();
            }
        }

        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();

        attach_action();

        ImGui::Separator();

        ImGui::Text("Filter:");
        ImGui::SameLine();
        m_filter.Draw("##process_filter", ImGui::GetContentRegionAvail().x);

        ImGui::Separator();

        process_table(m_filter);
    }

    void processes_view::process_table(const ImGuiTextFilter& filter) {
        const ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
                                      ImGuiTableFlags_Sortable | ImGuiTableFlags_ScrollY;

        if (ImGui::BeginTable("processes_table", 3, flags)) {
            ImGui::TableSetupColumn("PID", ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Path", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableHeadersRow();

            for (std::size_t i = 0; i < m_processes.size(); ++i) {
                const auto& process = m_processes[i];

                if (!filter.PassFilter(process.name.c_str()) 
                    && !filter.PassFilter(process.executable_path.string().c_str())) {
                    continue;
                }

                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                const bool is_selected = m_selected_index.has_value() && m_selected_index.value() == i;
                if (ImGui::Selectable(
                            std::format("{}", process.pid).c_str(), is_selected, ImGuiSelectableFlags_SpanAllColumns
                    )) {
                    m_selected_index = i;
                }

                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(process.name.c_str());

                ImGui::TableSetColumnIndex(2);
                ImGui::TextUnformatted(
                        process.executable_path.string().c_str()
                );
            }

            ImGui::EndTable();
        }
    }

    void processes_view::attach_action() {
        const bool can_attach = m_selected_index.has_value();
        if (!can_attach) {
            ImGui::BeginDisabled();
        }

        if (ImGui::Button("Attach")) {
            const auto& selected_process = m_processes[*m_selected_index];
            auto result = app::proc->attach_to(selected_process.pid);
            if (!result) {
            }
        }

        if (!can_attach) {
            ImGui::EndDisabled();
        }

        ImGui::SameLine();

        if (app::proc->is_attached()) {
            ImGui::TextColored({0.3f, 1.0f, 0.3f, 1.0f}, "Status: Attached");
        } else {
            ImGui::Text("Status: Not Attached");
        }
    }
} // namespace ui
