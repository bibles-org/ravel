#include "application.h"

#include <GLFW/glfw3.h>
#include <app/ctx.h>
#include <core/file_target.h>
#include <core/pe_dumper.h>
#include <core/process.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_internal.h>
#include <print>

#include "ui/icons.h"
#include "ui/theme.h"
#include "ui/view.h"
#include "ui/views/diff_view.h"
#include "ui/views/disassembly.h"
#include "ui/views/file_info.h"
#include "ui/views/memory.h"
#include "ui/views/processes.h"
#include "ui/views/scanner.h"
#include "ui/views/strings.h"
#include "ui/views/xref_view.h"

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

        m_views.push_back({std::make_unique<ui::processes_view>(), true});
        m_views.push_back({std::make_unique<ui::disassembly_view>(), true});
        m_views.push_back({std::make_unique<ui::file_info_view>(), false});
        m_views.push_back({std::make_unique<ui::memory_view>(), false});
        m_views.push_back({std::make_unique<ui::scanner_view>(), false});
        m_views.push_back({std::make_unique<ui::strings_view>(), false});
        m_views.push_back({std::make_unique<ui::diff_view>(), false});
        m_views.push_back({std::make_unique<ui::xref_view>(), false});
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

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            if (ImGui::Button("Open", ImVec2(120, 0))) {
                auto file_target = core::file_target::create(path_buf);
                if (file_target) {
                    active_target = std::make_unique<core::file_target>(std::move(*file_target));
                }
                m_show_open_file_popup = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                m_show_open_file_popup = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    void application::show_dump_popup() {
        if (m_show_dump_popup) {
            ImGui::OpenPopup("Dump PE Image");
        }

        if (ImGui::BeginPopupModal("Dump PE Image", &m_show_dump_popup, ImGuiWindowFlags_AlwaysAutoResize)) {
            static char path_buf[1024] = "dump.exe";
            ImGui::InputText("Output File", path_buf, sizeof(path_buf));

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            if (ImGui::Button("Dump", ImVec2(120, 0))) {
                if (active_target && active_target->is_live()) {
                    auto result = core::dump_pe_image(active_target.get(), path_buf);
                    if (!result) {
                        std::println(stderr, "failed to dump pe image (code={})", static_cast<int>(result.error()));
                    }
                }
                m_show_dump_popup = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                m_show_dump_popup = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }


    void application::render_top_bar() {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, 8.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12.0f, 6.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(12.0f, 0.0f));

        ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::colors::mantle);
        ImGui::PushStyleColor(ImGuiCol_Border, theme::colors::surface0);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);

        const float bar_height = 46.0f;

        if (ImGui::BeginChild("TopBar", ImVec2(0, bar_height), true, ImGuiWindowFlags_NoScrollbar)) {
            ImGui::AlignTextToFramePadding();
            ImGui::PushStyleColor(ImGuiCol_Text, theme::colors::primary);
            ImGui::Text("RAVEL");
            ImGui::PopStyleColor();

            ImGui::SameLine();
            ImGui::TextDisabled("|");
            ImGui::SameLine();

            auto menu_btn = [&](const char* label, const char* popup_id, const char* icon = nullptr) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, theme::colors::surface1);
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, theme::colors::surface2);

                std::string btn_text = icon ? std::format("{}  {}", icon, label) : label;

                if (ImGui::Button(btn_text.c_str())) {
                    ImGui::OpenPopup(popup_id);
                }

                ImGui::PopStyleColor(3);
            };

            auto push_popup_style = []() {
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 14.0f));
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 10.0f)); // Wide spacing
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 6.0f));
                ImGui::PushStyleColor(ImGuiCol_PopupBg, theme::colors::surface0);
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, theme::colors::surface1);
                ImGui::PushStyleColor(ImGuiCol_Separator, theme::colors::surface2);
            };

            auto pop_popup_style = []() {
                ImGui::PopStyleColor(3);
                ImGui::PopStyleVar(3);
            };

            menu_btn("File", "FileMenuPopup", ui::icons::folder);

            push_popup_style();
            if (ImGui::BeginPopup("FileMenuPopup")) {
                if (ImGui::MenuItem(std::format("{}  Open File...", ui::icons::folder).c_str())) {
                    m_show_open_file_popup = true;
                }
                if (ImGui::MenuItem("Close Target", nullptr, false, active_target != nullptr)) {
                    active_target.reset();
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                if (ImGui::MenuItem(
                            "Dump PE Image...", nullptr, false, active_target != nullptr && active_target->is_live()
                    )) {
                    m_show_dump_popup = true;
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                if (ImGui::MenuItem(std::format("{}  Exit", ui::icons::close).c_str())) {
                    glfwSetWindowShouldClose(m_window_handle.get(), 1);
                }
                ImGui::EndPopup();
            }
            pop_popup_style();

            ImGui::SameLine();

            menu_btn("Views", "ViewsMenuPopup", ui::icons::layers);

            push_popup_style();
            if (ImGui::BeginPopup("ViewsMenuPopup")) {
                for (auto& entry : m_views) {
                    bool visible = entry.visible;
                    if (ImGui::MenuItem(entry.instance->get_title().data(), nullptr, &visible)) {
                        entry.visible = visible;
                    }
                }
                ImGui::EndPopup();
            }
            pop_popup_style();
        }
        ImGui::EndChild();

        ImGui::PopStyleVar(4);
        ImGui::PopStyleColor(2);
    }

    void application::render_ui() {
        static bool dockspace_open = true;
        static bool first_time = true;
        static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking;
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

        render_top_bar();
        show_open_file_popup();
        show_dump_popup();

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

            ImGui::DockBuilderDockWindow("Processes", dock_left_id);
            ImGui::DockBuilderDockWindow("Disassembly", dock_main_id);
            ImGui::DockBuilderDockWindow("Strings", dock_main_id);

            ImGui::DockBuilderFinish(dockspace_id);
        }

        for (auto& entry : m_views) {
            if (entry.visible) {
                if (ImGui::Begin(entry.instance->get_title().data(), &entry.visible, ImGuiWindowFlags_NoCollapse)) {
                    entry.instance->render();
                }
                ImGui::End();
            }
        }

        ImGui::End();
    }

} // namespace app
