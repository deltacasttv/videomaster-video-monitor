from conan import ConanFile
from conan.tools.cmake import cmake_layout, CMakeDeps, CMakeToolchain
from conan.tools.files import load
import re


class VideoMonitor(ConanFile):
    settings = "os", "compiler", "build_type", "arch"

    def _read_requires(self, conan_txt_path: str) -> list[str]:
        requires = []
        in_requires_section = False

        conanfile_txt = load(self, conan_txt_path)
        for raw_line in conanfile_txt.splitlines():
            line = raw_line.strip()

            if not line or line.startswith("#") or line.startswith(";"):
                continue

            if line.startswith("[") and line.endswith("]"):
                in_requires_section = line.lower() == "[requires]"
                continue

            if in_requires_section:
                line = re.sub(r"\s[;#].*$", "", line).strip()
                if not line:
                    continue
                # Keep requirement lines as-is (e.g. "pkg/[>=1 <2]").
                requires.append(line)

        return requires

    def requirements(self):
        dependencies = self._read_requires("deps/video-viewer/conanfile.txt")

        for dep in dependencies:
            print(f"Adding dependency: {dep} (from deps/video-viewer/conanfile.txt)")
            self.requires(dep)

        self.requires("cli11/[>=2 <3]")
        self.requires("spdlog/[>=1 <2]")
        self.requires("fmt/[>=12 <13]")
        self.requires("ipaddress/[>=1 <2]")

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()
        tc = CMakeToolchain(self)
        tc.generate()

    def layout(self):
        cmake_layout(self)
