# pico_usbnet_tinyusb.cmake
#
# Include this BEFORE pico_sdk_init() so the Pico SDK builds against the TinyUSB
# you intend:
#
#     project(your_project)
#     include(${path/to/pico-usbnet}/pico_usbnet_tinyusb.cmake)
#     pico_sdk_init()
#     add_subdirectory(pico-usbnet)
#
# Behavior:
#   * If PICO_TINYUSB_PATH is already set (e.g. your project vendors its own
#     copy, or you want to build offline against a local checkout), it is used
#     as-is and nothing is fetched.
#   * Otherwise TinyUSB 0.21.0 is cloned into the build tree (_deps/) so you do
#     not have to hand-update the Pico SDK's bundled 0.18.0. The clone happens
#     once at configure time and is cached; later configures are offline.
#
# TinyUSB 0.21.0 is required for reliable CDC-NCM and is the version pico-usbnet's
# RP2040 Errata-15 patch (see patches/) is based on.

if (NOT PICO_TINYUSB_PATH)
    set(_pwn_tusb_dir ${CMAKE_BINARY_DIR}/_deps/tinyusb-0.21.0)
    if (NOT EXISTS ${_pwn_tusb_dir}/src/tusb.h)
        find_package(Git QUIET)
        if (NOT GIT_EXECUTABLE)
            message(FATAL_ERROR
                "pico-usbnet needs TinyUSB 0.21.0 but 'git' was not found, so it could not "
                "fetch one. Install git, or set PICO_TINYUSB_PATH to a local TinyUSB 0.21.0 "
                "checkout to build offline.")
        endif()
        message(STATUS "pico-usbnet: fetching TinyUSB 0.21.0 into ${_pwn_tusb_dir} ...")
        execute_process(
            COMMAND ${GIT_EXECUTABLE} clone --depth 1 --branch 0.21.0
                    https://github.com/hathach/tinyusb.git ${_pwn_tusb_dir}
            RESULT_VARIABLE _pwn_git_rc
            OUTPUT_QUIET
            ERROR_QUIET)
        if (NOT _pwn_git_rc EQUAL 0)
            file(REMOVE_RECURSE ${_pwn_tusb_dir})
            message(FATAL_ERROR
                "pico-usbnet failed to fetch TinyUSB 0.21.0 (git clone error ${_pwn_git_rc}). "
                "Check your network connection, or set PICO_TINYUSB_PATH to a local TinyUSB "
                "0.21.0 checkout to build offline.")
        endif()
    endif()
    set(PICO_TINYUSB_PATH ${_pwn_tusb_dir})
    message(STATUS "pico-usbnet: using TinyUSB 0.21.0 at ${PICO_TINYUSB_PATH}")
endif()
