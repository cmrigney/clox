cmake_minimum_required(VERSION 3.19.2)
project(clox VERSION 0.1.0)

include(CTest)
enable_testing()

FILE(GLOB MyCSources src/*.c src/native/*.c)
add_executable(clox ${MyCSources})

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

if("$ENV{BUNDLE}" STREQUAL "true")
  add_compile_definitions(BUNDLE)
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
