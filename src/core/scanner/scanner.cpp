#include <core/scanner/scanner.h>
#include <cstring>
#include <mutex>
#include <optional>
#include <vector>
#include "core/target.h"

namespace core {
    namespace {
        template <typename T>
        T read_at(const void* ptr) {
            T val;
            std::memcpy(&val, ptr, sizeof(T));
            return val;
        }

        std::optional<std::vector<std::byte>> parse_input(const std::string& input, scan_data_type type) {
            std::vector<std::byte> buffer(scanner::type_size(type));

            try {
                if (type == scan_data_type::f32) {
                    float v = std::stof(input);
                    std::memcpy(buffer.data(), &v, sizeof(v));
                } else if (type == scan_data_type::f64) {
                    double v = std::stod(input);
                    std::memcpy(buffer.data(), &v, sizeof(v));
                }

                else if (type == scan_data_type::u8 || type == scan_data_type::i8) {
                    // chars to ensure they are treated as number
                    int v = std::stoi(input, nullptr, 0);
                    if (type == scan_data_type::u8) {
                        auto uv = static_cast<std::uint8_t>(v);
                        std::memcpy(buffer.data(), &uv, sizeof(uv));
                    } else {
                        auto sv = static_cast<std::int8_t>(v);
                        std::memcpy(buffer.data(), &sv, sizeof(sv));
                    }
                } else {
                    std::uint64_t v = std::stoull(input, nullptr, 0);
                    std::memcpy(buffer.data(), &v, buffer.size());
                }
            } catch (...) {
                return std::nullopt;
            }

            return buffer;
        }

        template <typename T>
        struct exact_matcher {
            T target;
            bool operator()(T val) const {
                return val == target;
            }
        };

        template <typename T>
        struct greater_matcher {
            T target;
            bool operator()(T val) const {
                return val > target;
            }
        };

        template <typename T>
        struct less_matcher {
            T target;
            bool operator()(T val) const {
                return val < target;
            }
        };

        template <typename Func>
        void dispatch_scan_type(scan_data_type type, Func&& func) {
            switch (type) {
                case scan_data_type::u8:
                    func.template operator()<std::uint8_t>();
                    break;
                case scan_data_type::i8:
                    func.template operator()<std::int8_t>();
                    break;
                case scan_data_type::u16:
                    func.template operator()<std::uint16_t>();
                    break;
                case scan_data_type::i16:
                    func.template operator()<std::int16_t>();
                    break;
                case scan_data_type::u32:
                    func.template operator()<std::uint32_t>();
                    break;
                case scan_data_type::i32:
                    func.template operator()<std::int32_t>();
                    break;
                case scan_data_type::u64:
                    func.template operator()<std::uint64_t>();
                    break;
                case scan_data_type::i64:
                    func.template operator()<std::int64_t>();
                    break;

                case scan_data_type::f32:
                    func.template operator()<float>();
                    break;
                case scan_data_type::f64:
                    func.template operator()<double>();
                    break;
            }
        }
    } // namespace

    scanner::scanner(target* t) : active_target(t) {
    }

    scanner::~scanner() {
        cancel();
    }

    void scanner::set_target(target* t) {
        reset();
        active_target = t;
    }

    void scanner::cancel() {
        if (scanning) {
            cancel_req = true;
            if (scan_thread.joinable())
                scan_thread.join();
        }
    }

    void scanner::reset() {
        cancel();
        std::lock_guard lock(results_mutex);
        results.clear();
    }

    void scanner::begin_first_scan(const scan_config& config) {
        cancel();
        scanning = true;
        cancel_req = false;

        scan_thread = std::jthread([this, config] {
            worker_scan_first(config);
        });
    }

    void scanner::begin_next_scan(const scan_config& config) {
        if (results.empty())
            return;

        cancel();
        scanning = true;
        cancel_req = false;

        scan_thread = std::jthread([this, config] {
            worker_scan_next(config);
        });
    }

    void scanner::worker_scan_first(scan_config config) {
        if (!active_target) {
            scanning = false;
            return;
        }

        auto regions_exp = active_target->get_memory_regions();
        if (!regions_exp) {
            scanning = false;
            return;
        }

        auto regions = *regions_exp;
        total_scan_bytes = 0;

        // only writable regions
        std::erase_if(regions, [](const auto& r) {
            return r.permission.find('w') == std::string::npos;
        });

        for (const auto& r : regions) {
            total_scan_bytes += r.size;
        }

        auto target_bytes = parse_input(config.value_str, config.data_type);
        if (!target_bytes) {
            scanning = false;
            return;
        }

        dispatch_scan_type(config.data_type, [&]<typename T>() {
            const T target_val = read_at<T>(target_bytes->data());
            std::size_t align = config.fast_scan ? type_size(config.data_type) : 1;
            const std::size_t chunk_size = 1024 * 1024; // 1mb chunks

            std::vector<scan_result> local_results;
            local_results.reserve(100000);
            std::vector<std::byte> buffer(chunk_size);

            bytes_scanned = 0;

            auto run_pass = [&](auto matcher) {
                for (const auto& r : regions) {
                    if (cancel_req)
                        break;

                    std::uintptr_t current = r.base_address;
                    std::size_t remaining = r.size;

                    while (remaining > 0 && !cancel_req) {
                        std::size_t read_size = std::min(remaining, chunk_size);
                        std::span<std::byte> view(buffer.data(), read_size);

                        if (active_target->read_memory(current, view)) {
                            scan_region<T>(current, view, matcher, local_results, align);
                        }

                        current += read_size;
                        remaining -= read_size;
                        bytes_scanned += read_size;

                        if (total_scan_bytes > 0) {
                            progress_val = static_cast<float>(bytes_scanned) / static_cast<float>(total_scan_bytes);
                        }
                    }
                }
            };

            switch (config.compare_type) {
                case scan_compare_type::exact:
                    run_pass(exact_matcher(target_val));
                    break;
                case scan_compare_type::greater:
                    run_pass(greater_matcher(target_val));
                    break;
                case scan_compare_type::less:
                    run_pass(less_matcher(target_val));
                    break;
            }

            std::lock_guard lock(results_mutex);
            results = std::move(local_results);
        });

        scanning = false;
        progress_val = 1.0f;
    }

