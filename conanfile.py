from conan import ConanFile
from conan.tools.cmake import CMake, cmake_layout


class EnsensoModuleConan(ConanFile):
    """Conan recipe for Viam Ensenso Camera Module"""
    name = "viam-camera-ensenso"
    version = "0.1.0"

    # Package metadata
    license = "Apache-2.0"
    author = "Your Name"
    url = "https://github.com/yourusername/viam-camera-ensenso"
    description = "Viam module for IDS Ensenso 3D stereo cameras"
    topics = ("robotics", "viam", "camera", "ensenso", "3d")

    # Binary configuration
    settings = "os", "compiler", "build_type", "arch"
    options = {"shared": [True, False], "fPIC": [True, False]}
    default_options = {"shared": False, "fPIC": True}

    # Sources are located in the same place as this recipe
    exports_sources = "CMakeLists.txt", "src/*", "etc/*"

    # Generators
    generators = "CMakeDeps", "CMakeToolchain"

    def requirements(self):
        """Define dependencies"""
        # Viam C++ SDK (latest version from viamconan)
        self.requires("viam-cpp-sdk/0.32.1")

    def system_requirements(self):
        """Define system dependencies"""
        # Note: Ensenso SDK must be installed separately at /opt/ensenso
        pass

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")

    def layout(self):
        cmake_layout(self)

    def build(self):
        """Build the module"""
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        """Package the module"""
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        """Package information"""
        self.cpp_info.libs = ["viam-camera-ensenso"]
