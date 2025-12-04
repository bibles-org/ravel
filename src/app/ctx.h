#pragma once

#include <core/target.h>
#include <memory>

namespace app {
    extern std::unique_ptr<core::target> active_target;
};
