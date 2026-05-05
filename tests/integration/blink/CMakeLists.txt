cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(blink_smoke LANGUAGES C CXX)

target_sources(app PRIVATE src/main.c)

# Reach domain/, ports/, and adapters/mock/ for tests that link them in.
# This first integration test doesn't yet, but the include path needs to
# be in place for the next ones.
target_include_directories(app PRIVATE ${CMAKE_SOURCE_DIR}/../../../app/src)

# Mirror the firmware's MINIMAL_LIBCPP-with-headers profile, so any C++
# domain code linked into the test compiles against the same C++ surface
# as the chip build.
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/../../../cmake)
include(zephyr_cxx_includes)
ciliax_add_cxx_includes(app)
