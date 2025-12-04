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

        void process_table(core::process& target, const ImGuiTextFilter& filter);
    };
} // namespace ui
