#include "application.h"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

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

        ImGui_ImplGlfw_InitForOpenGL(m_window_handle.get(), true);
        ImGui_ImplOpenGL3_Init("#version 130");

        m_main_window = std::make_unique<main_window>();
    }

    application::~application() {
        m_main_window.reset();
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

            m_main_window->render();

            ImGui::Render();

            glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            glfwSwapBuffers(m_window_handle.get());
        }
    }
} // namespace app
