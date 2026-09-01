#include <engine/rhi/shader.hpp>

#include <fstream>

namespace engine::rhi {

std::expected<std::vector<core::u8>, std::string>
loadShaderBytecode(const std::filesystem::path &path) {
  if (!std::filesystem::exists(path)) {
    return std::unexpected("Shader file does not exist: " + path.string());
  }

  if (!std::filesystem::is_regular_file(path)) {
    return std::unexpected("Shader path is not a regular file: " +
                           path.string());
  }

  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file.is_open()) {
    return std::unexpected("Failed to open shader file: " + path.string());
  }

  const auto fileSize = file.tellg();
  if (fileSize <= 0) {
    return std::unexpected("Shader file is empty: " + path.string());
  }

  const auto byteSize = static_cast<core::usize>(fileSize);
  if ((byteSize % 4) != 0) {
    return std::unexpected(
        "Shader bytecode size is not a multiple of 4 bytes (invalid SPIR-V): " +
        path.string());
  }

  std::vector<core::u8> buffer(byteSize);
  file.seekg(0, std::ios::beg);
  file.read(reinterpret_cast<char *>(buffer.data()),
            static_cast<std::streamsize>(byteSize));

  if (!file) {
    return std::unexpected("Failed to read complete shader file contents: " +
                           path.string());
  }

  return buffer;
}

} // namespace engine::rhi
