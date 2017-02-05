cmake_minimum_required(VERSION 3.6)
project(chipemu)

set(CMAKE_C_STANDARD 11)

if (NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    # Strip debug symbols
    SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s")

    if (WIN32)
        # Static link MSYS2 dependencies and disable console window
        SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static -mwindows")
    endif ()
endif ()

find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIRS})

set(SOURCE_FILES main.c)
add_executable(${PROJECT_NAME} ${SOURCE_FILES})
target_link_libraries(${PROJECT_NAME} ${SDL2_LIBRARIES})