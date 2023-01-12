cmake_minimum_required(VERSION 3.19.2)

include(../../vendor/pico-sdk/pico_sdk_init.cmake)

project(cloxpico)

set(SOURCE_FILES main.c)
if(DEFINED ENV{USE_PICO_W})
  set(SOURCE_FILES ${SOURCE_FILES} clox-pico-w.c)
endif()
add_library(cloxpico ${SOURCE_FILES})

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

if(DEFINED ENV{USE_PICO_W})
  message("Building for Pico W")
  target_include_directories(cloxpico PRIVATE
    ${CMAKE_CURRENT_LIST_DIR} # for lwipopts.h
  )
  target_link_libraries(cloxpico pico_cyw43_arch_lwip_threadsafe_background pico_stdlib)
  add_compile_definitions(USE_PICO_W)
else()
  target_link_libraries(cloxpico pico_stdlib)
endif()

# enable usb output, disable uart output
pico_enable_stdio_usb(cloxpico 1)
pico_enable_stdio_uart(cloxpico 0)

set(SED_ARG "s/unsigned char/#include <pico\\/platform.h>\\nconst unsigned char __in_flash()/g")

add_custom_target(
 genlib ALL
 COMMAND mkdir -p ${CMAKE_CURRENT_SOURCE_DIR}/autogen && xxd -i -n pico_module_bundle ${CMAKE_CURRENT_SOURCE_DIR}/lib.lox | sed '${SED_ARG}' > ${CMAKE_CURRENT_SOURCE_DIR}/autogen/pico_lib.h
 BYPRODUCTS ${CMAKE_CURRENT_SOURCE_DIR}/autogen/pico_lib.h
 COMMENT "Generating Lox Pico Lib"
)

add_dependencies(cloxpico genlib)
