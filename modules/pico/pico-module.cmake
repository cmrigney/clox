target_sources(clox PRIVATE ${CMAKE_CURRENT_LIST_DIR}/main.c ${CMAKE_CURRENT_LIST_DIR}/machine.c)
if(DEFINED ENV{USE_PICO_W})
  target_sources(clox PRIVATE ${CMAKE_CURRENT_LIST_DIR}/clox-pico-w.c)
endif()

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

add_compile_definitions(PICO_MODULE)

if(DEFINED ENV{USE_PICO_W})
  message("Building for Pico W")
  target_include_directories(clox PRIVATE
    ${CMAKE_CURRENT_LIST_DIR} # for lwipopts.h
  )
  target_link_libraries(clox pico_cyw43_arch_lwip_poll pico_stdlib)
  add_compile_definitions(USE_PICO_W)
else()
  target_link_libraries(clox pico_stdlib)
endif()

# enable usb output, disable uart output
pico_enable_stdio_usb(clox 0)
pico_enable_stdio_uart(clox 1)

# for uf2 file
pico_add_extra_outputs(clox)

add_custom_target(
 genlib ALL
 COMMAND mkdir -p ${CMAKE_CURRENT_LIST_DIR}/autogen && xxd -i -n pico_module_bundle ${CMAKE_CURRENT_LIST_DIR}/lib.lox | ${PIPE_CMD} > ${CMAKE_CURRENT_LIST_DIR}/autogen/pico_lib.h
 BYPRODUCTS ${CMAKE_CURRENT_LIST_DIR}/autogen/pico_lib.h
 COMMENT "Generating Lox Pico Lib"
)

add_dependencies(clox genlib)
