#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <core/target.h>

namespace core::analysis {

    struct string_ref {
        std::uintptr_t address;
        std::uint32_t length;

        bool operator<(const string_ref& other) const {
            return address < other.address;
        }
    };

    struct string_scan_config {
        std::size_t min_length = 4;
        bool scan_executable = false;
    };

    class strings_analyzer {
    public:
        strings_analyzer();
        ~strings_analyzer();

        void scan(target* t, string_scan_config config = {});
        void cancel();
        void clear();

        [[nodiscard]] bool is_scanning() const;
        [[nodiscard]] float progress() const;
        [[nodiscard]] std::size_t count() const;

        [[nodiscard]] std::vector<string_ref> get_results_copy() const;

        [[nodiscard]] std::optional<string_ref> find_exact(std::uintptr_t address) const;

        [[nodiscard]] std::string read_string(target* t, const string_ref& ref) const;

    private:
        void worker(target* t, string_scan_config config);

        mutable std::mutex results_mutex;
        std::vector<string_ref> results;

        std::jthread scan_thread;
        std::atomic<bool> scanning = false;
        std::atomic<bool> cancel_req = false;
        std::atomic<float> progress_val = 0.0f;
    };

} // namespace core::analysis
