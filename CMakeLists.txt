# The CMakeLists.txt is heavily inspired by PyMatching
# See https://github.com/oscarhiggott/PyMatching/blob/master/CMakeLists.txt
cmake_minimum_required(VERSION 3.13)
project(leaky)

include_directories(src)
set(CMAKE_CXX_STANDARD 20 CACHE STRING "C++ version selection")
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(SIMD_WIDTH 128)

if (NOT(MSVC))
    if (CMAKE_SYSTEM_PROCESSOR MATCHES x86_64)
         set(ARCH_OPT "-O3" "-mno-avx2")
    else ()
         set(ARCH_OPT "-O3")
    endif ()
else ()
    set(ARCH_OPT "-O2")
    # https://stackoverflow.com/a/8591946 (RTC1 incompatible with O2 flag)
    STRING (REGEX REPLACE "/RTC(su|[1su])" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
endif ()

include(FetchContent)
FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG release-1.12.1
)

FetchContent_MakeAvailable(googletest)

FetchContent_Declare(stim
        GIT_REPOSITORY https://github.com/quantumlib/stim.git
        GIT_TAG b01e42391583d03db4266b387d907eda1d7ae488
)
FetchContent_MakeAvailable(stim)
if (NOT (MSVC))
    target_compile_options(libstim PRIVATE -fno-strict-aliasing -fPIC ${ARCH_OPT})
else ()
    target_compile_options(libstim PRIVATE -fPIC ${ARCH_OPT})
endif ()

set(SOURCE_FILES_NO_MAIN
        src/leaky/core/rand_gen.cc
        src/leaky/core/channel.cc
        src/leaky/core/simulator.cc
        )

set(TEST_FILES
        src/leaky/core/channel_test.cc
        src/leaky/core/simulator_test.cc
        )

set(PYTHON_API_FILES
        src/leaky/core/channel.pybind.cc
        src/leaky/core/simulator.pybind.cc
        src/leaky/core/rand_gen.pybind.cc
        src/leaky/leaky.pybind.cc
        ${stim_SOURCE_DIR}/src/stim/circuit/gate_target.pybind.cc
        ${stim_SOURCE_DIR}/src/stim/circuit/circuit_instruction.pybind.cc
        ${stim_SOURCE_DIR}/src/stim/py/base.pybind.h
        )


enable_testing()

add_executable(leaky_tests ${SOURCE_FILES_NO_MAIN} ${TEST_FILES})
target_compile_options(leaky_tests PRIVATE -fsanitize=address -g -fsanitize=undefined -fno-omit-frame-pointer -coverage)
if (NOT (MSVC))
    target_link_options(leaky_tests PRIVATE -pthread -fsanitize=address -fsanitize=undefined -coverage)
else ()
    target_link_options(leaky_tests PRIVATE -fsanitize=address -fsanitize=undefined -coverage)
endif ()
target_link_libraries(leaky_tests GTest::gtest_main GTest::gmock_main libstim)

add_library(libleaky ${SOURCE_FILES_NO_MAIN})
set_target_properties(libleaky PROPERTIES PREFIX "")
target_include_directories(libleaky PUBLIC src)
target_compile_options(libleaky PRIVATE ${ARCH_OPT} -fPIC)
if(NOT(MSVC))
    target_link_options(libleaky PRIVATE -pthread -O3)
endif()
target_link_libraries(libleaky libstim)
install(TARGETS libleaky LIBRARY DESTINATION)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/src/" DESTINATION "include" FILES_MATCHING PATTERN "*.h" PATTERN "*.inl")

include(GoogleTest)
gtest_discover_tests(leaky_tests)


find_package(Python COMPONENTS Interpreter Development)
find_package(pybind11 CONFIG)
if (${pybind11_FOUND} AND ${Python_FOUND})
    # In order to use `stim::PyCircuitInstruction`
    # we need to compile with some stim's python api files
    pybind11_add_module(_cpp_leaky ${PYTHON_API_FILES} ${SOURCE_FILES_NO_MAIN})
    target_link_libraries(_cpp_leaky PRIVATE libstim)
    target_compile_options(_cpp_leaky PRIVATE ${ARCH_OPT})
else()
  message("WARNING: Skipped stim_python_bindings target. `pybind11` not found. To fix, install pybind11. On debian based distributions, the package name is `pybind11-dev`")
endif()