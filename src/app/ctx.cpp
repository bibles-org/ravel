#include <app/ctx.h>
#include <memory>

namespace app {
    std::unique_ptr<core::target> active_target = nullptr;
    std::unique_ptr<core::analysis::strings_analyzer> strings = std::make_unique<core::analysis::strings_analyzer>();
} // namespace app
