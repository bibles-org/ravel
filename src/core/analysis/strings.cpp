#include <core/analysis/strings.h>

#include <algorithm>
#include <cctype>
#include <cstring>
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
        std::unique_lock lock(results_mutex);
        results.clear();
        results.shrink_to_fit();
    }

    bool strings_analyzer::is_scanning() const {
        return scanning;
    }

    float strings_analyzer::progress() const {
        return progress_val;
    }

    std::size_t strings_analyzer::count() const {
        std::shared_lock lock(results_mutex);
        return results.size();
    }

    std::size_t strings_analyzer::get_batch(std::size_t start_index, std::span<string_ref> out_buffer) const {
        std::shared_lock lock(results_mutex);

        if (start_index >= results.size()) {
            return 0;
        }

        std::size_t available = results.size() - start_index;
        std::size_t count = std::min(available, out_buffer.size());

        std::memcpy(out_buffer.data(), results.data() + start_index, count * sizeof(string_ref));

        return count;
    }

    std::optional<string_ref> strings_analyzer::find_exact(std::uintptr_t address) const {
        std::shared_lock lock(results_mutex);

        auto it = std::lower_bound(
                results.begin(), results.end(), address, [](const string_ref& ref, std::uintptr_t addr) {
                    return ref.address < addr;
                }
        );

        if (it != results.end() && it->address == address) {
            return *it;
        }
        return std::nullopt;
    }

    std::string strings_analyzer::read_string(target* t, const string_ref& ref) const {
        if (!t)
            return {};

        constexpr std::size_t max_display_len = 256;
        std::size_t len = std::min<std::size_t>(ref.length, max_display_len);

        std::string s;
        s.resize(len);

        if (t->read_memory(ref.address, std::as_writable_bytes(std::span(s)))) {
            for (char& c : s) {
                if (static_cast<unsigned char>(c) < 0x20 && c != '\t')
                    c = '.';
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
            bool is_readable = r.permission.find('r') != std::string::npos;
            return !is_readable;
        });

        std::size_t total_bytes = 0;
        for (const auto& r : regions)
            total_bytes += r.size;

        std::size_t bytes_processed = 0;

        std::vector<string_ref> local_results;
        local_results.reserve(100000);

        const std::size_t chunk_size = 64 * 1024;
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

                    const std::byte* ptr = view.data();
                    const std::byte* end = ptr + read_size;

                    for (const std::byte* p = ptr; p < end; ++p) {
                        unsigned char c = static_cast<unsigned char>(*p);
                        bool is_printable = (c >= 0x20 && c <= 0x7E) || c == '\t';

                        if (in_string) {
                            if (!is_printable) {
                                std::size_t len = static_cast<std::size_t>(p - ptr) - str_start;
                                if (len >= config.min_length) {
                                    local_results.push_back({current + str_start, static_cast<uint32_t>(len)});
                                }
                                in_string = false;
                            }
                        } else {
                            if (is_printable) {
                                in_string = true;
                                str_start = static_cast<std::size_t>(p - ptr);
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
                    if (bytes_processed % (1024 * 1024) == 0) {
                        progress_val.store(
                                static_cast<float>(bytes_processed) / static_cast<float>(total_bytes),
                                std::memory_order_relaxed
                        );
                    }
                }
            }
        }

        if (!cancel_req) {
            std::sort(local_results.begin(), local_results.end(), [](const auto& a, const auto& b) {
                return a.address < b.address;
            });

            std::unique_lock lock(results_mutex);
            results = std::move(local_results);
        }

        scanning = false;
        progress_val = 1.0f;
    }

} // namespace core::analysis
