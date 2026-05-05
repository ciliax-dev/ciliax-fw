cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(my_project LANGUAGES C CXX)

target_sources(app PRIVATE src/main.cpp)

# Source files in domain/ and adapters/zephyr/ are picked up via glob.
# Order is irrelevant for these layers (no link-order-sensitive globals);
# globbing keeps the CMakeLists from churning on every new file. Domain and
# port code that lives in headers (typical) doesn't need to be listed here.
file(GLOB _domain_sources CONFIGURE_DEPENDS src/domain/*.cpp)
file(GLOB _zephyr_adapter_sources CONFIGURE_DEPENDS src/adapters/zephyr/*.cpp)
target_sources(app PRIVATE ${_domain_sources} ${_zephyr_adapter_sources})

# Lets domain/adapter code use repo-rooted includes:
#   #include "ports/i_led.hpp"
#   #include "domain/blinker.hpp"
target_include_directories(app PRIVATE src)

# Restore header-only STL and ETL on top of Zephyr's MINIMAL_LIBCPP.
# See cmake/zephyr_cxx_includes.cmake and docs/cpp_subset.md.
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/../cmake)
include(zephyr_cxx_includes)
ciliax_add_cxx_includes(app)
