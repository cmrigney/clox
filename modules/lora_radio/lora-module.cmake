target_sources(clox PRIVATE ${CMAKE_CURRENT_LIST_DIR}/lora.c ${CMAKE_CURRENT_LIST_DIR}/rf_95.c)

add_compile_definitions(LORA_MODULE)

add_custom_target(
 genloralib ALL
 COMMAND mkdir -p ${CMAKE_CURRENT_LIST_DIR}/autogen && xxd -i -n lora_module_bundle ${CMAKE_CURRENT_LIST_DIR}/lib.lox | ${PIPE_CMD} > ${CMAKE_CURRENT_LIST_DIR}/autogen/lora_lib.h
 BYPRODUCTS ${CMAKE_CURRENT_LIST_DIR}/autogen/lora_lib.h
 COMMENT "Generating Lox Lora Lib"
)

add_dependencies(clox genloralib)
target_link_libraries(clox hardware_spi)
