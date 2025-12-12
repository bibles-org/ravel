#include <ui/views/disassembly.h>

#include <app/ctx.h>
#include <core/analysis/strings.h>
#include <format>
#include <ranges>
#include <ui/theme.h>

import zydis;

namespace ui {
    disassembly_view::disassembly_view() : view("Disassembly") {
        zydis::init(MACHINE_MODE_LONG_64, STACK_WIDTH_64, FORMATTER_STYLE_INTEL);
    }

    void disassembly_view::render() {
        if (app::active_target.get() != active_target) {
            update_target_regions();
            active_target = app::active_target.get();
        }

        if (!app::active_target) {
            ImGui::TextDisabled("No active target.");
            return;
        }

        render_region_selector();
        ImGui::Separator();
        render_listing();
    }

    void disassembly_view::render_region_selector() {
        if (ImGui::Button("Refresh Regions")) {
            update_target_regions();
        }
        ImGui::SameLine();

        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        std::string preview = "Select an executable region...";
        if (current_region_idx) {
            const auto& region = executable_regions[*current_region_idx];
            preview = std::format(
                    "{} (0x{:X} - 0x{:X})", region.name, region.base_address, region.base_address + region.size
            );
        }

        if (ImGui::BeginCombo("##region_combo", preview.c_str())) {
            for (std::size_t i = 0; i < executable_regions.size(); ++i) {
                const bool is_selected = current_region_idx && (*current_region_idx == i);
                const auto& region = executable_regions[i];
                std::string item_text = std::format(
                        "{} (0x{:X} - 0x{:X})", region.name, region.base_address, region.base_address + region.size
                );

                if (ImGui::Selectable(item_text.c_str(), is_selected)) {
                    if (!is_selected) {
                        load_region_data(i);
                    }
                }
                if (is_selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
    }

    void disassembly_view::render_listing() {
        if (executable_regions.empty()) {
            ImGui::TextDisabled("No executable regions found.");
            return;
        }
        if (!current_region_idx) {
            ImGui::TextDisabled("Select a region to view disassembly.");
            return;
        }
        if (region_data.empty()) {
            ImGui::TextColored(theme::colors::red, "Could not read memory for the selected region.");
            return;
        }

        if (ImGui::BeginChild("disassembly_listing", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar)) {
            ImGuiListClipper clipper;
            clipper.Begin(static_cast<int>(region_data.size()));

            while (clipper.Step()) {
                disassemble_to_index(clipper.DisplayEnd);
                for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
                    if (static_cast<std::size_t>(row) >= instructions.size()) {
                        break;
                    }
                    render_instruction(instructions[static_cast<std::size_t>(row)]);
                }
            }
            clipper.End();
        }
        ImGui::EndChild();
    }

    void disassembly_view::render_instruction(const instruction& instr) {
        ImGui::TextColored(theme::colors::overlay0, "0x%llX", static_cast<unsigned long long>(instr.address));
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x * 2.0f);

        bool is_selected = (selected_addr == instr.address);
        if (ImGui::Selectable(
                    std::format("##{}", instr.address).c_str(), is_selected,
                    ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap
            )) {
            selected_addr = instr.address;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Address: 0x%llX", static_cast<unsigned long long>(instr.address));
        }

        ImGui::SameLine(0.0f, 0.0f);

        for (size_t i = 0; i < instr.tokens.size(); ++i) {
            const auto& token = instr.tokens[i];
            ImGui::TextColored(get_token_color(token), "%s", token.text.c_str());

            if (i < instr.tokens.size() - 1) {
                ImGui::SameLine(0.0f, 0.0f);
            }
        }

        if (instr.comment) {
            ImGui::SameLine(0.0f, 20.0f);
            ImGui::TextColored(theme::colors::overlay1, "; \"%s\"", instr.comment->c_str());
        }
    }

    ImVec4 disassembly_view::get_token_color(const zydis::formatted_token& token) const {
        if (token.type == ZYDIS_TOKEN_MNEMONIC) {
            return theme::colors::blue;
        }

        switch (token.type) {
            case ZYDIS_TOKEN_REGISTER:
                return theme::colors::yellow;
            case ZYDIS_TOKEN_IMMEDIATE:
                return theme::colors::green;
            case ZYDIS_TOKEN_ADDRESS_ABS:
            case ZYDIS_TOKEN_ADDRESS_REL:
            case ZYDIS_TOKEN_SYMBOL:
                return theme::colors::peach;
            case ZYDIS_TOKEN_PREFIX:
                return theme::colors::red;
            case ZYDIS_TOKEN_DELIMITER:
            case ZYDIS_TOKEN_PARENTHESIS_OPEN:
            case ZYDIS_TOKEN_PARENTHESIS_CLOSE:
                return theme::colors::overlay1;
            case ZYDIS_TOKEN_DECORATOR:
                return theme::colors::rosewater;
            default:
                return theme::colors::text;
        }
    }

    void disassembly_view::update_target_regions() {
        instructions.clear();
        executable_regions.clear();
        region_data.clear();
        scan_offset = 0;
        selected_addr = 0;
        current_region_idx.reset();

        if (!app::active_target) {
            return;
        }

        auto regions_result = app::active_target->get_memory_regions();
        if (!regions_result) {
            return;
        }

        for (const auto& region : *regions_result) {
            if (region.permission.find('x') != std::string::npos) {
                executable_regions.push_back(region);
            }
        }
    }

    void disassembly_view::load_region_data(std::size_t index) {
        current_region_idx = index;
        instructions.clear();
        region_data.clear();
        scan_offset = 0;
        selected_addr = 0;

        if (!current_region_idx || !app::active_target) {
            return;
        }

        const auto& region = executable_regions[*current_region_idx];

        region_data.resize(region.size);
        auto read_result = app::active_target->read_memory(region.base_address, region_data);

        if (!read_result) {
            region_data.clear();
        }
    }

    void disassembly_view::disassemble_to_index(int index) {
        if (region_data.empty() || !current_region_idx) {
            return;
        }

        const std::size_t target = static_cast<std::size_t>(index);

        if (instructions.capacity() < target) {
            instructions.reserve(target);
        }

        while (instructions.size() < target && scan_offset < region_data.size()) {
            const auto* ptr = reinterpret_cast<const std::uint8_t*>(region_data.data()) + scan_offset;
            auto disassembled = zydis::disassemble(ptr);

            instruction instr{};
            instr.address = executable_regions[*current_region_idx].base_address + scan_offset;

            if (disassembled) {
                instr.decoded = disassembled->decoded;

                zydis::instruction wrapper_instr;
                wrapper_instr.decoded = instr.decoded;
                std::copy(disassembled->operands.begin(), disassembled->operands.end(), wrapper_instr.operands.begin());

                auto tokens = zydis::tokenize(wrapper_instr, instr.address);

                if (tokens && !tokens->empty()) {
                    instr.tokens = std::move(*tokens);
                } else {
                    instr.tokens.push_back({ZYDIS_TOKEN_MNEMONIC, "???"});
                }

                if (app::strings && app::strings->count() > 0) {
                    auto abs_addr = wrapper_instr.get_absolute_address(instr.address);

                    if (abs_addr) {
                        auto string_ref = app::strings->find_exact(static_cast<std::uintptr_t>(*abs_addr));
                        if (string_ref) {
                            std::string s = app::strings->read_string(app::active_target.get(), *string_ref);
                            if (s.size() > max_comment_length)
                                s = s.substr(0, max_comment_length - 3) + "...";
                            instr.comment = s;
                        }
                    }
                }

            } else {
                instr.tokens.push_back({ZYDIS_TOKEN_MNEMONIC, "db"});
                instr.tokens.push_back({ZYDIS_TOKEN_WHITESPACE, " "});
                instr.tokens.push_back(
                        {ZYDIS_TOKEN_IMMEDIATE, std::format("{:02X}", std::to_integer<int>(region_data[scan_offset]))}
                );

                instr.decoded.length = 1;
                instr.decoded.meta.category = ZYDIS_CATEGORY_INVALID;
            }

            instr.decoded.length = std::max<std::uint8_t>(1, instr.decoded.length);

            scan_offset += instr.decoded.length;
            instructions.push_back(std::move(instr));
        }
    }

} // namespace ui
