#include <imgui.h>

#include <app/main_window.h>
#include <ui/view.h>

#include <ui/views/memory.h>
#include <ui/views/processes.h>

namespace app {
    main_window::main_window() {
        m_views.push_back(std::make_unique<ui::processes_view>());
        m_views.push_back(std::make_unique<ui::memory_viewer>());
    }

    main_window::~main_window() = default;

    void main_window::render() {
        render_toolbar();

        for (const auto& view : m_views) {
            if (view->is_visible()) {
                ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);

                if (ImGui::Begin(view->get_title().data(), view->get_visibility_flag())) {
                    view->render();
                }
                ImGui::End();
            }
        }
    }

    void main_window::render_toolbar() {
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("View")) {
                for (const auto& view : m_views) {
                    ImGui::MenuItem(view->get_title().data(), nullptr, view->get_visibility_flag());
                }
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }
    }
} // namespace app
