#pragma once

#include <stop_token>
#include <thread>
#include <ui/view.h>

#include <atomic>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace core {
    class target;
}

namespace ui {

    struct xref_code_ref {
        std::uintptr_t address;
        std::string text;
        std::string type;
    };

    struct xref_item {
        std::uintptr_t address;
        std::string name;
        std::string value;
        std::vector<xref_code_ref> refs;
    };

    struct xref_row {
        size_t item_index;
        int ref_index; // -1 for parent item, >= 0 for child ref
    };

    class xref_view final : public view {
    public:
        xref_view();
        ~xref_view() override;

        void render() override;

    private:
        void render_toolbar();
        void render_list();

        void start_scan();
        void apply_filter();
        void rebuild_layout();

        void scan_task(core::target* t, std::stop_token st);

        core::target* current_target = nullptr;

        std::vector<xref_item> items;
        std::mutex data_mutex;

        std::vector<size_t> filtered_indices;
        std::vector<xref_row> display_rows;
        std::set<std::uintptr_t> expanded_items;

        std::jthread scan_thread;
        std::atomic<bool> is_scanning = false;
        std::atomic<float> progress = 0.0f;

        char filter_buffer[256] = {};
        bool filter_dirty = false;
        bool layout_dirty = false;
    };

} // namespace ui
