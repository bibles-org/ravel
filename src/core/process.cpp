#include <core/process.h>

#if defined(__linux__)
#include <platform/linux_controller.h>
#else
#error "unsupported platform"
#endif

namespace core {

    process::process() {
#if defined(__linux__)
        m_controller = std::make_unique<platform::linux_controller>();
#endif
    }

    std::expected<std::vector<process_info>, error_code> process::enumerate_processes() {
        return m_controller->enumerate_processes();
    }

    std::expected<void, error_code> process::attach_to(std::uint32_t pid) {
        if (m_attached_pid != 0) {
            detach();
        }

        auto result = m_controller->attach(pid);
        if (result.has_value()) {
            m_attached_pid = pid;
        }
        return result;
    }

    void process::detach() {
        if (m_attached_pid != 0) {
            m_controller->detach(m_attached_pid);
            m_attached_pid = 0;
        }
    }

    bool process::is_attached() const {
        return m_attached_pid != 0;
    }

    std::expected<std::vector<memory_region>, error_code> process::get_memory_regions() {
        if (!is_attached()) {
            return std::unexpected(error_code::process_not_found);
        }
        return m_controller->get_memory_regions(m_attached_pid);
    }

    std::expected<void, error_code> process::read_memory(std::uintptr_t address, std::span<std::byte> buffer) {
        if (!is_attached()) {
            return std::unexpected(error_code::process_not_found);
        }
        return m_controller->read_memory(m_attached_pid, address, buffer);
    }

} // namespace core
