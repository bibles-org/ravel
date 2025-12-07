#include "application.h"

#include <GLFW/glfw3.h>
#include <app/ctx.h>
#include <core/file_target.h>
#include <core/process.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_internal.h>
#include <print>

#include "ui/theme.h"
#include "ui/view.h"
#include "ui/views/diff_view.h"
#include "ui/views/disassembly.h"
#include "ui/views/file_info.h"
#include "ui/views/memory.h"
#include "ui/views/processes.h"

namespace app {
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

        m_views.push_back(std::make_unique<ui::processes_view>());
        m_views.push_back(std::make_unique<ui::file_info_view>());
        m_views.push_back(std::make_unique<ui::memory_view>());
        m_views.push_back(std::make_unique<ui::disassembly_view>());
        m_views.push_back(std::make_unique<ui::diff_view>());
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
            glClearColor(theme::colors::base.x, theme::colors::base.y, theme::colors::base.z, 1.0f);
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

    void application::show_open_file_popup() {
        if (m_show_open_file_popup) {
            ImGui::OpenPopup("Open File");
        }

        if (ImGui::BeginPopupModal("Open File", &m_show_open_file_popup, ImGuiWindowFlags_AlwaysAutoResize)) {
            static char path_buf[1024] = "";
            ImGui::InputText("File Path", path_buf, sizeof(path_buf));
            if (ImGui::Button("Open")) {
                auto file_target = core::file_target::create(path_buf);
                if (file_target) {
                    active_target = std::make_unique<core::file_target>(std::move(*file_target));
                } else {
                }
                m_show_open_file_popup = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                m_show_open_file_popup = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    void application::render_ui() {
        static bool dockspace_open = true;
        static bool first_time = true;
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

        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Open File...")) {
                    m_show_open_file_popup = true;
                }
                if (ImGui::MenuItem("Close Target", nullptr, false, active_target != nullptr)) {
                    active_target.reset();
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        show_open_file_popup();

        ImGuiID dockspace_id = ImGui::GetID("RavelDockspace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);

        if (first_time) {
            first_time = false;
            ImGui::DockBuilderRemoveNode(dockspace_id);
            ImGui::DockBuilderAddNode(dockspace_id, dockspace_flags | ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->WorkSize);

            ImGuiID dock_main_id = dockspace_id;
            ImGuiID dock_left_id =
                    ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.25f, nullptr, &dock_main_id);
            ImGuiID dock_bottom_id =
                    ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.30f, nullptr, &dock_main_id);
            ImGuiID dock_right_id =
                    ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.60f, nullptr, &dock_main_id);

            ImGui::DockBuilderDockWindow("Processes", dock_left_id);
            ImGui::DockBuilderDockWindow("File Info", dock_left_id);
            ImGui::DockBuilderDockWindow("Memory", dock_right_id);
            ImGui::DockBuilderDockWindow("Disassembly", dock_main_id);
            ImGui::DockBuilderDockWindow("Binary Diff", dock_bottom_id);
            ImGui::DockBuilderFinish(dockspace_id);
        }

        for (const auto& view : m_views) {
            if (ImGui::Begin(view->get_title().data(), nullptr, ImGuiWindowFlags_NoCollapse)) {
                view->render();
            }
            ImGui::End();
        }

        ImGui::End();
    }

} // namespace app
