#include <app/ctx.h>
#include <format>
#include <imgui.h>
#include <ui/theme.h>
#include <ui/views/strings.h>

namespace ui {

    strings_view::strings_view() : view("Strings") {
    }

    void strings_view::render() {
        if (!app::active_target) {
            ImGui::TextDisabled("No target loaded.");
            return;
        }

        auto& analyzer = *app::strings;

        if (ImGui::Button("Scan")) {
            analyzer.scan(app::active_target.get(), config);
            needs_refresh = true;
        }

        ImGui::SameLine();
        ImGui::Checkbox("Scan Executable", &config.scan_executable);

        ImGui::SameLine();
        ImGui::SetNextItemWidth(100);
        int min_len = static_cast<int>(config.min_length);
        if (ImGui::InputInt("Min Len", &min_len)) {
            if (min_len < 1)
                min_len = 1;
            config.min_length = static_cast<size_t>(min_len);
        }

        if (analyzer.is_scanning()) {
            ImGui::SameLine();
            ImGui::TextColored(theme::colors::yellow, "Scanning... %.0f%%", analyzer.progress() * 100.0f);

            if (ImGui::Button("Cancel")) {
                analyzer.cancel();
            }
        } else {
            if (needs_refresh && analyzer.count() > 0) {
                refresh();
                needs_refresh = false;
            }
        }

        ImGui::Separator();

        ImGui::Text("Total: %zu", cache.size());
        ImGui::SameLine();
        ImGui::InputText("Filter (Not implemented)", filter_buf, sizeof(filter_buf));

        if (ImGui::BeginTable(
                    "StringsTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY
            )) {
            ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("Length", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("String", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            ImGuiListClipper clipper;
            clipper.Begin(static_cast<int>(cache.size()));

            while (clipper.Step()) {
                for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                    const auto& ref = cache[i];

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("0x%llX", static_cast<unsigned long long>(ref.address));

                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("%u", ref.length);

                    ImGui::TableSetColumnIndex(2);
                    std::string text = analyzer.read_string(app::active_target.get(), ref);

                    if (text.size() > 64) {
                        text = text.substr(0, 61) + "...";
                    }

                    ImGui::TextUnformatted(text.c_str());
                }
            }

            ImGui::EndTable();
        }
    }

    void strings_view::refresh() {
        if (app::strings) {
            cache = app::strings->get_results_copy();
        }
    }

} // namespace ui
