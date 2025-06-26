#include <app/ctx.h>
#include <cinttypes> // For PRIXPTR
#include <format>
#include <ranges>
#include <ui/theme.h>
#include <ui/views/disassembly.h>

import zydis;

namespace ui {
    disassembly_view::disassembly_view() : view("Disassembly") {
        zydis::init(ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64, ZYDIS_FORMATTER_STYLE_INTEL);
    }

    void disassembly_view::render() {
        bool is_attached = app::proc->is_attached();
        if (is_attached != m_attached_last_frame) {
            refresh_executable_regions();
            m_attached_last_frame = is_attached;
        }

        if (ImGui::Button("Refresh Regions")) {
            refresh_executable_regions();
        }

        if (m_executable_regions.empty()) {
            ImGui::SameLine();
            ImGui::TextDisabled("(No executable regions found or process not attached)");
        } else {
            ImGui::SameLine();
            ImGui::SetNextItemWidth(450);

            std::string combo_preview_str;
            if (!m_selected_region_index.has_value()) {
                combo_preview_str = "Select an executable region";
            } else {
                const auto& region = m_executable_regions[*m_selected_region_index];
                combo_preview_str = std::format(
                        "{} (0x{:X} - 0x{:X})", region.name, region.base_address, region.base_address + region.size
                );
            }

            if (ImGui::BeginCombo("##region_combo", combo_preview_str.c_str())) {
                for (std::size_t i = 0; i < m_executable_regions.size(); ++i) {
                    const bool is_selected = m_selected_region_index.has_value() && (*m_selected_region_index == i);
                    std::string item_text = std::format(
                            "{} (0x{:X} - 0x{:X})", m_executable_regions[i].name, m_executable_regions[i].base_address,
                            m_executable_regions[i].base_address + m_executable_regions[i].size
                    );

                    if (ImGui::Selectable(item_text.c_str(), is_selected)) {
                        if (!m_selected_region_index.has_value() || *m_selected_region_index != i) {
                            select_region(i);
                        }
                    }
                    if (is_selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
        }

        ImGui::Separator();

        if (!m_selected_region_index.has_value()) {
            ImGui::Text("Select an executable memory region to view disassembly");
            return;
        }

        if (m_region_buffer.empty()) {
            ImGui::TextColored(theme::colors::RED, "Could not read memory for the selected region");
            return;
        }

        const ImGuiTableFlags flags =
                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY;

        if (ImGui::BeginTable("disassembly_table", 3, flags)) {
            ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthFixed, 150.0f);
            ImGui::TableSetupColumn("Bytes", ImGuiTableColumnFlags_WidthFixed, 200.0f);
            ImGui::TableSetupColumn("Instruction", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            const auto& region = m_executable_regions[*m_selected_region_index];

            ImGuiListClipper clipper;
            clipper.Begin(static_cast<int>(region.size));

            while (clipper.Step()) {
                ensure_display_offsets(clipper.DisplayEnd);

                for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
                    if (static_cast<std::size_t>(row) >= m_instruction_offsets.size()) {
                        break;
                    }

                    const std::size_t offset = m_instruction_offsets[static_cast<std::size_t>(row)];
                    const std::uint8_t* instruction_ptr =
                            reinterpret_cast<const std::uint8_t*>(m_region_buffer.data() + offset);

                    auto disasm_result = zydis::disassemble_format(instruction_ptr);

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextColored(theme::get_address_color(), "0x%" PRIXPTR, region.base_address + offset);

                    if (!disasm_result) {
                        ImGui::TableSetColumnIndex(1);
                        ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
                        ImGui::TextColored(
                                theme::get_bytes_color(), "%02X", std::to_integer<unsigned int>(m_region_buffer[offset])
                        );
                        ImGui::PopFont();
                        ImGui::TableSetColumnIndex(2);
                        ImGui::Text("db ??");
                        continue;
                    }

                    const auto& [decoded, text] = *disasm_result;

                    ImGui::TableSetColumnIndex(1);
                    std::string byte_str;
                    for (int i = 0; i < decoded.decoded.length; ++i) {
                        byte_str += std::format("{:02X} ", static_cast<unsigned int>(instruction_ptr[i]));
                    }
                    if (!byte_str.empty())
                        byte_str.pop_back();

                    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
                    ImGui::TextColored(theme::get_bytes_color(), "%s", byte_str.c_str());
                    ImGui::PopFont();

                    ImGui::TableSetColumnIndex(2);
                    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
                    ImGui::TextUnformatted(text.c_str());
                    ImGui::PopFont();
                }
            }
            clipper.End();

            ImGui::EndTable();
        }
    }

    void disassembly_view::refresh_executable_regions() {
        m_instruction_offsets.clear();
        m_executable_regions.clear();
        m_region_buffer.clear();
        m_mapped_offset = 0;
        m_selected_region_index.reset();

        if (!app::proc->is_attached()) {
            return;
        }

        auto regions_result = app::proc->get_memory_regions();
        if (!regions_result.has_value()) {
            return;
        }

        for (const auto& region : regions_result.value()) {
            if (region.permission.find('x') != std::string::npos) {
                m_executable_regions.push_back(region);
            }
        }
    }

    void disassembly_view::select_region(std::size_t index) {
        m_selected_region_index = index;
        m_instruction_offsets.clear();
        m_region_buffer.clear();
        m_mapped_offset = 0;

        if (!m_selected_region_index.has_value()) {
            return;
        }

        const auto& region = m_executable_regions[*m_selected_region_index];

        m_region_buffer.resize(region.size);
        auto read_result = app::proc->read_memory(region.base_address, m_region_buffer);

        if (!read_result.has_value()) {
            m_region_buffer.clear();
        }
    }

    void disassembly_view::ensure_display_offsets(int last_item_index) {
        if (static_cast<std::size_t>(last_item_index) <= m_instruction_offsets.size()) {
            return;
        }

        if (m_mapped_offset >= m_region_buffer.size()) {
            return;
        }

        while (m_instruction_offsets.size() < static_cast<std::size_t>(last_item_index) &&
               m_mapped_offset < m_region_buffer.size()) {

            m_instruction_offsets.push_back(m_mapped_offset);

            const std::uint8_t* instruction_ptr =
                    reinterpret_cast<const std::uint8_t*>(m_region_buffer.data() + m_mapped_offset);

            auto decoded_info = zydis::get_instruction_info(instruction_ptr);
            if (!decoded_info || decoded_info->length == 0) {
                m_mapped_offset++;
            } else {
                m_mapped_offset += decoded_info->length;
            }
        }
    }

} // namespace ui
