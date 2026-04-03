from conan import ConanFile
from conan.tools.cmake import cmake_layout
import configparser


class VideoMonitor(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps", "CMakeToolchain"

    def requirements(self):
        config = configparser.ConfigParser(allow_no_value=True)
        config.read("deps/video-viewer/conanfile.txt")

        for dep in config["requires"]:
            self.requires(dep)

        self.requires("cli11/[>=2.4.2 <3.0]")
        self.requires("spdlog/[>=1.10.0 <2.0]")

    def layout(self):
        cmake_layout(self)
