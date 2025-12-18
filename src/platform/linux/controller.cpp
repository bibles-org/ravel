#include <core/process.h>
#include <filesystem>
#include <platform/linux/controller.h>

#include <algorithm>
#include <charconv>
#include <fstream>

#include <dirent.h>
#include <fcntl.h>
#include <sys/uio.h>
#include <unistd.h>

namespace platform {
    linux_controller::~linux_controller() {
        if (m_mem_fd != -1) {
            close(m_mem_fd);
        }
    }

    std::optional<std::uint32_t> parse_int(std::string_view s) {
        std::uint32_t value;
        auto result = std::from_chars(s.data(), s.data() + s.size(), value);
        if (result.ec == std::errc()) {
            return value;
        }

        return std::nullopt;
    }

    std::expected<std::vector<core::process_info>, core::error_code> linux_controller::enumerate_processes() {
        std::vector<core::process_info> processes;

        DIR* proc_dir = opendir("/proc");
        if (!proc_dir) {
            return std::unexpected(core::error_code::proc_fs_unavailable);
        }

        std::unique_ptr<DIR, decltype(&closedir)> dir_closer(proc_dir, &closedir);

        for (const dirent* entry = readdir(proc_dir); entry != nullptr; entry = readdir(proc_dir)) {
            if (entry->d_type != DT_DIR)
                continue;

            if (auto pid_opt = parse_int(entry->d_name); pid_opt) {
                core::process_info info;
                info.pid = *pid_opt;

                std::filesystem::path comm_path = std::filesystem::path(std::format("/proc/{}/comm", entry->d_name));
                std::ifstream comm_file(comm_path);

                if (comm_file) {
                    std::getline(comm_file, info.name);
                    if (!info.name.empty() && info.name.back() == '\n') {
                        info.name.pop_back();
                    }
                } else {
                    info.name = "<unknown>";
                }

                std::filesystem::path exe_path = std::filesystem::path(std::format("/proc/{}/exe", entry->d_name));
                std::error_code ec;
                auto symlink_target = std::filesystem::read_symlink(exe_path, ec);

                if (!ec) {
                    info.executable_path = symlink_target;
                } else {
                    std::filesystem::path cmdline_path =
                            std::filesystem::path(std::format("/proc/{}/cmdline", entry->d_name));
                    std::ifstream cmdline_file(cmdline_path);

                    if (cmdline_file) {
                        std::string cmdline;
                        std::getline(cmdline_file, cmdline, '\0');
                        if (!cmdline.empty()) {
                            info.executable_path = cmdline;
                        } else {
                            info.executable_path = std::format("[{}]", info.name);
                        }
                    } else {
                        info.executable_path = std::format("[{}]", info.name);
                    }
                }

                processes.push_back(std::move(info));
            }
        }

        std::ranges::sort(processes, {}, &core::process_info::pid);
        return processes;
    }

    std::expected<std::vector<core::memory_region>, core::error_code>
    linux_controller::get_memory_regions(std::uint32_t pid) {
        std::vector<core::memory_region> regions;
        std::filesystem::path maps_path = std::filesystem::path(std::format("/proc/{}/maps", std::to_string(pid)));

        std::ifstream maps_file(maps_path);
        if (!maps_file) {
            return std::unexpected(core::error_code::permission_denied);
        }

        std::string line;
        while (std::getline(maps_file, line)) {
            std::string_view line_view = line;

            auto first_space = line_view.find(' ');
            if (first_space == std::string_view::npos)
                continue;

            auto second_space = line_view.find(' ', first_space + 1);
            if (second_space == std::string_view::npos)
                continue;

            auto addr_range = line_view.substr(0, first_space);
            auto perms = line_view.substr(first_space + 1, second_space - first_space - 1);

            auto dash_pos = addr_range.find('-');
            if (dash_pos == std::string_view::npos)
                continue;

            auto start_str = addr_range.substr(0, dash_pos);
            auto end_str = addr_range.substr(dash_pos + 1);

            std::uint64_t start, end;
            if (std::from_chars(start_str.data(), start_str.data() + start_str.size(), start, 16).ec != std::errc{}) {
                continue;
            }

            if (std::from_chars(end_str.data(), end_str.data() + end_str.size(), end, 16).ec != std::errc{}) {
                continue;
            }

            core::memory_region region;
            region.base_address = start;
            region.size = end - start;
            region.permission = std::string(perms);

            // name from eol
            auto remaining = line_view.substr(second_space + 1);
            size_t field_count = 0;
            size_t pos = 0;

            // skip offset, device, inode fields
            while (pos < remaining.size() && field_count < 3) {
                pos = remaining.find(' ', pos);
                if (pos == std::string_view::npos)
                    break;
                pos = remaining.find_first_not_of(' ', pos);
                if (pos == std::string_view::npos)
                    break;
                field_count++;
            }

            if (pos < remaining.size()) {
                region.name = std::string(remaining.substr(pos));
            } else {
                region.name = "<anonymous>";
            }

            regions.push_back(std::move(region));
        }

        return regions;
    }

