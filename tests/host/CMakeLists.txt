cmake_minimum_required(VERSION 3.20.0)

# Standalone host build for the project's domain / ports / mock layers.
# Deliberately does NOT call find_package(Zephyr) — these tests run on
# the developer laptop and on CI's plain Ubuntu image, with no NCS
# toolchain involved. That is the entire point: the inner TDD loop is
# host-side and measured in milliseconds.
project(ciliax_fw_host_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Drop a compile_commands.json so clang-tidy / IDEs can pick up the
# real flags. The firmware build's compile_commands.json bakes in
# Docker paths and arm-zephyr-eabi flags; this one is host-clean.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "" FORCE)

set(_warning_flags
    -Wall
    -Wextra
    -Wpedantic
    -Wshadow
    -Wconversion
    -Werror
)

option(ENABLE_SANITIZERS
    "Build host tests with AddressSanitizer + UBSan" OFF)

option(ENABLE_COVERAGE
    "Build host tests with gcov instrumentation (gcc/g++ only)" OFF)

# The two are deliberately mutually exclusive in a single build dir:
# coverage wants -O0 for stable line mapping, sanitizers want their own
# heap shadow, and mixing the two binary layouts produces noisy
# .gcda/.gcno output. Use a separate build-cov/ tree for coverage.
if(ENABLE_SANITIZERS AND ENABLE_COVERAGE)
    message(FATAL_ERROR
        "ENABLE_SANITIZERS and ENABLE_COVERAGE cannot be combined; "
        "configure each into its own build directory.")
endif()

if(ENABLE_SANITIZERS)
    set(_sanitizer_flags
        -fsanitize=address,undefined
        -fno-omit-frame-pointer
        -O1
        -g
    )
else()
    set(_sanitizer_flags "")
endif()

if(ENABLE_COVERAGE)
    set(_coverage_flags --coverage -O0 -g)
else()
    set(_coverage_flags "")
endif()

# Where the code under test lives. domain/, ports/, adapters/mock/ are
# reachable from this build; adapters/zephyr/ deliberately is not.
set(APP_SRC ${CMAKE_SOURCE_DIR}/../../app/src)

# GoogleTest pinned by release tag. Skip install rules and gmock — we
# only need gtest_main, and we never `make install` this build.
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

enable_testing()

# The single host-test binary. Test files (foo_test.cpp) and any
# domain/*.cpp under test are added via target_sources from step 11
# onward — keeping the binary monolithic keeps test discovery and
# the link step trivially fast.
#
# CMake 3.28+ rejects add_executable() without sources, so a
# placeholder TU is generated into the build dir at configure time.
# It contributes nothing — gtest_main provides main() — and goes
# away when the build dir is wiped.
set(_runtests_placeholder ${CMAKE_BINARY_DIR}/_runtests_placeholder.cpp)
file(WRITE ${_runtests_placeholder}
    "// Auto-generated placeholder. Real tests are added via\n"
    "// target_sources(runtests PRIVATE ...) from step 11 onward.\n"
)
add_executable(runtests ${_runtests_placeholder})
target_sources(runtests PRIVATE blinker_test.cpp)
target_sources(runtests PRIVATE ${APP_SRC}/domain/blinker.cpp)
target_sources(runtests PRIVATE event_detector_test.cpp)
target_sources(runtests PRIVATE ${APP_SRC}/domain/event_detector.cpp)
target_include_directories(runtests PRIVATE ${APP_SRC})
target_compile_options(runtests PRIVATE
    ${_warning_flags}
    ${_sanitizer_flags}
    ${_coverage_flags}
)
target_link_options(runtests PRIVATE
    ${_sanitizer_flags}
    ${_coverage_flags}
)
target_link_libraries(runtests PRIVATE gtest_main)

gtest_discover_tests(runtests)