    void scanner::worker_scan_next(scan_config config) {
        auto target_bytes = parse_input(config.value_str, config.data_type);
        if (!target_bytes || results.empty()) {
            scanning = false;
            return;
        }

        dispatch_scan_type(config.data_type, [&]<typename T>() {
            const T target_val = read_at<T>(target_bytes->data());

            std::vector<scan_result> next_results;
            next_results.reserve(results.size());

            std::vector<std::byte> buf(sizeof(T));
            std::size_t processed = 0;
            const std::size_t total = results.size();

            auto filter_pass = [&](auto matcher) {
                for (const auto& res : results) {
                    if (cancel_req)
                        break;

                    if (++processed % 1000 == 0) {
                        progress_val = static_cast<float>(processed) / static_cast<float>(total);
                    }

                    if (active_target->read_memory(res.address, buf)) {
                        T val = read_at<T>(buf.data());
                        if (matcher(val)) {
                            next_results.push_back(res);
                        }
                    }
                }
            };

            switch (config.compare_type) {
                case scan_compare_type::exact:
                    filter_pass(exact_matcher(target_val));
                    break;
                case scan_compare_type::greater:
                    filter_pass(greater_matcher(target_val));
                    break;
                case scan_compare_type::less:
                    filter_pass(less_matcher(target_val));
                    break;
            }

            std::lock_guard lock(results_mutex);
            results = std::move(next_results);
        });

        scanning = false;
        progress_val = 1.0f;
    }

    template <typename T, typename Predicate>
    void scanner::scan_region(
            std::uintptr_t base, std::span<const std::byte> buffer, Predicate pred,
            std::vector<scan_result>& local_results, std::size_t align
    ) {
        if (buffer.size() < sizeof(T))
            return;

        const std::byte* ptr = buffer.data();
        const std::byte* end = buffer.data() + buffer.size() - sizeof(T);

        for (; ptr <= end; ptr += align) {
            T val;
            std::memcpy(&val, ptr, sizeof(T));
            if (pred(val)) {
                local_results.push_back({
                        .address = base + static_cast<std::size_t>(ptr - buffer.data()),
                        .original_bytes = {} // TODO: only used if we track history
                });
            }
        }
    }

    std::size_t scanner::type_size(scan_data_type type) {
        switch (type) {
            case scan_data_type::i8:
            case scan_data_type::u8:
                return 1;
            case scan_data_type::i16:
            case scan_data_type::u16:
                return 2;
            case scan_data_type::i32:
            case scan_data_type::u32:
            case scan_data_type::f32:
                return 4;
            case scan_data_type::i64:
            case scan_data_type::u64:
            case scan_data_type::f64:
                return 8;
        }
        return 1;
    }


    std::string scanner::format_value(const std::vector<std::byte>& data, scan_data_type type) {
        if (data.size() < scanner::type_size(type))
            return "?";

        switch (type) {
            case scan_data_type::i8:
                return std::format("{}", read_at<std::int8_t>(data.data()));
            case scan_data_type::u8:
                return std::format("{}", read_at<std::uint8_t>(data.data()));
            case scan_data_type::i16:
                return std::format("{}", read_at<std::int16_t>(data.data()));
            case scan_data_type::u16:
                return std::format("{}", read_at<std::uint16_t>(data.data()));
            case scan_data_type::i32:
                return std::format("{}", read_at<std::int32_t>(data.data()));
            case scan_data_type::u32:
                return std::format("{}", read_at<std::uint32_t>(data.data()));
            case scan_data_type::i64:
                return std::format("{}", read_at<std::int64_t>(data.data()));
            case scan_data_type::u64:
                return std::format("{}", read_at<std::uint64_t>(data.data()));
            case scan_data_type::f32:
                return std::format("{:.3f}", read_at<float>(data.data()));
            case scan_data_type::f64:
                return std::format("{:.6f}", read_at<double>(data.data()));
        }
    }

    float scanner::progress() const {
        return progress_val;
    }

    bool scanner::is_scanning() const {
        return scanning;
    }

    std::size_t scanner::result_count() const {
        return results.size();
    }

    const std::vector<scan_result>& scanner::get_results() const {
        return results;
    }
} // namespace core
