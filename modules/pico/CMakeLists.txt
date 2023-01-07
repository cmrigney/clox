cmake_minimum_required(VERSION 3.19.2)

include(../../vendor/pico-sdk/pico_sdk_init.cmake)

project(cloxpico)

set(SOURCE_FILES main.c)
add_library(cloxpico ${SOURCE_FILES})

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

target_link_libraries(cloxpico pico_stdlib)

# enable usb output, disable uart output
pico_enable_stdio_usb(cloxpico 1)
pico_enable_stdio_uart(cloxpico 0)

add_custom_target(
 genlib ALL
 COMMAND mkdir -p ${CMAKE_CURRENT_SOURCE_DIR}/autogen && xxd -i -n pico_module_bundle ${CMAKE_CURRENT_SOURCE_DIR}/lib.lox > ${CMAKE_CURRENT_SOURCE_DIR}/autogen/pico_lib.h
 BYPRODUCTS ${CMAKE_CURRENT_SOURCE_DIR}/autogen/pico_lib.h
 COMMENT "Generating Lox Pico Lib"
)

add_dependencies(cloxpico genlib)
