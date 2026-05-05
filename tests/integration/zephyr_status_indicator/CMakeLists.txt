cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(zephyr_status_indicator LANGUAGES C CXX)

set(APP_SRC ${CMAKE_SOURCE_DIR}/../../../app/src)

# Pulls the production ZephyrStatusIndicator into a native_sim ztest
# so its set_pattern() mapping and k_work_delayable blink driver can
# be asserted against gpio_emul. led0 is provided by native_sim's
# default DT (gpio_emul-backed); no overlay needed.
target_sources(app PRIVATE
    src/main.c
    src/status_indicator_init.cpp
    ${APP_SRC}/adapters/zephyr/zephyr_status_indicator.cpp
)

target_include_directories(app PRIVATE ${APP_SRC})

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/../../../cmake)
include(zephyr_cxx_includes)
ciliax_add_cxx_includes(app)
