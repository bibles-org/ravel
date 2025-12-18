#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <span>
#include <string>
#include <thread>
#include <vector>

#include <core/target.h>

namespace core::analysis {

    struct string_ref {
        std::uintptr_t address;
        std::uint32_t length;
    };

    struct string_scan_config {
        std::size_t min_length = 4;
        bool scan_executable = false;
    };

    class strings_analyzer {
    public:
        strings_analyzer();
        ~strings_analyzer();

        strings_analyzer(const strings_analyzer&) = delete;
        strings_analyzer& operator=(const strings_analyzer&) = delete;

        void scan(target* t, string_scan_config config = {});
        void cancel();
        void clear();

        [[nodiscard]] bool is_scanning() const;
        [[nodiscard]] float progress() const;
        [[nodiscard]] std::size_t count() const;

        std::size_t get_batch(std::size_t start_index, std::span<string_ref> out_buffer) const;

        [[nodiscard]] std::optional<string_ref> find_exact(std::uintptr_t address) const;

        [[nodiscard]] std::string read_string(target* t, const string_ref& ref) const;

    private:
        void worker(target* t, string_scan_config config);

        mutable std::shared_mutex results_mutex;
        std::vector<string_ref> results;

        std::jthread scan_thread;
        std::atomic<bool> scanning = false;
        std::atomic<bool> cancel_req = false;
        std::atomic<float> progress_val = 0.0f;
    };

} // namespace core::analysis
