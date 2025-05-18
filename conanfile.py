from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMakeDeps, CMake, cmake_layout
import os

class ConanApplication(ConanFile):
    name = "common"
    package_type = "library"
    settings = "os", "compiler", "build_type", "arch"
    options = {"shared": [True, False]}
    default_options = {"shared": False}
    exports_sources = "src/*", "include/*", "CMakeLists.txt", "VERSION.txt"

    def set_version(self):
        version_file = os.path.join(os.path.dirname(__file__), "VERSION.txt")
        self.version = open(version_file).read().strip()

    def requirements(self):
        requirements = self.conan_data.get('requirements', [])
        for requirement in requirements:
            self.requires(requirement)

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()
        tc = CMakeToolchain(self)
        tc.generate()  # ✅ Toolchain

    def layout(self):
        cmake_layout(self)

    def build(self):
        cmake = CMake(self)
        cmake.configure()

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.set_property("cmake_target_name", "common")
        self.cpp_info.includedirs = ["include"]
