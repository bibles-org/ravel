#pragma once

#include <core/scanner/scanner.h>
#include <optional>
#include <string>
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
        void draw_editor();

        core::scanner engine;
        core::scan_config config;
        core::target* last_target = nullptr;

        char val_buf[128]{};
        char write_buf[128]{};
        std::string write_message;
        std::optional<std::size_t> selected_result_idx;
        int selected_type_idx = 5; // i32
        int selected_cmp_idx = 0;  // exact
        bool is_first_scan = true;
    };
} // namespace ui
