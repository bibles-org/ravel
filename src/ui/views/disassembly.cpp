#include <ui/views/disassembly.h>

#include <app/ctx.h>
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
        ImGui::TextColored(get_instruction_color(instr.decoded), "%s", instr.text.c_str());
    }

    ImVec4 disassembly_view::get_instruction_color(const ZydisDecodedInstruction& instr) const {
        switch (instr.meta.category) {
            case ZYDIS_CATEGORY_CALL:
                return theme::colors::blue;
            case ZYDIS_CATEGORY_COND_BR:
                return theme::colors::yellow;
            case ZYDIS_CATEGORY_UNCOND_BR:
                return theme::colors::red;
            case ZYDIS_CATEGORY_RET:
                return theme::colors::mauve;
            case ZYDIS_CATEGORY_INTERRUPT:
            case ZYDIS_CATEGORY_SYSTEM:
                return theme::colors::peach;
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

        if (instructions.capacity() < static_cast<std::size_t>(index)) {
            instructions.reserve(static_cast<std::size_t>(index));
        }

        while (instructions.size() < static_cast<std::size_t>(index) && scan_offset < region_data.size()) {
            const auto* ptr = reinterpret_cast<const std::uint8_t*>(region_data.data() + scan_offset);
            auto result = zydis::disassemble_format(ptr);

            instruction new_instr;
            new_instr.address = executable_regions[*current_region_idx].base_address + scan_offset;

            if (result) {
                auto& [decoded_full, text] = *result;
                new_instr.decoded = decoded_full.decoded;
                new_instr.text = std::move(text);
            } else {
                new_instr.text = "db ??";
                new_instr.decoded.length = 1;
                new_instr.decoded.meta.category = ZYDIS_CATEGORY_INVALID;
            }

            new_instr.decoded.length = std::max(std::uint8_t{1}, new_instr.decoded.length);

            scan_offset += new_instr.decoded.length;
            instructions.push_back(std::move(new_instr));
        }
    }

} // namespace ui
