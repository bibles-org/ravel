#pragma once

#include <core/analysis/strings.h>
#include <ui/view.h>
#include <vector>

namespace ui {

    class strings_view final : public view {
    public:
        strings_view();
        void render() override;

    private:
        void refresh();

        core::analysis::string_scan_config config;
        std::vector<core::analysis::string_ref> cache;
        bool needs_refresh = true;

        char filter_buf[128]{};
        bool use_filter = false;
    };

} // namespace ui
