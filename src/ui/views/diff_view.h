#pragma once

#include <ui/view.h>
#include <core/differ.h>

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace ui {

    class diff_view final : public view {
    public:
        diff_view();
        void render() override;

    private:
        struct subroutine_display_item {
            enum class type {
                matched,
                primary_only,
                secondary_only
            };
            type item_type;
            size_t original_index;
            uint64_t primary_address;
            uint64_t secondary_address;
            double similarity_score;
        };

        void render_controls();
        void render_subroutine_list();
        void render_diff_pane();
        void process_comparison();
        void build_display_list();

        char primary_path_buf_[1024]{};
        char secondary_path_buf_[1024]{};

        std::optional<binary_differ::diff_result> diff_result_;

        std::vector<subroutine_display_item> subroutine_list_;
        int selected_subroutine_index_{-1};

        std::string error_message_;
    };

} // namespace ui
