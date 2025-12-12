#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <variant>
#include <vector>
#include "core/target.h"

namespace core {
    enum class scan_data_type : std::uint8_t {
        u8,
        i8,
        u16,
        i16,
        u32,
        i32,
        u64,
        i64,
        f32,
        f64
    };

    enum class scan_compare_type : std::uint8_t {
        exact,
        greater,
        less,
        // changed,
        // unchanged,
        // unknown // initial scan
    };

    struct scan_value {
        std::variant<std::uint64_t, double> data;
        scan_data_type type;
    };

    struct scan_result {
        std::uintptr_t address;
        std::vector<std::byte> original_bytes;
    };

    struct scan_config {
        scan_data_type data_type = scan_data_type::i32;
        scan_compare_type compare_type = scan_compare_type::exact;
        std::string value_str;
        bool fast_scan = true; // aligned scanning
    };

    class scanner {
    public:
        explicit scanner(target* t);
        ~scanner();

        scanner(const scanner&) = delete;
        scanner& operator=(const scanner&) = delete;
        scanner(scanner&&) = delete;
        scanner& operator=(scanner&&) = delete;

        void set_target(target* t);

        std::unique_lock<std::mutex> lock_results() const;

        void begin_first_scan(const scan_config& config);
        void begin_next_scan(const scan_config& config);

        void cancel();
        void reset();

        float progress() const;
        bool is_scanning() const;
        std::size_t result_count() const;

        const std::vector<scan_result>& get_results() const;
        static std::size_t type_size(scan_data_type type);
        static std::string format_value(const std::vector<std::byte>& data, scan_data_type type);

    private:
        target* active_target;

        void worker_scan_first(scan_config config);
        void worker_scan_next(scan_config config);

        template <typename T, typename Predicate>
        void scan_region(
                std::uintptr_t base, std::span<const std::byte> buffer, Predicate pred,
                std::vector<scan_result>& local_results, std::size_t align
        );

        std::vector<scan_result> results;
        mutable std::mutex results_mutex;

        std::atomic<bool> scanning = false;
        std::atomic<bool> cancel_req = false;
        std::atomic<float> progress_val = 0.0f;
        std::atomic<std::size_t> bytes_scanned = 0;
        std::size_t total_scan_bytes = 0;

        std::jthread scan_thread;
    };
} // namespace core
