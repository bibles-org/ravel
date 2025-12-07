#include "diff_view.h"
#include "../theme.h"

#include <imgui.h>

#include <algorithm>
#include <format>
#include <stdexcept>
#include <string>
#include <vector>

namespace ui {
    namespace {
        struct instruction_diff_line {
            enum class type {
                common,
                added,
                removed
            };
            type line_type;
            std::string text_primary;
            std::string text_secondary;
        };

        auto get_instruction_diffs(const std::vector<std::string>& seq1, const std::vector<std::string>& seq2)
                -> std::vector<instruction_diff_line> {
            const size_t m = seq1.size();
            const size_t n = seq2.size();
            std::vector<std::vector<size_t>> dp(m + 1, std::vector<size_t>(n + 1, 0));

            for (size_t i = 1; i <= m; ++i) {
                for (size_t j = 1; j <= n; ++j) {
                    if (seq1[i - 1] == seq2[j - 1]) {
                        dp[i][j] = dp[i - 1][j - 1] + 1;
                    } else {
                        dp[i][j] = std::max(dp[i - 1][j], dp[i][j - 1]);
                    }
                }
            }

            std::vector<instruction_diff_line> diff;
            size_t i = m, j = n;
            while (i > 0 || j > 0) {
                if (i > 0 && j > 0 && seq1[i - 1] == seq2[j - 1]) {
                    diff.push_back({instruction_diff_line::type::common, seq1[i - 1], seq2[j - 1]});
                    --i;
                    --j;
                } else if (j > 0 && (i == 0 || dp[i][j - 1] >= dp[i - 1][j])) {
                    diff.push_back({instruction_diff_line::type::added, "", seq2[j - 1]});
                    --j;
                } else if (i > 0 && (j == 0 || dp[i - 1][j] > dp[i][j - 1])) {
                    diff.push_back({instruction_diff_line::type::removed, seq1[i - 1], ""});
                    --i;
                }
            }
            std::reverse(diff.begin(), diff.end());
            return diff;
        }

    } // namespace

    diff_view::diff_view() : view("Binary Diff") {
    }

    void diff_view::render() {
        render_controls();

        ImGui::Separator();

        if (!error_message_.empty()) {
            ImGui::TextColored(theme::colors::red, "Error: %s", error_message_.c_str());
        }

        if (!diff_result_) {
            ImGui::TextDisabled("Select two files and press 'Compare' to see the difference.");
            return;
        }

        if (subroutine_list_.empty()) {
            ImGui::Text("No subroutines found or matched in the provided binaries.");
            return;
        }

        ImGui::BeginChild("SubroutineList", ImVec2(ImGui::GetContentRegionAvail().x * 0.3f, 0), true);
        render_subroutine_list();
        ImGui::EndChild();

        ImGui::SameLine();

        ImGui::BeginChild("DiffPane", ImVec2(0, 0), true);
        render_diff_pane();
        ImGui::EndChild();
    }

    void diff_view::render_controls() {
        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.5f - 100.0f);
        ImGui::InputTextWithHint(
                "##PrimaryPath", "Path to primary binary...", primary_path_buf_, sizeof(primary_path_buf_)
        );
        ImGui::SameLine();
        ImGui::InputTextWithHint(
                "##SecondaryPath", "Path to secondary binary...", secondary_path_buf_, sizeof(secondary_path_buf_)
        );
        ImGui::PopItemWidth();

