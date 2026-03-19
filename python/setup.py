import os
import re
import subprocess
import sys
from pathlib import Path

from setuptools import Extension, setup, find_packages
from setuptools.command.build_ext import build_ext

# A CMakeExtension needs a sourcedir instead of a file list.
# The name must be the _exact_ name of the extension, but the sourcedir is the
# root of the CMake project - in our case, the root of the repo (where backend CMakeLists.txt lives).
class CMakeExtension(Extension):
    def __init__(self, name, sourcedir=""):
        super().__init__(name, sources=[])
        self.sourcedir = os.path.abspath(sourcedir)


class CMakeBuild(build_ext):
    def build_extension(self, ext: CMakeExtension):
        # We need to point to the `backend` directory since that holds the CMakeLists.txt
        extdir = os.path.abspath(os.path.dirname(self.get_ext_fullpath(ext.name)))
        
        # Ensure that the extension directory exists
        if not os.path.exists(extdir):
            os.makedirs(extdir)

        # required for auto-detection & inclusion of auxiliary "native" libs
        if not extdir.endswith(os.path.sep):
            extdir += os.path.sep

        # Detect debug/release
        debug = int(os.environ.get("DEBUG", 0)) if self.debug is None else self.debug
        cfg = "Debug" if debug else "Release"

        # CMake lets you override the generator - we need to check this.
        # Can be set with Conda-Build, for example.
        cmake_generator = os.environ.get("CMAKE_GENERATOR", "")

        cmake_args = [
            f"-DCMAKE_LIBRARY_OUTPUT_DIRECTORY={extdir}",
            f"-DPYTHON_EXECUTABLE={sys.executable}",
            f"-DCMAKE_BUILD_TYPE={cfg}",  # not used on MSVC, but no harm
        ]
        
        # Disable CUDA by default for Python packages to avoid MSVC/nvcc conflicts,
        # unless user explicitly opts in.
        enable_cuda = os.environ.get("QUBIT_ENGINE_CUDA", "0")
        if enable_cuda == "1":
            cmake_args.append("-DENABLE_CUDA=ON")
        else:
            cmake_args.append("-DENABLE_CUDA=OFF")
        
        # Disable VCPKG by default for standard pip installs to avoid compiling huge dependencies,
        # users can mount VCPKG_ROOT if they want the full suite.
        # But we DO need to ensure pybind11 is found.
        # We will turn off building the main executable and tests, just build the module.
        # Wait, our CMakeLists.txt builds gRPC/Proto as well. Let's pass a flag to only build python module?
        # Actually gRPC is in vcpkg. For a PyPI wheel, we'd bundle everything.
        
        # Pass any VCPKG toolchain if present in environment
        vcpkg_root = os.environ.get("VCPKG_ROOT")
        if vcpkg_root:
             cmake_args += [f"-DCMAKE_TOOLCHAIN_FILE={vcpkg_root}/scripts/buildsystems/vcpkg.cmake"]
        else:
             # Look in standard locations
             if sys.platform.startswith("win"):
                 if os.path.exists("C:/vcpkg/scripts/buildsystems/vcpkg.cmake"):
                     cmake_args += ["-DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake"]

        build_args = []
        if self.compiler.compiler_type != "msvc":
            # Using Ninja-build since it a) is available as a wheel and b)
            # multithreads automatically. MSVC would require all variables be
            # exported for Ninja to pick it up, which is a little tricky to do.
            # Users can override the generator with CMAKE_GENERATOR in CMake
            # 3.15+.
            if not cmake_generator or cmake_generator == "Ninja":
                try:
                    import ninja
                    ninja_executable_path = Path(ninja.BIN_DIR) / "ninja"
                    cmake_args += [
                        "-GNinja",
                        f"-DCMAKE_MAKE_PROGRAM:FILEPATH={ninja_executable_path}",
                    ]
                except ImportError:
                    pass
        else:
            # Single config generators are handled "normally"
            single_config = any(x in cmake_generator for x in {"NMake", "Ninja"})
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

        built_target = "qubit_engine_module" # Our python module target

        # Set CMAKE_BUILD_PARALLEL_LEVEL to control the parallel build level
        # across all generators.
        if "CMAKE_BUILD_PARALLEL_LEVEL" not in os.environ:
            build_args += ["--parallel", str(os.cpu_count() or 2)]
            
        build_args += ["--target", built_target]
        
        # Add MSVC native parallel build flag AT THE END if using MSVC
        if self.compiler.compiler_type == "msvc" and not single_config:
            build_args += ["--", "/m"]

        build_temp = Path(self.build_temp) / ext.name
        if not build_temp.exists():
            build_temp.mkdir(parents=True)

        backend_dir = os.path.abspath(os.path.join(ext.sourcedir, "..", "backend"))

        try:
             subprocess.check_call(["cmake", backend_dir] + cmake_args, cwd=build_temp)
             subprocess.check_call(["cmake", "--build", "."] + build_args, cwd=build_temp)
        except subprocess.CalledProcessError as e:
             print(f"Build failed with error code: {e.returncode}")
             print(f"CMake Args: {cmake_args}")
             sys.exit(1)


setup(
    name="qubit-engine",
    version="0.2.0",
    author="QubitEngine Core Team",
    description="High-performance C++ Quantum Simulator with CUDA support",
    ext_modules=[CMakeExtension("qubit_engine.core")],
    cmdclass={"build_ext": CMakeBuild},
    packages=find_packages(),
    zip_safe=False,
    python_requires=">=3.8",
    extras_require={
        "qiskit": ["qiskit>=1.0.0"],
        "pennylane": ["pennylane>=0.35.0"],
        "all": ["qiskit>=1.0.0", "pennylane>=0.35.0"]
    },
)
