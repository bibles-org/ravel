#pragma once

#include <core/target.h>
#include <optional>
#include <string>
#include <ui/view.h>
#include <vector>

#include <Zydis/Zydis.h>
#include <imgui.h>

namespace ui {
    class disassembly_view final : public view {
    public:
        disassembly_view();
        void render() override;

    private:
        struct instruction {
            std::uintptr_t address;
            std::string text;
            ZydisDecodedInstruction decoded;
        };

        void update_target_regions();
        void load_region_data(std::size_t index);
        void disassemble_to_index(int index);

        void render_region_selector();
        void render_listing();
        void render_instruction(const instruction& instr);
        [[nodiscard]] ImVec4 get_instruction_color(const ZydisDecodedInstruction& instr) const;

        std::vector<core::memory_region> executable_regions;
        std::optional<std::size_t> current_region_idx;
        core::target* active_target = nullptr;

        std::vector<std::byte> region_data;
        std::vector<instruction> instructions;
        std::size_t scan_offset = 0;
        std::uintptr_t selected_addr = 0;
    };
} // namespace ui
