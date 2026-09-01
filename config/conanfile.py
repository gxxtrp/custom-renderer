from conan import ConanFile


class CustomEngineConan(ConanFile):
    name = "custom-engine"
    version = "0.1.0"
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps", "CMakeToolchain"

    def requirements(self):
        self.requires("sdl/3.4.14")
        self.requires("spdlog/1.17.0")
        self.requires("tracy/0.13.1")
        self.requires("fastgltf/0.9.0")
        self.requires("meshoptimizer/0.23")
