#pragma once

#include <string>
#include <string_view>

namespace ui {
    class view {
    public:
        virtual ~view() = default;

        [[nodiscard]] std::string_view get_title() const {
            return m_title;
        }

        [[nodiscard]] bool is_visible() const {
            return m_is_visible;
        }

        bool* get_visibility_flag() {
            return &m_is_visible;
        }

        virtual void render() = 0;

    protected:
        explicit view(std::string title) : m_title(std::move(title)) {
        }

        view(const view&) = delete;
        view& operator=(const view&) = delete;
        view(view&&) = delete;
        view& operator=(view&&) = delete;

    private:
        std::string m_title;
        bool m_is_visible = true;
    };
} // namespace ui
