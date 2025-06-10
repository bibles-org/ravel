#pragma once

#include <expected>
#include <memory>
#include <string_view>

#include <app/main_window.h>

struct GLFWwindow;

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

        std::unique_ptr<GLFWwindow, glfw_deleter> m_window_handle;
        std::unique_ptr<main_window> m_main_window;
    };
} // namespace app
