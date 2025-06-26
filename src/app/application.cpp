#include "application.h"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "ui/theme.h"
#include "ui/view.h"
#include "ui/views/disassembly.h"
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

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

        ImGui_ImplGlfw_InitForOpenGL(m_window_handle.get(), true);
        ImGui_ImplOpenGL3_Init("#version 130");

        theme::apply();

        ImGuiStyle& style = ImGui::GetStyle();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            style.WindowRounding = 0.0f;
            style.Colors[ImGuiCol_WindowBg].w = 1.0f;
        }

        proc = std::make_unique<core::process>();

        m_views.push_back(std::make_unique<ui::processes_view>());
        m_views.push_back(std::make_unique<ui::memory_view>());
        m_views.push_back(std::make_unique<ui::disassembly_view>());
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

            int display_w, display_h;
            glfwGetFramebufferSize(m_window_handle.get(), &display_w, &display_h);
            glViewport(0, 0, display_w, display_h);
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            ImGuiIO& io = ImGui::GetIO();
            if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
                GLFWwindow* backup_current_context = glfwGetCurrentContext();
                ImGui::UpdatePlatformWindows();
                ImGui::RenderPlatformWindowsDefault();
                glfwMakeContextCurrent(backup_current_context);
            }

            glfwSwapBuffers(m_window_handle.get());
        }
    }

    void application::render_ui() {
        static bool dockspace_open = true;
        static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

        ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                        ImGuiWindowFlags_NoMove;
        window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

        if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
            window_flags |= ImGuiWindowFlags_NoBackground;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("DockSpace", &dockspace_open, window_flags);
        ImGui::PopStyleVar(3);

        ImGuiIO& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
            ImGuiID dockspace_id = ImGui::GetID("dockspace_ravel");
            ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
        }

        render_toolbar();

        ImGui::End();

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
        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("View")) {
                for (const auto& view : m_views) {
                    ImGui::MenuItem(view->get_title().data(), nullptr, view->get_visibility_flag());
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }
    }
} // namespace app
