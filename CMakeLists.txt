cmake_minimum_required(VERSION 3.0.0)
project(clox VERSION 0.1.0)

include(CTest)
enable_testing()

FILE(GLOB MyCSources src/*.c)
add_executable(clox ${MyCSources})

if(DEFINED ENV{EMCC}) 
  target_link_libraries(clox -lnodefs.js)
  add_compile_definitions(EMCC)
endif()

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
