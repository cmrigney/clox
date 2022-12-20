cmake_minimum_required(VERSION 3.0.0)
project(clox VERSION 0.1.0)

include(CTest)
enable_testing()

FILE(GLOB MyCSources src/*.c)

add_executable(clox ${MyCSources})

if(DEFINED ENV{EMCC_JS}) 
  target_link_libraries(clox -lnodefs.js)
  add_compile_definitions(EMCC_JS)
endif()

if(DEFINED ENV{WASM_STANDALONE})
  set(CMAKE_C_COMPILER "$ENV{WASI_SDK_PATH}/bin/clang")
  set(CMAKE_SYSROOT "$ENV{WASI_SDK_PATH}/share/wasi-sysroot")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_WASI_EMULATED_PROCESS_CLOCKS")
  target_link_options(clox PRIVATE -lwasi-emulated-process-clocks)
  set_target_properties(clox PROPERTIES SUFFIX ".wasm")
  add_compile_definitions(WASM_STANDALONE)
endif()

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
