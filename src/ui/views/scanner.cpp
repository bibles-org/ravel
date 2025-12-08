#include "core/scanner/scanner.h"
#include <app/ctx.h>
#include <imgui.h>
#include <ui/views/scanner.h>

namespace ui {
    const char* type_names[] = {"u8", "i8", "u16", "i16", "u32", "i32", "u64", "i64", "f32", "f64"};
    const char* cmp_names[] = {"Exact", "Greater", "Less"};

    scanner_view::scanner_view() : view("Scanner"), engine(nullptr) {
    }

    void scanner_view::render() {
        if (app::active_target.get() != last_target) {
            last_target = app::active_target.get();
            engine.set_target(last_target);
            engine.reset();
            is_first_scan = true;
        }

        if (!last_target) {
            ImGui::TextDisabled("No active target.");
            return;
        }

        if (ImGui::BeginChild("Configuration", ImVec2(260.0f, 0), true)) {
            draw_config();
        }
        ImGui::EndChild();

        ImGui::SameLine();


        if (ImGui::BeginChild("Settings", ImVec2(0, 0), true)) {
            if (engine.is_scanning()) {
                draw_status();
                ImGui::Separator();
            }

            if (ImGui::BeginChild("ResultsList", ImVec2(0, 0))) {
                draw_results();
            }
            ImGui::EndChild();
        }
        ImGui::EndChild();
    }

    void scanner_view::draw_config() {
        ImGui::Text("Scan Configuration");
        ImGui::Separator();

        ImGui::InputText("Value", val_buf, sizeof(val_buf));
        ImGui::Combo("Type", &selected_type_idx, type_names, IM_ARRAYSIZE(type_names));
        ImGui::Combo("Mode", &selected_cmp_idx, cmp_names, IM_ARRAYSIZE(cmp_names));

        ImGui::Checkbox("Fast Scan (Aligned)", &config.fast_scan);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        config.data_type = static_cast<core::scan_data_type>(selected_type_idx);
        config.compare_type = static_cast<core::scan_compare_type>(selected_cmp_idx);
        config.value_str = val_buf;

        if (engine.is_scanning()) {
            if (ImGui::Button("Cancel Scan", ImVec2(-1, 30))) {
                engine.cancel();
            }
        } else {
            if (is_first_scan) {
                if (ImGui::Button("First Scan", ImVec2(-1, 30))) {
                    engine.set_target(app::active_target.get());
                    engine.begin_first_scan(config);
                    is_first_scan = false;
                }
            } else {
                if (ImGui::Button("Next Scan", ImVec2(-1, 30))) {
                    engine.begin_next_scan(config);
                }
                ImGui::Spacing();
                if (ImGui::Button("Reset / New Scan", ImVec2(-1, 0))) {
                    engine.reset();
                    is_first_scan = true;
                }
            }
        }

        ImGui::Spacing();
        ImGui::TextDisabled("Found: %zu", engine.result_count());
    }

    void scanner_view::draw_status() {
        float p = engine.progress();
        ImGui::ProgressBar(p, ImVec2(-1, 20), std::format("{:.1f}%", p * 100).c_str());
    }

    void scanner_view::draw_results() {
        const auto& results = engine.get_results();
        if (results.empty()) {
            ImGui::TextDisabled("No results.");
            return;
        }

        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(results.size()));

        const ImGuiTableFlags flags =
                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY;

        if (ImGui::BeginTable("ResTable", 2, flags)) {
            ImGui::TableSetupColumn("Address");
            ImGui::TableSetupColumn("Value");
            ImGui::TableHeadersRow();

            while (clipper.Step()) {
                for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                    const auto& res = results[i];
                    ImGui::TableNextRow();

                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("0x%llX", static_cast<unsigned long long>(res.address));

                    ImGui::TableSetColumnIndex(1);
                    std::vector<std::byte> buf(core::scanner::type_size(config.data_type));
                    if (app::active_target->read_memory(res.address, buf)) {
                        std::string val_str = core::scanner::format_value(buf, config.data_type);
                        ImGui::Text("%s", val_str.c_str());
                    } else {
                        ImGui::TextDisabled("??");
                    }
                }
            }

            ImGui::EndTable();
        }

        clipper.End();
    }
} // namespace ui
