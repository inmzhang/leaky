import glob
import os

import pybind11
from setuptools import Extension, setup

HEADER_FILES = glob.glob("src/**/*.h", recursive=True)


# A CMakeExtension needs a sourcedir instead of a file list.
# The name must be the _single_ output extension from the CMake build.
# If you need multiple extensions, see scikit-build.
class CMakeExtension(Extension):
    def __init__(self, name, sourcedir=""):
        Extension.__init__(
            self, name, sources=[], include_dirs=[pybind11.get_include()]
        )
        self.sourcedir = os.path.abspath(sourcedir)


version = {}
with open("src/leaky/_version.py") as fp:
    exec(fp.read(), version)

with open("README.md", "r", encoding="utf-8") as fh:
    long_description = fh.read()

setup(
    name="Leaky",
    version=version["__version__"],
    author="Yiming Zhang",
    url="https://github.com/inmzhang/leaky",
    description="An implementation of Google's Pauli+ simulator.",
    long_description=long_description,
    long_description_content_type="text/markdown",
    ext_modules=[CMakeExtension("leaky._cpp_leaky")],
    packages=["leaky"],
    package_dir={"leaky": "src/leaky"},
    package_data={"": [*HEADER_FILES, "src/leaky/__init__.pyi", "pyproject.toml"]},
    zip_safe=False,
    extras_require={"test": ["pytest>=6.0"]},
    python_requires=">=3.7",
    install_requires=["stim==1.13.0"],
)
