#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>

#include <platform/windows_controller.h>
#include <core/process.h>
#include <util/expected.h>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace platform {
    namespace {
        core::error_code to_error_code(DWORD win_err) {
            switch (win_err) {
                case ERROR_ACCESS_DENIED:
                    return core::error_code::permission_denied;
                case ERROR_INVALID_PARAMETER:
                case ERROR_INVALID_HANDLE:
                    return core::error_code::process_not_found;
                case ERROR_PARTIAL_COPY:
                    return core::error_code::partial_read;
                case ERROR_NOACCESS:
                    return core::error_code::invalid_address;
                case ERROR_NOT_ENOUGH_MEMORY:
                    return core::error_code::out_of_memory;
                default:
                    return core::error_code::read_failed;
            }
        }

        struct snapshot_handle_deleter {
            void operator()(HANDLE h) const {
                if (h != INVALID_HANDLE_VALUE) {
                    CloseHandle(h);
                }
            }
        };
        using unique_snapshot_handle = std::unique_ptr<void, snapshot_handle_deleter>;

        std::string protect_flags_string(DWORD protect) {
            if (protect & PAGE_NOACCESS)
                return "---";
            if (protect & PAGE_GUARD)
                return "g--";

            std::string perms = "---";
            if (protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE |
                           PAGE_WRITECOPY | PAGE_EXECUTE_WRITECOPY)) {
                perms[0] = 'r';
            }
            if (protect & (PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) {
                perms[1] = 'w';
            }
            if (protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) {
                perms[2] = 'x';
            }
            return perms;
        }

    } // namespace

    windows_controller::windows_controller() = default;

    windows_controller::~windows_controller() {
        detach(0);
    }

    std::expected<std::vector<core::process_info>, core::error_code> windows_controller::enumerate_processes() {
        std::vector<core::process_info> processes;

        unique_snapshot_handle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
        if (snapshot.get() == INVALID_HANDLE_VALUE) {
            return std::unexpected(to_error_code(GetLastError()));
        }

        PROCESSENTRY32W pe32;
        pe32.dwSize = sizeof(PROCESSENTRY32W);

        if (!Process32FirstW(snapshot.get(), &pe32)) {
            if (GetLastError() == ERROR_NO_MORE_FILES) {
                return processes;
            }
            return std::unexpected(to_error_code(GetLastError()));
        }

        do {
            core::process_info info;
            info.pid = pe32.th32ProcessID;
            info.name = std::filesystem::path(pe32.szExeFile).string();

            HANDLE process_handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, info.pid);
            if (process_handle) {
                WCHAR path_buffer[MAX_PATH];
                if (GetModuleFileNameExW(process_handle, nullptr, path_buffer, MAX_PATH)) {
                    info.executable_path = path_buffer;
                } else {
                    info.executable_path = L"[access denied]";
                }
                CloseHandle(process_handle);
            } else {
                if (info.pid == 0 || info.pid == 4) {
                    info.executable_path = L"[system process]";
                } else {
                    info.executable_path = L"[access denied]";
                }
            }

            processes.push_back(std::move(info));
        } while (Process32NextW(snapshot.get(), &pe32));

        std::ranges::sort(processes, {}, &core::process_info::pid);
        return processes;
    }

    std::expected<void, core::error_code> windows_controller::attach(std::uint32_t pid) {
        if (m_process_handle) {
            detach(0);
        }

        m_process_handle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_VM_OPERATION, FALSE, pid);

        if (!m_process_handle) {
            return std::unexpected(to_error_code(GetLastError()));
        }

        return {};
    }

    void windows_controller::detach(std::uint32_t /*pid*/) {
        if (m_process_handle) {
            CloseHandle(m_process_handle);
            m_process_handle = nullptr;
        }
    }

    std::expected<std::vector<core::memory_region>, core::error_code>
    windows_controller::get_memory_regions(std::uint32_t /*pid*/) {
        if (!m_process_handle) {
            return std::unexpected(core::error_code::process_not_found);
        }

        std::vector<core::memory_region> regions;
        MEMORY_BASIC_INFORMATION mbi;
        std::uintptr_t current_address = 0;

        while (VirtualQueryEx(m_process_handle, reinterpret_cast<LPCVOID>(current_address), &mbi, sizeof(mbi)) ==
               sizeof(mbi)) {

            if (mbi.State != MEM_FREE) {
                core::memory_region region;
                region.base_address = reinterpret_cast<std::uintptr_t>(mbi.BaseAddress);
                region.size = mbi.RegionSize;
                region.permission = protect_flags_string(mbi.Protect);

                WCHAR name_buffer[MAX_PATH];
                if (GetMappedFileNameW(m_process_handle, mbi.BaseAddress, name_buffer, MAX_PATH)) {
                    region.name = std::filesystem::path(name_buffer).string();
                } else {
                    switch (mbi.Type) {
                        case MEM_IMAGE:
                            region.name = "[image]";
                            break;
                        case MEM_MAPPED:
                            region.name = "[mapped]";
                            break;
                        case MEM_PRIVATE:
                            region.name = "[private]";
                            break;
                        default:
                            region.name = "<anonymous>";
                            break;
                    }
                }
                regions.push_back(std::move(region));
            }

            current_address = reinterpret_cast<std::uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;

            if (current_address < reinterpret_cast<std::uintptr_t>(mbi.BaseAddress)) {
                break;
            }
        }
        return regions;
    }

    std::expected<void, core::error_code>
    windows_controller::read_memory(std::uint32_t /*pid*/, std::uintptr_t address, std::span<std::byte> buffer) {
        if (!m_process_handle) {
            return std::unexpected(core::error_code::process_not_found);
        }

        SIZE_T bytes_read = 0;
        if (!ReadProcessMemory(
                    m_process_handle, reinterpret_cast<LPCVOID>(address), buffer.data(), buffer.size(), &bytes_read
            )) {
            return std::unexpected(to_error_code(GetLastError()));
        }

        if (bytes_read != buffer.size()) {
            return std::unexpected(core::error_code::partial_read);
        }
        return {};
    }

} // namespace platform
