#pragma once

#include <ui/view.h>

namespace ui {
    class file_info_view final : public view {
    public:
        file_info_view();
        void render() override;
    };
} // namespace ui
