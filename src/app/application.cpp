#include "application.h"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "ui/theme.h"
#include "ui/view.h"
#include "ui/views/memory.h"
#include "ui/views/processes.h"

namespace app {
    std::unique_ptr<core::process> proc = nullptr;

    struct glfw_manager {
        glfw_manager() {
            if (glfwInit())
                is_initialized = true;
        }
        ~glfw_manager() {
            if (is_initialized)
                glfwTerminate();
        }
        bool is_initialized = false;
    };

    std::expected<application, init_error> application::create(int width, int height, std::string_view title) {
        static glfw_manager manager;
        if (!manager.is_initialized) {
            return std::unexpected(init_error::glfw_failed);
        }

        GLFWwindow* window = glfwCreateWindow(width, height, title.data(), nullptr, nullptr);
        if (!window) {
            return std::unexpected(init_error::window_failed);
        }

        return std::expected<application, init_error>(std::in_place, window);
    }

    application::application(GLFWwindow* window) : m_window_handle(window) {
        glfwMakeContextCurrent(m_window_handle.get());

        ImGui::CreateContext();

        ImGui_ImplGlfw_InitForOpenGL(m_window_handle.get(), true);
        ImGui_ImplOpenGL3_Init("#version 130");

        theme::apply();

        proc = std::make_unique<core::process>();

        m_views.push_back(std::make_unique<ui::processes_view>());
        m_views.push_back(std::make_unique<ui::memory_view>());
    }

    application::~application() {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

    void application::glfw_deleter::operator()(GLFWwindow* w) {
        if (w)
            glfwDestroyWindow(w);
    }

    void application::run() {
        while (!glfwWindowShouldClose(m_window_handle.get())) {
            glfwPollEvents();

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            render_ui();

            ImGui::Render();

            glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            glfwSwapBuffers(m_window_handle.get());
        }
    }

    void application::render_ui() {
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

    void application::render_toolbar() {
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
