cmake_minimum_required(VERSION 3.12)

set(PICO_BOARD $ENV{PICO_BOARD})
set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})
include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(example)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_subdirectory(.. build)

add_executable(example main.c)

target_link_libraries(example pico_stdlib pico-usbnet)
pico_add_extra_outputs(example)

# TinyUSB is used directly for NCM; a second USB CDC interface would conflict
pico_enable_stdio_uart(example 1)
pico_enable_stdio_usb(example 0)