    std::expected<void, core::error_code> linux_controller::attach(std::uint32_t pid) {
        if (m_mem_fd != -1) {
            close(m_mem_fd);
            m_mem_fd = -1;
        }

        std::string mem_path = std::format("/proc/{}/mem", pid);
        m_mem_fd = open(mem_path.c_str(), O_RDONLY);

        if (m_mem_fd == -1) {
            switch (errno) {
                case EACCES:
                case EPERM:
                    return std::unexpected(core::error_code::permission_denied);
                case ENOENT:
                    return std::unexpected(core::error_code::process_not_found);
                default:
                    return std::unexpected(core::error_code::proc_fs_unavailable);
            }
        }

        return {};
    }

    void linux_controller::detach(std::uint32_t) {
        if (m_mem_fd != -1) {
            close(m_mem_fd);
            m_mem_fd = -1;
        }
    }

    std::expected<void, core::error_code>
    linux_controller::read_memory(std::uint32_t pid, std::uintptr_t address, std::span<std::byte> buffer) {
        if (pid == 0) {
            return std::unexpected(core::error_code::process_not_found);
        }

        iovec local_iov;
        local_iov.iov_base = buffer.data();
        local_iov.iov_len = buffer.size();

        iovec remote_iov;
        remote_iov.iov_base = reinterpret_cast<void*>(address);
        remote_iov.iov_len = buffer.size();

        ssize_t bytes_read = process_vm_readv(static_cast<pid_t>(pid), &local_iov, 1, &remote_iov, 1, 0);

        if (bytes_read == -1) {
            switch (errno) {
                case EPERM:
                case EACCES:
                    return std::unexpected(core::error_code::permission_denied);
                case ESRCH:
                    return std::unexpected(core::error_code::process_not_found);
                case EFAULT:
                    return std::unexpected(core::error_code::invalid_address);
                case ENOMEM:
                    return std::unexpected(core::error_code::out_of_memory);
                default:
                    return std::unexpected(core::error_code::read_failed);
            }
        }

        if (static_cast<size_t>(bytes_read) != buffer.size()) {
            return std::unexpected(core::error_code::partial_read);
        }

        return {};
    }

    std::expected<void, core::error_code>
    linux_controller::write_memory(std::uint32_t pid, std::uintptr_t address, std::span<const std::byte> buffer) {
        if (pid == 0) {
            return std::unexpected(core::error_code::process_not_found);
        }

        iovec local_iov;
        local_iov.iov_base = const_cast<std::byte*>(buffer.data());
        local_iov.iov_len = buffer.size();

        iovec remote_iov;
        remote_iov.iov_base = reinterpret_cast<void*>(address);
        remote_iov.iov_len = buffer.size();

        ssize_t bytes_written = process_vm_writev(static_cast<pid_t>(pid), &local_iov, 1, &remote_iov, 1, 0);

        if (bytes_written == -1) {
            switch (errno) {
                case EPERM:
                case EACCES:
                    return std::unexpected(core::error_code::permission_denied);
                case ESRCH:
                    return std::unexpected(core::error_code::process_not_found);
                case EFAULT:
                    return std::unexpected(core::error_code::invalid_address);
                case ENOMEM:
                    return std::unexpected(core::error_code::out_of_memory);
                default:
                    return std::unexpected(core::error_code::write_failed);
            }
        }

        if (static_cast<size_t>(bytes_written) != buffer.size()) {
            return std::unexpected(core::error_code::partial_read);
        }

        return {};
    }
} // namespace platform
