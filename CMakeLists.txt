cmake_minimum_required(VERSION 3.0.0)
project(clox VERSION 0.1.0)

include(CTest)
enable_testing()

FILE(GLOB MyCSources src/*.c src/native/*.c)
add_executable(clox ${MyCSources})

target_include_directories(clox PRIVATE src)

target_compile_options(clox PRIVATE -Werror)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
