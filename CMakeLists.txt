cmake_minimum_required(VERSION 3.6)
project(chipemu)

set(CMAKE_C_STANDARD 11)

if (EMSCRIPTEN)
    set(CMAKE_EXECUTABLE_SUFFIX ".html")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -s USE_SDL=2")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s USE_SDL=2")
else ()
    message(" * C++ compiler: ${CMAKE_CXX_COMPILER_ID}")
endif ()

if (NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    if (EMSCRIPTEN)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -O2")
    else ()
        # Strip debug symbols
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s")
    endif ()

    if (WIN32)
        # Static link MSYS2 dependencies and disable console window
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static -mwindows")
    endif ()
endif ()

if (NOT EMSCRIPTEN)
    find_package(SDL2 REQUIRED)
    include_directories(${SDL2_INCLUDE_DIRS})
endif ()

set(SOURCE_FILES main.c)
add_executable(${PROJECT_NAME} ${SOURCE_FILES})
if (EMSCRIPTEN)
    set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS
            "--preload-file ${CMAKE_SOURCE_DIR}/games@games --shell-file ${CMAKE_SOURCE_DIR}/shell.html " +
            "-s EXPORTED_FUNCTIONS=\"['_main', '_setCurrentRom', '_setPaused']\"")
elseif (WIN32)
    target_link_libraries(${PROJECT_NAME} ${SDL2_LIBRARIES} Ws2_32)
else ()
    target_link_libraries(${PROJECT_NAME} ${SDL2_LIBRARIES})
endif ()