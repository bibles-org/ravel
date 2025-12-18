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
        core::analysis::string_scan_config config;

        std::vector<core::analysis::string_ref> batch_buffer;
        std::size_t total_count = 0;
    };

} // namespace ui
