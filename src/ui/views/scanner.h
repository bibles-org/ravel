#pragma once

#include <core/scanner/scanner.h>
#include <ui/view.h>

namespace ui {
    class scanner_view final : public view {
    public:
        scanner_view();
        void render() override;

    private:
        void draw_config();
        void draw_results();
        void draw_status();

        core::scanner engine;
        core::scan_config config;
        core::target* last_target = nullptr;

        char val_buf[128]{};
        int selected_type_idx = 5; // i32
        int selected_cmp_idx = 0; // exact
        bool is_first_scan = true;
    };
} // namespace ui
