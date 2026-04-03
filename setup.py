import glob
import os
import re
import shutil
import subprocess
import sys
from shutil import which

from setuptools import Extension, setup, find_packages
from setuptools.command.build_ext import build_ext

# Convert distutils Windows platform specifiers to CMake -A arguments
PLAT_TO_CMAKE = {
    "win32": "Win32",
    "win-amd64": "x64",
    "win-arm32": "ARM",
    "win-arm64": "ARM64",
}

HEADER_FILES = glob.glob("src/**/*.h", recursive=True)


# A CMakeExtension needs a sourcedir instead of a file list.
# The name must be the _single_ output extension from the CMake build.
# If you need multiple extensions, see scikit-build.
class CMakeExtension(Extension):
    def __init__(self, name, sourcedir=""):
        Extension.__init__(self, name, sources=[])
        self.sourcedir = os.path.abspath(sourcedir)


class CMakeBuild(build_ext):
    @staticmethod
    def _read_cmake_cache(cache_path):
        cache = {}
        if not os.path.exists(cache_path):
            return cache
        with open(cache_path, encoding="utf-8") as fh:
            for line in fh:
                line = line.strip()
                if not line or line.startswith("//") or line.startswith("#"):
                    continue
                if ":" not in line or "=" not in line:
                    continue
                key, value = line.split("=", 1)
                cache[key.split(":", 1)[0]] = value
        return cache

    def build_extension(self, ext):
        extdir = os.path.abspath(os.path.dirname(self.get_ext_fullpath(ext.name)))

        # required for auto-detection & inclusion of auxiliary "native" libs
        if not extdir.endswith(os.path.sep):
            extdir += os.path.sep

        debug = int(os.environ.get("DEBUG", 0)) if self.debug is None else self.debug
        cfg = "Debug" if debug else "Release"

        # CMake lets you override the generator - we need to check this.
        # Can be set with Conda-Build, for example.
        cmake_generator = os.environ.get("CMAKE_GENERATOR", "")

        # Set Python_EXECUTABLE instead if you use PYBIND11_FINDPYTHON
        # EXAMPLE_VERSION_INFO shows you how to pass a value into the C++ code
        # from Python.
        cmake_args = [
            f"-DCMAKE_LIBRARY_OUTPUT_DIRECTORY={extdir}",
            f"-DPYTHON_EXECUTABLE={sys.executable}",
            f"-DCMAKE_BUILD_TYPE={cfg}",  # not used on MSVC, but no harm
        ]
        build_args = []
        # Adding CMake arguments set as environment variable
        # (needed e.g. to build for ARM OSx on conda-forge)
        if "CMAKE_ARGS" in os.environ:
            cmake_args += [item for item in os.environ["CMAKE_ARGS"].split(" ") if item]

        # # In this example, we pass in the version to C++. You might not need to.
        # cmake_args += [f"-DPYMATCHING_VERSION_INFO={self.distribution.get_version()}"]

        if self.compiler.compiler_type != "msvc":
            # Using Ninja-build since it a) is available as a wheel and b)
            # multithreads automatically. MSVC would require all variables be
            # exported for Ninja to pick it up, which is a little tricky to do.
            # Users can override the generator with CMAKE_GENERATOR in CMake
            # 3.15+.
            if not cmake_generator or cmake_generator == "Ninja":
                try:
                    import ninja  # noqa: F401

                    ninja_executable_path = os.path.join(ninja.BIN_DIR, "ninja")
                    cmake_args += [
                        "-GNinja",
                        f"-DCMAKE_MAKE_PROGRAM:FILEPATH={ninja_executable_path}",
                    ]
                except ImportError:
                    pass

        else:
            # Single config generators are handled "normally"
            single_config = any(x in cmake_generator for x in {"NMake", "Ninja"})

            # CMake allows an arch-in-generator style for backward compatibility
            contains_arch = any(x in cmake_generator for x in {"ARM", "Win64"})

            # Specify the arch if using MSVC generator, but only if it doesn't
            # contain a backward-compatibility arch spec already in the
            # generator name.
            if not single_config and not contains_arch:
                cmake_args += ["-A", PLAT_TO_CMAKE[self.plat_name]]

            # Multi-config generators have a different way to specify configs
            if not single_config:
                cmake_args += [
                    f"-DCMAKE_LIBRARY_OUTPUT_DIRECTORY_{cfg.upper()}={extdir}"
                ]
                build_args += ["--config", cfg]

        if sys.platform.startswith("darwin"):
            # Cross-compile support for macOS - respect ARCHFLAGS if set
            archs = re.findall(r"-arch (\S+)", os.environ.get("ARCHFLAGS", ""))
            if archs:
                cmake_args += ["-DCMAKE_OSX_ARCHITECTURES={}".format(";".join(archs))]
            else:
                # If archflag not set, use platform.machine() to detect architecture
                import platform

                arch = platform.machine()
                if arch:
                    cmake_args += [f"-DCMAKE_OSX_ARCHITECTURES={platform.machine()}"]

        if (
            sys.platform.startswith("linux")
            and which("gcc-10") is not None
            and which("g++-10") is not None
        ):
            os.environ["CC"] = "gcc-10"
            os.environ["CXX"] = "g++-10"

        # Set CMAKE_BUILD_PARALLEL_LEVEL to control the parallel build level
        # across all generators.
        if "CMAKE_BUILD_PARALLEL_LEVEL" not in os.environ:
            # self.parallel is a Python 3 only way to set parallel jobs by hand
            # using -j in the build_ext call, not supported by pip or PyPA-build.
            if hasattr(self, "parallel") and self.parallel:
                # CMake 3.12+ only.
                build_args += [f"-j{self.parallel}"]

        build_temp = os.path.join(self.build_temp, ext.name)
        os.makedirs(build_temp, exist_ok=True)
        cache = self._read_cmake_cache(os.path.join(build_temp, "CMakeCache.txt"))
        requested_generator = cmake_generator or ("Ninja" if any(arg == "-GNinja" for arg in cmake_args) else "")
        cached_generator = cache.get("CMAKE_GENERATOR", "")
        cached_make_program = cache.get("CMAKE_MAKE_PROGRAM", "")
        cached_python = cache.get("PYTHON_EXECUTABLE", "") or cache.get("_Python_EXECUTABLE", "")
        stale_cache = (
            (requested_generator and cached_generator and cached_generator != requested_generator)
            or (cached_make_program and not os.path.exists(cached_make_program))
            or (cached_python and os.path.realpath(cached_python) != os.path.realpath(sys.executable))
        )
        if stale_cache:
            shutil.rmtree(build_temp)
            os.makedirs(build_temp, exist_ok=True)
        subprocess.check_call(["cmake", "-S", ext.sourcedir, "-B", build_temp] + cmake_args)
        subprocess.check_call(
            ["cmake", "--build", build_temp, "--target", "_cpp_leaky"] + build_args,
        )


version = {}
with open("src/leaky/_version.py") as fp:
    exec(fp.read(), version)

with open("README.md", "r", encoding="utf-8") as fh:
    long_description = fh.read()

setup(
    name="leakysim",
    version=version["__version__"],
    author="Yiming Zhang",
    url="https://github.com/inmzhang/leaky",
    description="An implementation of Google's Pauli+ simulator.",
    long_description=long_description,
    long_description_content_type="text/markdown",
    ext_modules=[CMakeExtension("leaky._cpp_leaky")],
    packages=find_packages("src"),
    package_dir={"": "src"},
    package_data={
        "": [*HEADER_FILES, "src/leaky/__init__.pyi", "pyproject.toml"]
    },
    cmdclass={"build_ext": CMakeBuild},
    zip_safe=False,
    extras_require={"test": ["pytest>=6.0"]},
    python_requires=">=3.9",
    install_requires=["stim==1.15"],
)
