cmake_minimum_required(VERSION 3.29)

project(arkin_shader)

# Arkin
add_subdirectory("${CMAKE_SOURCE_DIR}/libs/arkin")

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED true)
set(CMAKE_EXPORT_COMPILE_COMMANDS true)
add_compile_options("-Wall" "-Wextra")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")
set(CMAKE_BUILD_TYPE Debug)

set(SOURCES
    src/main.c
)

add_executable(${CMAKE_PROJECT_NAME} ${SOURCES})
target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC "${CMAKE_SOURCE_DIR}/include")
target_link_libraries(${CMAKE_PROJECT_NAME} arkin)
