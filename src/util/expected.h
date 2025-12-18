#pragma once

namespace core {
    enum class error_code {
        proc_fs_unavailable,
        permission_denied,
        process_not_found,

        invalid_address,
        out_of_memory,
        read_failed,
        write_failed,
        partial_read,

        ptrace_attach_failed,
        ptrace_detach_failed,
        ptrace_read_failed,
        ptrace_write_failed,
        process_wait_failed,
        process_is_a_kernel_thread,
    };
} // namespace core
