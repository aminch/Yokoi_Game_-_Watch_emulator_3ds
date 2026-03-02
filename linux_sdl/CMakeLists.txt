cmake_minimum_required(VERSION 3.16)

project(yokoi_linux_sdl LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# This target is intended for PortMaster/RGDS class devices (RK3568) and
# relies on OpenGL ES 3.0 (GLSL "#version 300 es").

# Minimum external pack content version that this build accepts.
set(YOKOI_ROMPACK_REQUIRED_CONTENT_VERSION "3" CACHE STRING "Minimum required ROM pack content version")

# Repo root from linux_sdl/
set(YOKOI_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")

add_executable(yokoi_sdl
    main.cpp
    png_decode.cpp
    texture_pack_loader.cpp
)

target_compile_definitions(yokoi_sdl PRIVATE
    YOKOI_ROMPACK_REQUIRED_CONTENT_VERSION=${YOKOI_ROMPACK_REQUIRED_CONTENT_VERSION}
)

target_include_directories(yokoi_sdl PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}"
    "${YOKOI_ROOT}/source"
    "${YOKOI_ROOT}/source/std"
    "${YOKOI_ROOT}/source/SM5XX"
    "${YOKOI_ROOT}/android/app/src/main/cpp"
    "${YOKOI_ROOT}/android/app/src/main/cpp/core"
    "${YOKOI_ROOT}/android/app/src/main/cpp/input"
    "${YOKOI_ROOT}/android/app/src/main/cpp/render"
    "${YOKOI_ROOT}/android/app/src/main/cpp/frontend"
)

# Core sources: reuse the same C++ core used by Android, but without Java.
file(GLOB_RECURSE YOKOI_SM5XX_SRC CONFIGURE_DEPENDS
    "${YOKOI_ROOT}/source/SM5XX/*.cpp"
)

file(GLOB_RECURSE YOKOI_STD_SRC CONFIGURE_DEPENDS
    "${YOKOI_ROOT}/source/std/*.cpp"
)

list(FILTER YOKOI_STD_SRC EXCLUDE REGEX ".*[/\\]GW_ROM[/\\].*")
list(FILTER YOKOI_STD_SRC EXCLUDE REGEX ".*[/\\]GW_ROM_RGDS[/\\].*")

target_sources(yokoi_sdl PRIVATE
    ${YOKOI_SM5XX_SRC}
    ${YOKOI_STD_SRC}

    "${YOKOI_ROOT}/android/app/src/main/cpp/core/yokoi_app_flow.cpp"
    "${YOKOI_ROOT}/android/app/src/main/cpp/core/yokoi_audio.cpp"
    "${YOKOI_ROOT}/android/app/src/main/cpp/core/yokoi_cpu_utils.cpp"
    "${YOKOI_ROOT}/android/app/src/main/cpp/core/yokoi_emulation_thread.cpp"
    "${YOKOI_ROOT}/android/app/src/main/cpp/core/yokoi_game_loader.cpp"
    "${YOKOI_ROOT}/android/app/src/main/cpp/core/yokoi_layout.cpp"
    "${YOKOI_ROOT}/android/app/src/main/cpp/core/yokoi_menu_selection.cpp"
    "${YOKOI_ROOT}/android/app/src/main/cpp/core/yokoi_runtime_state.cpp"
    "${YOKOI_ROOT}/android/app/src/main/cpp/core/yokoi_segments_state.cpp"

    "${YOKOI_ROOT}/android/app/src/main/cpp/input/yokoi_controller_state.cpp"
    "${YOKOI_ROOT}/android/app/src/main/cpp/input/yokoi_input_mapping.cpp"

    "${YOKOI_ROOT}/android/app/src/main/cpp/render/yokoi_gl.cpp"

    "${YOKOI_ROOT}/android/app/src/main/cpp/frontend/yokoi_frontend_api.cpp"
)

# libpng (in-memory PNG decode for rompack textures)
find_package(PNG REQUIRED)
target_link_libraries(yokoi_sdl PRIVATE PNG::PNG)

# SDL2 discovery (prefer config package, fallback to pkg-config)
find_package(SDL2 QUIET)
if(SDL2_FOUND)
    target_link_libraries(yokoi_sdl PRIVATE SDL2::SDL2)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 REQUIRED sdl2)
    target_include_directories(yokoi_sdl PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(yokoi_sdl PRIVATE ${SDL2_LIBRARIES})
endif()

# GLES/EGL
find_library(EGL_LIB EGL)
find_library(GLES_LIB GLESv3)
if(NOT EGL_LIB)
    find_library(EGL_LIB EGL REQUIRED)
endif()
if(NOT GLES_LIB)
    # Some distros expose GLESv2 even for ES3 symbols.
    find_library(GLES_LIB GLESv2)
endif()

target_link_libraries(yokoi_sdl PRIVATE
    ${EGL_LIB}
    ${GLES_LIB}
)

if(UNIX AND NOT APPLE)
    target_link_libraries(yokoi_sdl PRIVATE pthread dl m)
endif()
