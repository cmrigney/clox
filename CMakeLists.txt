cmake_minimum_required(VERSION 3.19.2)

if("$ENV{MODULES}" MATCHES "pico")
  include(vendor/pico-sdk/pico_sdk_init.cmake)
endif()

project(clox VERSION 0.1.0)

include(CTest)
enable_testing()

add_executable(clox)

FILE(GLOB MyCSources src/*.c src/native/*.c)
target_sources(clox PRIVATE ${MyCSources})

target_include_directories(clox PRIVATE src vendor autogen modules)

target_compile_options(clox PRIVATE -Werror)

if("$ENV{MODULES}" MATCHES "filesystem")
  find_library(FILESYSTEM_MODULE NAMES libcloxfilesystem.a HINTS "modules/filesystem")
  target_link_libraries(clox ${FILESYSTEM_MODULE})
  add_compile_definitions(FILESYSTEM_MODULE)
endif()

if("$ENV{MODULES}" MATCHES "os")
  find_library(OS_MODULE NAMES libcloxos.a HINTS "modules/os")
  target_link_libraries(clox ${OS_MODULE})
  add_compile_definitions(OS_MODULE)
endif()

set(PIPE_CMD "cat") # pass through

if("$ENV{MODULES}" MATCHES "pico")
  set(PIPE_CMD sed "'s/unsigned char/#include <pico\\/platform.h>\\nconst unsigned char __in_flash()/g'")
  include(modules/pico/pico-module.cmake)
  if("$ENV{MODULES}" MATCHES "lora") # only supported for pico currently
    include(modules/lora_radio/lora-module.cmake)
  endif()
endif()


if(DEFINED ENV{EMCC_JS}) 
  target_link_libraries(clox -lnodefs.js)
  add_compile_definitions(EMCC_JS WASM)
endif()

if(DEFINED ENV{WASM_STANDALONE})
  set(CMAKE_C_COMPILER "$ENV{WASI_SDK_PATH}/bin/clang")
  set(CMAKE_SYSROOT "$ENV{WASI_SDK_PATH}/share/wasi-sysroot")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_WASI_EMULATED_PROCESS_CLOCKS")
  target_link_options(clox PRIVATE -lwasi-emulated-process-clocks)
  set_target_properties(clox PROPERTIES SUFFIX ".wasm")
  add_compile_definitions(WASM_STANDALONE WASM)
endif()

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)

add_custom_target(
  genstdlib ALL
  COMMAND mkdir -p ${CMAKE_CURRENT_SOURCE_DIR}/autogen && xxd -i -n stdlib_lib_lox ${CMAKE_CURRENT_SOURCE_DIR}/stdlib/lib.lox | ${PIPE_CMD} > ${CMAKE_CURRENT_SOURCE_DIR}/autogen/stdlib_lox.h
  BYPRODUCTS ${CMAKE_CURRENT_SOURCE_DIR}/autogen/stdlib_lox.h
  COMMENT "Generating Std Lox Lib"
)

add_dependencies(clox genstdlib)

if(DEFINED LOX_BUNDLE)
  add_custom_target(
    genbundle ALL
    COMMAND mkdir -p ${CMAKE_CURRENT_SOURCE_DIR}/autogen && xxd -i -n exec_bundle ${CMAKE_CURRENT_SOURCE_DIR}/${LOX_BUNDLE} | ${PIPE_CMD} > ${CMAKE_CURRENT_SOURCE_DIR}/autogen/bundle_lox.h
    BYPRODUCTS ${CMAKE_CURRENT_SOURCE_DIR}/autogen/bundle_lox.h
    COMMENT "Generating Lox Bundle"
  )
  add_dependencies(clox genbundle)
  add_compile_definitions(BUNDLE)
else()
  add_custom_target(
    cleanbundle ALL
    COMMAND rm -f ${CMAKE_CURRENT_SOURCE_DIR}/autogen/bundle_lox.h
    BYPRODUCTS ${CMAKE_CURRENT_SOURCE_DIR}/autogen/bundle_lox.h
    COMMENT "Cleaning up Lox Bundle"
  )
  add_dependencies(clox cleanbundle)
endif()
