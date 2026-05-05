cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(event_flow LANGUAGES C CXX)

set(APP_SRC ${CMAKE_SOURCE_DIR}/../../../app/src)

# Self-contained integration test: domain + mock adapters compiled
# under the firmware's MINIMAL_LIBCPP profile, exposed to the C ztest
# entrypoint via extern "C" wrappers. The Zephyr adapters live behind
# Step 9's hardware smoke test and the zephyr_status_indicator/ ztest
# suite next door — they are deliberately NOT pulled in here.
target_sources(app PRIVATE
    src/main.c
    src/event_flow_init.cpp
    ${APP_SRC}/domain/event_detector.cpp
)

target_include_directories(app PRIVATE ${APP_SRC})

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/../../../cmake)
include(zephyr_cxx_includes)
ciliax_add_cxx_includes(app)