        ImGui::SameLine();
        if (ImGui::Button("Compare", ImVec2(180.0f, 0))) {
            process_comparison();
        }
    }

    void diff_view::render_subroutine_list() {
        ImGui::Text("Subroutines");
        ImGui::Separator();

        for (int i = 0; i < static_cast<int>(subroutine_list_.size()); ++i) {
            const auto& item = subroutine_list_[i];
            std::string label;
            ImVec4 color = theme::colors::text;

            switch (item.item_type) {
                case subroutine_display_item::type::matched:
                    label = std::format(
                            "0x{:X} <-> 0x{:X} ({:.1f}%%)", item.primary_address, item.secondary_address,
                            item.similarity_score * 100
                    );
                    color = theme::colors::green;
                    break;
                case subroutine_display_item::type::primary_only:
                    label = std::format("(-) 0x{:X}", item.primary_address);
                    color = theme::colors::red;
                    break;
                case subroutine_display_item::type::secondary_only:
                    label = std::format("(+) 0x{:X}", item.secondary_address);
                    color = theme::colors::yellow;
                    break;
            }

            ImGui::PushStyleColor(ImGuiCol_Text, color);
            if (ImGui::Selectable(label.c_str(), selected_subroutine_index_ == i)) {
                selected_subroutine_index_ = i;
            }
            ImGui::PopStyleColor();
        }
    }

    void diff_view::render_diff_pane() {
        if (selected_subroutine_index_ < 0 || selected_subroutine_index_ >= static_cast<int>(subroutine_list_.size())) {
            ImGui::TextDisabled("Select a subroutine from the list to view details.");
            return;
        }

        const auto& item = subroutine_list_[selected_subroutine_index_];
        std::vector<std::string> primary_instructions;
        std::vector<std::string> secondary_instructions;

        switch (item.item_type) {
            case subroutine_display_item::type::matched: {
                const auto& match = diff_result_->matched_subroutines[item.original_index];
                for (const auto& bb : match.first.basic_blocks) {
                    primary_instructions.insert(
                            primary_instructions.end(), bb.instructions.begin(), bb.instructions.end()
                    );
                }
                for (const auto& bb : match.second.basic_blocks) {
                    secondary_instructions.insert(
                            secondary_instructions.end(), bb.instructions.begin(), bb.instructions.end()
                    );
                }
                break;
            }
            case subroutine_display_item::type::primary_only: {
                const auto& sub = diff_result_->unmatched_primary[item.original_index];
                for (const auto& bb : sub.basic_blocks) {
                    primary_instructions.insert(
                            primary_instructions.end(), bb.instructions.begin(), bb.instructions.end()
                    );
                }
                break;
            }
            case subroutine_display_item::type::secondary_only: {
                const auto& sub = diff_result_->unmatched_secondary[item.original_index];
                for (const auto& bb : sub.basic_blocks) {
                    secondary_instructions.insert(
                            secondary_instructions.end(), bb.instructions.begin(), bb.instructions.end()
                    );
                }
                break;
            }
        }

        auto diff_lines = get_instruction_diffs(primary_instructions, secondary_instructions);

        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(diff_lines.size()));
        while (clipper.Step()) {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                const auto& line = diff_lines[static_cast<size_t>(i)];

                ImVec4 bg_color = theme::colors::transparent;
                std::string text_line;

                switch (line.line_type) {
                    case instruction_diff_line::type::removed:
                        bg_color = theme::with_alpha(theme::colors::red, 0.2f);
                        text_line = std::format("- {}", line.text_primary);
                        break;
                    case instruction_diff_line::type::added:
                        bg_color = theme::with_alpha(theme::colors::green, 0.2f);
                        text_line = std::format("+ {}", line.text_secondary);
                        break;
                    case instruction_diff_line::type::common:
                    default:
                        text_line = std::format("  {}", line.text_primary);
                        break;
                }

                ImVec2 p_min = ImGui::GetCursorScreenPos();
                ImVec2 p_max = ImVec2(p_min.x + ImGui::GetContentRegionAvail().x, p_min.y + ImGui::GetTextLineHeight());

                if (bg_color.w > 0.0f) {
                    ImGui::GetWindowDrawList()->AddRectFilled(p_min, p_max, ImGui::ColorConvertFloat4ToU32(bg_color));
                }

                ImGui::TextUnformatted(text_line.c_str());
            }
        }
        clipper.End();
    }

    void diff_view::process_comparison() {
        diff_result_.reset();
        subroutine_list_.clear();
        selected_subroutine_index_ = -1;
        error_message_.clear();

        std::string p_path_str = primary_path_buf_;
        std::string s_path_str = secondary_path_buf_;

        if (p_path_str.empty() || s_path_str.empty()) {
            error_message_ = "Both file paths must be provided.";
            return;
        }

        try {
            binary_differ differ(p_path_str, s_path_str);
            diff_result_ = differ.compare();
            build_display_list();
        } catch (const std::runtime_error& e) {
            error_message_ = e.what();
        } catch (...) {
            error_message_ = "An unknown error occurred during comparison.";
        }
    }

    void diff_view::build_display_list() {
        subroutine_list_.clear();
        if (!diff_result_)
            return;

        for (size_t i = 0; i < diff_result_->matched_subroutines.size(); ++i) {
            const auto& match = diff_result_->matched_subroutines[i];
            subroutine_list_.push_back({
                    subroutine_display_item::type::matched,
                    i,
                    match.first.start_address,
                    match.second.start_address,
                    match.first.similarity_score,
            });
        }
        for (size_t i = 0; i < diff_result_->unmatched_primary.size(); ++i) {
            const auto& sub = diff_result_->unmatched_primary[i];
            subroutine_list_.push_back({subroutine_display_item::type::primary_only, i, sub.start_address, 0, 0.0});
        }
        for (size_t i = 0; i < diff_result_->unmatched_secondary.size(); ++i) {
            const auto& sub = diff_result_->unmatched_secondary[i];
            subroutine_list_.push_back({subroutine_display_item::type::secondary_only, i, 0, sub.start_address, 0.0});
        }

        std::sort(subroutine_list_.begin(), subroutine_list_.end(), [](const auto& a, const auto& b) {
            uint64_t addr_a = (a.primary_address != 0) ? a.primary_address : a.secondary_address;
            uint64_t addr_b = (b.primary_address != 0) ? b.primary_address : b.secondary_address;
            return addr_a < addr_b;
        });
    }

} // namespace ui
