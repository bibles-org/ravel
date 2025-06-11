#pragma once

#include <core/process.h>
#include <optional>
#include <ui/view.h>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

namespace ui {
    class processes_view final : public view {
    public:
        processes_view();

        void render() override;

    private:
        std::vector<core::process_info> m_processes;

        std::optional<std::size_t> m_selected_index;

        ImGuiTextFilter m_filter;
        char m_filter_buffer[256]{};

        void render_process_table(const ImGuiTextFilter& filter);
        void attach_action();
    };
} // namespace ui
