#include <ui/views/strings.h>

#include <app/ctx.h>
#include <format>
#include <imgui.h>
#include <ui/theme.h>

namespace ui {

    strings_view::strings_view() : view("Strings") {
        batch_buffer.reserve(128);
    }

    void strings_view::render() {
        if (!app::active_target) {
            ImGui::TextDisabled("No target loaded.");
            return;
        }

        auto& analyzer = *app::strings;

        if (ImGui::Button("Scan")) {
            analyzer.scan(app::active_target.get(), config);
        }

        ImGui::SameLine();
        ImGui::Checkbox("Exec", &config.scan_executable);

        ImGui::SameLine();
        ImGui::SetNextItemWidth(80);
        int min_len = static_cast<int>(config.min_length);
        if (ImGui::InputInt("Min Len", &min_len)) {
            config.min_length = static_cast<std::size_t>(std::max(1, min_len));
        }

        if (analyzer.is_scanning()) {
            ImGui::SameLine();
            ImGui::TextColored(theme::colors::yellow, "Scanning... %.0f%%", analyzer.progress() * 100.0f);
            ImGui::SameLine();
            if (ImGui::Button("Stop")) {
                analyzer.cancel();
            }
        }

        total_count = analyzer.count();

        ImGui::Separator();

        ImGui::Text("Total Found: %zu", total_count);

        const ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
                                      ImGuiTableFlags_SizingFixedFit;

        if (ImGui::BeginTable("StringsTable", 3, flags)) {
            ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthFixed, 120.0f);
            ImGui::TableSetupColumn("Len", ImGuiTableColumnFlags_WidthFixed, 60.0f);
            ImGui::TableSetupColumn("String", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            ImGuiListClipper clipper;
            clipper.Begin(static_cast<int>(total_count));

            while (clipper.Step()) {
                std::size_t start = static_cast<std::size_t>(clipper.DisplayStart);
                std::size_t count = static_cast<std::size_t>(clipper.DisplayEnd - clipper.DisplayStart);

                if (batch_buffer.size() < count) {
                    batch_buffer.resize(count);
                }

                std::size_t fetched = analyzer.get_batch(start, std::span(batch_buffer.data(), count));

                for (std::size_t i = 0; i < fetched; ++i) {
                    const auto& ref = batch_buffer[i];

                    ImGui::TableNextRow();

                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("0x%llX", static_cast<unsigned long long>(ref.address));

                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("%u", ref.length);

                    ImGui::TableSetColumnIndex(2);
                    std::string text = analyzer.read_string(app::active_target.get(), ref);
                    if (text.size() > 100)
                        text = text.substr(0, 97) + "...";
                    ImGui::TextUnformatted(text.c_str());
                }

                if (fetched < count) {
                    for (std::size_t k = fetched; k < count; ++k) {
                        ImGui::TableNextRow();
                    }
                }
            }

            ImGui::EndTable();
        }
    }

} // namespace ui
