#include "xref_view.h"

#include <algorithm>
#include <format>
#include <map>
#include <numeric>

#include <Zycore/Status.h>
#include <Zydis/Utils.h>
#include <Zydis/Zydis.h>

#include <app/ctx.h>
#include <core/target.h>
#include <imgui.h>
#include <ui/theme.h>

import zydis;

namespace ui {

    namespace {
        std::string make_name(std::uintptr_t addr, int bits) {
            const char* type = "unk";
            switch (bits) {
                case 8:
                    type = "byte";
                    break;
                case 16:
                    type = "word";
                    break;
                case 32:
                    type = "dword";
                    break;
                case 64:
                    type = "qword";
                    break;
                case 128:
                    type = "xmm";
                    break;
            }
            return std::format("{}_{:X}", type, addr);
        }

        std::string make_val_def(int bits) {
            switch (bits) {
                case 8:
                    return "db ?";
                case 16:
                    return "dw ?";
                case 32:
                    return "dd ?";
                case 64:
                    return "dq ?";
                default:
                    return "db ?";
            }
        }
    } // namespace

    xref_view::xref_view() : view("Cross References") {
    }

    xref_view::~xref_view() {
        scan_thread.request_stop();
    }

    void xref_view::render() {
        if (app::active_target.get() != current_target) {
            scan_thread.request_stop();
            if (scan_thread.joinable()) {
                scan_thread.join();
            }
            current_target = app::active_target.get();
            std::lock_guard lock(data_mutex);
            items.clear();
            filtered_indices.clear();
            display_rows.clear();
            expanded_items.clear();
            is_scanning = false;
        }

        if (!current_target) {
            ImGui::TextDisabled("No target loaded.");
            return;
        }

        render_toolbar();
        ImGui::Separator();
        render_list();
    }

    void xref_view::render_toolbar() {
        if (is_scanning) {
            if (ImGui::Button("Stop")) {
                scan_thread.request_stop();
            }
            ImGui::SameLine();
            ImGui::TextColored(theme::colors::yellow, "Scanning... %.0f%%", progress * 100.0f);
        } else {
            if (ImGui::Button("Refresh")) {
                start_scan();
            }
        }

        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();

        {
            std::lock_guard lock(data_mutex);
            ImGui::Text("%zu Items", filtered_indices.size());
        }

        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        if (ImGui::InputTextWithHint("##filter", "Filter...", filter_buffer, sizeof(filter_buffer))) {
            filter_dirty = true;
        }
    }

