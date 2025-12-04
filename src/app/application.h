#pragma once

#include <expected>
#include <memory>
#include <string_view>
#include <vector>

struct GLFWwindow;

namespace ui {
    class view;
}

namespace app {
    enum class init_error {
        glfw_failed,
        window_failed,
        opengl_loader_failed,
        imgui_failed,
    };

    class application {
    public:
        static std::expected<application, init_error> create(int width, int height, std::string_view title);

        application(GLFWwindow* window);
        ~application();

        void run();

        application(const application&) = delete;
        application& operator=(const application&) = delete;
        application(application&&) = delete;
        application& operator=(application&&) = delete;

    private:
        struct glfw_deleter {
            void operator()(GLFWwindow* w);
        };

        void render_ui();
        void show_open_file_popup();

        std::unique_ptr<GLFWwindow, glfw_deleter> m_window_handle;
        std::vector<std::unique_ptr<ui::view>> m_views;

        bool m_show_open_file_popup = false;
    };
} // namespace app
