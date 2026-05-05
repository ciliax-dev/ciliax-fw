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
# Header dirs are extracted by asking the active C++ compiler what its
# include search paths would be (`-x c++ -E -v -` reads from /dev/null
# and the verbose stderr lists the search path). This works for both
# the cross compiler used for the firmware build (arm-zephyr-eabi-g++)
# and the host compiler used by native_sim ztests, regardless of where
# either keeps libstdc++ on disk.

function(ciliax_add_cxx_includes target)
    execute_process(
        COMMAND ${CMAKE_CXX_COMPILER} -x c++ -E -v -
        INPUT_FILE /dev/null
        OUTPUT_VARIABLE _cpp_stdout
        ERROR_VARIABLE _cpp_stderr
        RESULT_VARIABLE _cpp_status
    )
    if(NOT _cpp_status EQUAL 0)
        message(FATAL_ERROR
            "ciliax_add_cxx_includes: ${CMAKE_CXX_COMPILER} -x c++ -E -v - "
            "exited with ${_cpp_status}:\n${_cpp_stderr}")
    endif()

    # Pull each ` /some/path/c++/...` line out of the verbose stderr
    # ("ignoring duplicate ..." messages start with a non-space and are
    # filtered by the leading-space anchor).
    set(_libstdcxx_dirs "")
    string(REPLACE "\n" ";" _cpp_lines "${_cpp_stderr}")
    foreach(_line IN LISTS _cpp_lines)
        if(_line MATCHES "^ +(.+/c\\+\\+/.*)$")
            list(APPEND _libstdcxx_dirs ${CMAKE_MATCH_1})
        endif()
    endforeach()

    if(NOT _libstdcxx_dirs)
        message(FATAL_ERROR
            "ciliax_add_cxx_includes: no libstdc++ include paths found in "
            "${CMAKE_CXX_COMPILER}'s search path. Stderr was:\n${_cpp_stderr}")
    endif()

    target_include_directories(${target} SYSTEM PRIVATE
        ${_libstdcxx_dirs}
        ${ZEPHYR_BASE}/../modules/lib/etl/include
    )
endfunction()
