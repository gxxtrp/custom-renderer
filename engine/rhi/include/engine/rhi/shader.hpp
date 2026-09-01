#pragma once

#include <expected>
#include <filesystem>
#include <string>
#include <vector>

#include <engine/core/types.hpp>

namespace engine::rhi {

/**
 * @brief Reads a compiled SPIR-V bytecode binary file into an in-memory byte
 * buffer.
 * @param path Filesystem path to the .spv file.
 * @return Byte vector containing SPIR-V bytecode, or an error string on
 * failure.
 */
[[nodiscard]] std::expected<std::vector<core::u8>, std::string>
loadShaderBytecode(const std::filesystem::path &path);

} // namespace engine::rhi
