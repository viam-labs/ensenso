import os
import tarfile
import re
from tempfile import TemporaryDirectory

from conan import ConanFile
from conan.tools.build import check_min_cppstd
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout
from conan.tools.files import copy, load


class ViamEnsenso(ConanFile):
    name = "viam-camera-ensenso"

    license = "Apache-2.0"
    url = "https://github.com/yourusername/viam-camera-ensenso"
    package_type = "application"
    settings = "os", "compiler", "build_type", "arch"
    options = {"with_tests": [True, False]}
    default_options = {
        "with_tests": False,
        "viam-cpp-sdk/*:shared": False
    }

    exports_sources = "CMakeLists.txt", "LICENSE", "src/*", "cmake/*", "etc/meta.json"

    version = "0.1.0"

    def set_version(self):
        content = load(self, "CMakeLists.txt")
        version_match = re.search(r"set\(CMAKE_PROJECT_VERSION (.+)\)", content)
        if version_match:
            self.version = version_match.group(1).strip()

    def validate(self):
        check_min_cppstd(self, 17)

    def requirements(self):
        # Use same Viam SDK version as RealSense (known to work!)
        self.requires("viam-cpp-sdk/0.39.0")
        self.requires("stb/cci.20230920")

    def layout(self):
        cmake_layout(self, src_folder=".")

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["VIAM_ENSENSO_ENABLE_TESTS"] = self.options.with_tests
        tc.generate()

        CMakeDeps(self).generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def deploy(self):
        with TemporaryDirectory(dir=self.deploy_folder) as tmp_dir:
            self.output.debug(f"Creating temporary directory {tmp_dir}")

            self.output.info("Deploying necessary files to module.tar.gz")

            # Copy the main binary to root
            copy(self, "viam-camera-ensenso", src=self.package_folder, dst=tmp_dir)

            # Copy meta.json to root
            copy(self, "meta.json", src=self.package_folder, dst=tmp_dir)

            self.output.info("Creating module.tar.gz")
            with tarfile.open(os.path.join(self.deploy_folder, "module.tar.gz"), "w|gz") as tar:
                tar.add(tmp_dir, arcname=".", recursive=True)

                self.output.info("module.tar.gz contents:")
                for mem in tar.getmembers():
                    self.output.info(mem.name)
