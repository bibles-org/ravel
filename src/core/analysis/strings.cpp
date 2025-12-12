#include <core/analysis/strings.h>

#include <algorithm>
#include <cctype>
#include <ranges>

namespace core::analysis {

    strings_analyzer::strings_analyzer() = default;

    strings_analyzer::~strings_analyzer() {
        cancel();
    }

    void strings_analyzer::scan(target* t, string_scan_config config) {
        cancel();
        clear();

        scanning = true;
        cancel_req = false;
        progress_val = 0.0f;

        scan_thread = std::jthread([this, t, config] {
            worker(t, config);
        });
    }

    void strings_analyzer::cancel() {
        if (scanning) {
            cancel_req = true;
            if (scan_thread.joinable()) {
                scan_thread.join();
            }
        }
    }

    void strings_analyzer::clear() {
        std::lock_guard lock(results_mutex);
        results.clear();
    }

    bool strings_analyzer::is_scanning() const {
        return scanning;
    }

    float strings_analyzer::progress() const {
        return progress_val;
    }

    std::size_t strings_analyzer::count() const {
        std::lock_guard lock(results_mutex);
        return results.size();
    }

    std::vector<string_ref> strings_analyzer::get_results_copy() const {
        std::lock_guard lock(results_mutex);
        return results;
    }

    std::optional<string_ref> strings_analyzer::find_exact(std::uintptr_t address) const {
        std::lock_guard lock(results_mutex);

        auto it = std::ranges::lower_bound(results, address, {}, &string_ref::address);
        if (it != results.end() && it->address == address) {
            return *it;
        }
        return std::nullopt;
    }

    std::string strings_analyzer::read_string(target* t, const string_ref& ref) const {
        if (!t)
            return {};

        // cap max length for ui display purposes
        std::size_t len = std::min<std::size_t>(ref.length, 256);
        std::vector<std::byte> buffer(len);

        if (t->read_memory(ref.address, buffer)) {
            std::string s;
            s.reserve(len);
            for (auto b : buffer) {
                s.push_back(static_cast<char>(b));
            }
            return s;
        }
        return "??";
    }

    void strings_analyzer::worker(target* t, string_scan_config config) {
        if (!t) {
            scanning = false;
            return;
        }

        auto regions_exp = t->get_memory_regions();
        if (!regions_exp) {
            scanning = false;
            return;
        }

        auto regions = *regions_exp;

        std::erase_if(regions, [&](const auto& r) {
            bool is_exec = r.permission.find('x') != std::string::npos;
            if (is_exec && !config.scan_executable)
                return true;
            return false;
        });

        std::size_t total_bytes = 0;
        for (const auto& r : regions)
            total_bytes += r.size;

        std::size_t bytes_processed = 0;
        std::vector<string_ref> local_results;
        local_results.reserve(10000);

        const std::size_t chunk_size = 1024 * 1024; // 1MB chunks
        std::vector<std::byte> buffer(chunk_size);

        for (const auto& r : regions) {
            if (cancel_req)
                break;

            std::uintptr_t current = r.base_address;
            std::size_t remaining = r.size;

            while (remaining > 0 && !cancel_req) {
                std::size_t read_size = std::min(remaining, chunk_size);
                std::span<std::byte> view(buffer.data(), read_size);

                if (t->read_memory(current, view)) {
                    std::size_t str_start = 0;
                    bool in_string = false;

                    for (std::size_t i = 0; i < read_size; ++i) {
                        char c = static_cast<char>(view[i]);
                        // standard printable ascii + tab
                        bool is_printable = (c >= 0x20 && c <= 0x7E) || c == '\t';

                        if (in_string) {
                            if (!is_printable) {
                                std::size_t len = i - str_start;
                                if (len >= config.min_length) {
                                    local_results.push_back({current + str_start, static_cast<uint32_t>(len)});
                                }
                                in_string = false;
                            }
                        } else {
                            if (is_printable) {
                                in_string = true;
                                str_start = i;
                            }
                        }
                    }

                    if (in_string) {
                        std::size_t len = read_size - str_start;
                        if (len >= config.min_length) {
                            local_results.push_back({current + str_start, static_cast<uint32_t>(len)});
                        }
                    }
                }

                current += read_size;
                remaining -= read_size;
                bytes_processed += read_size;

                if (total_bytes > 0) {
                    progress_val = static_cast<float>(bytes_processed) / static_cast<float>(total_bytes);
                }
            }
        }

        if (!cancel_req) {
            std::lock_guard lock(results_mutex);
            results = std::move(local_results);

            std::ranges::sort(results, [](const auto& a, const auto& b) {
                return a.address < b.address;
            });
        }

        scanning = false;
        progress_val = 1.0f;
    }

} // namespace core::analysis
