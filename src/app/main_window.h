#pragma once

#include <memory>
#include <vector>

namespace ui {
    class view;
}

namespace app {
    class main_window {
    public:
        main_window();
        ~main_window();

        void render();

    private:
        void render_toolbar();

        std::vector<std::unique_ptr<ui::view>> m_views;
    };
} // namespace app
