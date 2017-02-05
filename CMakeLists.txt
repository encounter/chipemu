cmake_minimum_required(VERSION 3.6)
project(chipemu)

set(CMAKE_CXX_STANDARD 11)

find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIRS})

set(SOURCE_FILES main.c)
add_executable(${PROJECT_NAME} ${SOURCE_FILES})
target_link_libraries(${PROJECT_NAME} ${SDL2_LIBRARIES})