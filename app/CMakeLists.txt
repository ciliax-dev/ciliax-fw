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

# Re-expose the toolchain's libstdc++ headers (header-only STL: <array>, <span>,
# <optional>, <type_traits>, ...). Zephyr's MINIMAL_LIBCPP sets -nostdinc++ to
# avoid linking libstdc++; we keep the no-link choice but restore the headers.
# Heap-using containers (std::vector, std::string) compile but fail to link, by
# design.
execute_process(
    COMMAND ${CMAKE_CXX_COMPILER} -dumpmachine
    OUTPUT_VARIABLE _gcc_target
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
get_filename_component(_zephyr_sdk_bin "${CMAKE_CXX_COMPILER}" DIRECTORY)
get_filename_component(_zephyr_sdk_root "${_zephyr_sdk_bin}/.." ABSOLUTE)
file(GLOB _libstdcxx_include_dir
    "${_zephyr_sdk_root}/${_gcc_target}/include/c++/*"
)
if(NOT _libstdcxx_include_dir)
    message(FATAL_ERROR
        "Could not locate libstdc++ headers under "
        "${_zephyr_sdk_root}/${_gcc_target}/include/c++/")
endif()
list(GET _libstdcxx_include_dir 0 _libstdcxx_include_dir)

target_include_directories(app SYSTEM PRIVATE
    ${_libstdcxx_include_dir}
    ${_libstdcxx_include_dir}/${_gcc_target}
    ${_libstdcxx_include_dir}/backward
    ${ZEPHYR_BASE}/../modules/lib/etl/include
)
