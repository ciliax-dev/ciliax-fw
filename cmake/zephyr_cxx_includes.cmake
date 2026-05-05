# Adds toolchain libstdc++ headers and ETL to a Zephyr-built target as
# SYSTEM includes.
#
# Why this exists: Zephyr's MINIMAL_LIBCPP profile sets -nostdinc++,
# which strips both the libstdc++ headers and the runtime. We want the
# headers without the runtime — that way header-only STL (<array>,
# <span>, <optional>, <type_traits>, ...) compiles, but heap-using
# containers (std::vector, std::string, std::map, <iostream>) fail at
# link. ETL provides the canonical fixed-capacity container path.
# See docs/cpp_subset.md "How that's wired".
#
# Usage from a Zephyr CMakeLists.txt (after find_package(Zephyr ...)):
#
#     list(APPEND CMAKE_MODULE_PATH <repo-root>/cmake)
#     include(zephyr_cxx_includes)
#     ciliax_add_cxx_includes(app)
#
# The libstdc++ header dir is located by querying the active C++ compiler
# for its target triple (-dumpmachine) and globbing the matching
# c++/<version>/ folder under the Zephyr SDK, so this keeps working
# across SDK versions and target archs.

function(ciliax_add_cxx_includes target)
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
            "ciliax_add_cxx_includes: could not locate libstdc++ headers "
            "under ${_zephyr_sdk_root}/${_gcc_target}/include/c++/")
    endif()
    list(GET _libstdcxx_include_dir 0 _libstdcxx_include_dir)

    target_include_directories(${target} SYSTEM PRIVATE
        ${_libstdcxx_include_dir}
        ${_libstdcxx_include_dir}/${_gcc_target}
        ${_libstdcxx_include_dir}/backward
        ${ZEPHYR_BASE}/../modules/lib/etl/include
    )
endfunction()
