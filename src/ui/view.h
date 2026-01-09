#pragma once

#include <format>
#include <string>
#include <string_view>

namespace ui {
    class view {
    public:
        virtual ~view() = default;

        [[nodiscard]] std::string_view get_title() const {
            return m_title;
        }

        virtual void render() = 0;

    protected:
        view(std::string_view title, std::string_view icon) {
            m_title = std::format("{} {}###{}", icon, title, title);
        }

        view(const view&) = delete;
        view& operator=(const view&) = delete;
        view(view&&) = delete;
        view& operator=(view&&) = delete;

    private:
        std::string m_title;
    };
} // namespace ui
