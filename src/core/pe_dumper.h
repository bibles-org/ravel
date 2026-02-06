#pragma once

#include <core/target.h>
#include <expected>
#include <filesystem>
#include <util/expected.h>

namespace core {
    std::expected<void, error_code> dump_pe_image(target* t, const std::filesystem::path& output_path);
}
