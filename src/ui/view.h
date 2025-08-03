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
    };
} // namespace ui
