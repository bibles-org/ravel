#pragma once

#include <core/process.h>
#include <optional>
#include <vector>

#include <ui/view.h>

namespace ui {
    class disassembly_view final : public view {
    public:
        disassembly_view();
        void render() override;

    private:
        void refresh_executable_regions();
        void select_region(std::size_t index);
        void ensure_display_offsets(int last_item_index);

        std::vector<core::memory_region> m_executable_regions;
        std::vector<std::byte> m_region_buffer;

        std::vector<std::size_t> m_instruction_offsets;

        std::size_t m_mapped_offset = 0;

        std::optional<std::size_t> m_selected_region_index;
        bool m_attached_last_frame = false;
    };
} // namespace ui