    void xref_view::render_list() {
        if (filter_dirty) {
            apply_filter();
            filter_dirty = false;
        }
        if (layout_dirty) {
            rebuild_layout();
            layout_dirty = false;
        }

        std::lock_guard lock(data_mutex);

        const ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY |
                                      ImGuiTableFlags_SizingFixedFit;

        if (ImGui::BeginTable("##xreftable", 3, flags)) {
            ImGui::TableSetupColumn("Location", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Value / Instruction", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Info", ImGuiTableColumnFlags_WidthFixed, 60.0f);
            ImGui::TableHeadersRow();

            ImGuiListClipper clipper;
            clipper.Begin(static_cast<int>(display_rows.size()));

            while (clipper.Step()) {
                for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                    const auto& row = display_rows[i];
                    const auto& item = items[row.item_index];

                    ImGui::TableNextRow();

                    if (row.ref_index == -1) {
                        ImGui::TableSetColumnIndex(0);
                        bool is_expanded = expanded_items.contains(item.address);

                        ImGui::SetNextItemOpen(is_expanded);
                        bool open = ImGui::TreeNodeEx(
                                reinterpret_cast<void*>(item.address), ImGuiTreeNodeFlags_SpanFullWidth, "0x%llX  %s",
                                static_cast<unsigned long long>(item.address), item.name.c_str()
                        );

                        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
                        }

                        if (ImGui::IsItemToggledOpen()) {
                            if (is_expanded)
                                expanded_items.erase(item.address);
                            else
                                expanded_items.insert(item.address);
                            layout_dirty = true;
                        }

                        if (open)
                            ImGui::TreePop();

                        // Context Menu
                        if (ImGui::BeginPopupContextItem()) {
                            if (ImGui::MenuItem("Copy Address")) {
                                ImGui::SetClipboardText(std::format("0x{:X}", item.address).c_str());
                            }
                            if (ImGui::MenuItem("Copy Name")) {
                                ImGui::SetClipboardText(item.name.c_str());
                            }
                            ImGui::EndPopup();
                        }

                        ImGui::TableSetColumnIndex(1);
                        ImGui::TextColored(theme::colors::overlay1, "%s", item.value.c_str());

                        ImGui::TableSetColumnIndex(2);
                        ImGui::TextDisabled("%zu refs", item.refs.size());
                    } else {
                        // Child Row (Ref)
                        const auto& ref = item.refs[row.ref_index];

                        ImGui::TableSetColumnIndex(0);
                        ImGui::Indent(20.0f);
                        ImGui::TextColored(
                                theme::colors::blue, "sub_%llX", static_cast<unsigned long long>(ref.address)
                        );
                        if (ImGui::IsItemClicked()) {
                            // Navigate
                        }
                        if (ImGui::IsItemHovered()) {
                            ImGui::SetTooltip("Go to code");
                        }
                        ImGui::Unindent(20.0f);

                        ImGui::TableSetColumnIndex(1);
                        ImGui::TextUnformatted(ref.text.c_str());

                        ImGui::TableSetColumnIndex(2);
                        ImGui::TextColored(theme::colors::peach, "%s", ref.type.c_str());
                    }
                }
            }
            ImGui::EndTable();
        }
    }

    void xref_view::apply_filter() {
        std::lock_guard lock(data_mutex);
        filtered_indices.clear();
        std::string_view filter(filter_buffer);

        for (size_t i = 0; i < items.size(); ++i) {
            if (filter.empty()) {
                filtered_indices.push_back(i);
                continue;
            }

            const auto& item = items[i];
            if (item.name.find(filter) != std::string::npos) {
                filtered_indices.push_back(i);
                continue;
            }

            char addr_buf[32];
            std::snprintf(addr_buf, sizeof(addr_buf), "%llX", static_cast<unsigned long long>(item.address));
            if (std::string_view(addr_buf).find(filter) != std::string::npos) {
                filtered_indices.push_back(i);
            }
        }
        layout_dirty = true;
    }

    void xref_view::rebuild_layout() {
        std::lock_guard lock(data_mutex);
        display_rows.clear();
        display_rows.reserve(filtered_indices.size() * 2);

        for (size_t idx : filtered_indices) {
            const auto& item = items[idx];

            display_rows.push_back({idx, -1});

            if (expanded_items.contains(item.address)) {
                for (int i = 0; i < static_cast<int>(item.refs.size()); ++i) {
                    display_rows.push_back({idx, i});
                }
            }
        }
    }

    void xref_view::start_scan() {
        scan_thread.request_stop();
        if (scan_thread.joinable()) {
            scan_thread.join();
        }

        {
            std::lock_guard lock(data_mutex);
            items.clear();
            filtered_indices.clear();
            display_rows.clear();
            expanded_items.clear();
            is_scanning = true;
            progress = 0.0f;
        }

        scan_thread = std::jthread([this](std::stop_token st) {
            scan_task(current_target, st);
        });
    }

    void xref_view::scan_task(core::target* t, std::stop_token st) {
        if (!t)
            return;

        auto regions_opt = t->get_memory_regions();
        if (!regions_opt)
            return;

        const auto& regions = *regions_opt;
        std::vector<core::memory_region> code_segments;
        std::vector<core::memory_region> data_segments;
        size_t total_code_size = 0;

        for (const auto& r : regions) {
            if (r.permission.find('x') != std::string::npos) {
                code_segments.push_back(r);
                total_code_size += r.size;
            } else if (r.permission.find('r') != std::string::npos) {
                data_segments.push_back(r);
            }
        }

        std::map<uintptr_t, xref_item> result_map;
        std::vector<std::byte> buffer;
        size_t processed = 0;

        for (const auto& region : code_segments) {
            if (st.stop_requested())
                break;

            buffer.resize(std::min<size_t>(region.size, 1024 * 1024));
            size_t offset = 0;

            while (offset < region.size) {
                if (st.stop_requested())
                    break;

                size_t chunk = std::min(buffer.size(), region.size - offset);
                if (!t->read_memory(region.base_address + offset, std::span(buffer.data(), chunk))) {
                    offset += chunk;
                    continue;
                }

                const uint8_t* ptr = reinterpret_cast<const uint8_t*>(buffer.data());
                size_t chunk_offset = 0;

                while (chunk_offset < chunk) {
                    if ((chunk_offset % 2048) == 0) {
                        progress = static_cast<float>(processed + offset + chunk_offset) /
                                   static_cast<float>(total_code_size);
                    }

                    auto info = zydis::disassemble_format(ptr + chunk_offset);
                    if (!info) {
                        chunk_offset++;
                        continue;
                    }

                    const auto& [instr, text] = *info;
                    uintptr_t ip = region.base_address + offset + chunk_offset;

                    for (int i = 0; i < instr.decoded.operand_count_visible; ++i) {
                        const auto& op = instr.operands[i];
                        uintptr_t target = 0;
                        bool found = false;

                        if (op.type == ZYDIS_OPERAND_TYPE_MEMORY) {
                            if (op.mem.base == ZYDIS_REGISTER_RIP) {
                                ZyanU64 abs = 0;
                                if (ZYAN_SUCCESS(ZydisCalcAbsoluteAddress(&instr.decoded, &op, ip, &abs))) {
                                    target = static_cast<uintptr_t>(abs);
                                    found = true;
                                }
                            } else if (op.mem.base == ZYDIS_REGISTER_NONE && op.mem.index == ZYDIS_REGISTER_NONE) {
                                if (op.mem.disp.value != 0) {
                                    target = static_cast<uintptr_t>(op.mem.disp.value);
                                    found = true;
                                }
                            }
                        }

                        if (found) {
                            for (const auto& ds : data_segments) {
                                if (target >= ds.base_address && target < ds.base_address + ds.size) {
                                    auto& item = result_map[target];
                                    if (item.address == 0) {
                                        item.address = target;
                                        item.name = make_name(target, op.size);
                                        item.value = make_val_def(op.size);
                                    }

                                    std::string type = "r";
                                    if (instr.decoded.mnemonic == ZYDIS_MNEMONIC_MOV && i == 0)
                                        type = "w";
                                    else if (instr.decoded.mnemonic == ZYDIS_MNEMONIC_LEA)
                                        type = "o";

                                    item.refs.push_back({ip, text, type});
                                    break;
                                }
                            }
                        }
                    }
                    chunk_offset += instr.decoded.length;
                }
                offset += chunk;
            }
            processed += region.size;
        }

        if (!st.stop_requested()) {
            std::lock_guard lock(data_mutex);
            items.reserve(result_map.size());
            for (auto& [k, v] : result_map) {
                items.push_back(std::move(v));
            }
            std::sort(items.begin(), items.end(), [](const auto& a, const auto& b) {
                return a.address < b.address;
            });

            filtered_indices.resize(items.size());
            std::iota(filtered_indices.begin(), filtered_indices.end(), 0);

            display_rows.clear();
            for (size_t idx : filtered_indices) {
                display_rows.push_back({idx, -1});
            }
        }

        is_scanning = false;
        progress = 1.0f;
    }

} // namespace ui
