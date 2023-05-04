target_sources(clox PRIVATE ${CMAKE_CURRENT_LIST_DIR}/main.cpp)

include(${CMAKE_CURRENT_LIST_DIR}/pimoroni_pico_import.cmake)

add_compile_definitions(PIMORONI_MODULE)

add_custom_target(
 genpimoronilib ALL
 COMMAND mkdir -p ${CMAKE_CURRENT_LIST_DIR}/autogen && xxd -i -n pimoroni_module_bundle ${CMAKE_CURRENT_LIST_DIR}/lib.lox | ${PIPE_CMD} > ${CMAKE_CURRENT_LIST_DIR}/autogen/pimoroni_lib.h
 BYPRODUCTS ${CMAKE_CURRENT_LIST_DIR}/autogen/pimoroni_lib.h
 COMMENT "Generating Lox Pimoroni Lib"
)

# Include required libraries
include(${PIMORONI_PICO_PATH}/common/pimoroni_bus.cmake)
include(${PIMORONI_PICO_PATH}/libraries/pico_graphics/pico_graphics.cmake)
include(${PIMORONI_PICO_PATH}/libraries/pico_display/pico_display.cmake)
include(${PIMORONI_PICO_PATH}/drivers/rgbled/rgbled.cmake)
include(${PIMORONI_PICO_PATH}/drivers/st7789/st7789.cmake)

add_dependencies(clox genpimoronilib)
target_link_libraries(clox rgbled pico_display pico_graphics st7789)

