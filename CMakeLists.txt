cmake_minimum_required(VERSION 4.0...4.4)
project(
    ZumaHD
    LANGUAGES C
    VERSION 0.1.0
)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# deps exapt
set(EXPAT_BUILD_TESTS OFF)
set(EXPAT_BUILD_DOCS OFF)
set(EXPAT_BUILD_TOOLS OFF)

# deps sdl
# The vendored SDL2 submodule builds everywhere. Distributions and CI can pass
# -DZUMA_USE_SYSTEM_SDL2=ON to use an installed SDL2 instead of building it.
option(ZUMA_USE_SYSTEM_SDL2 "Link against the system SDL2 instead of the submodule" OFF)

add_subdirectory(thirdparty/expat/expat)
if(ZUMA_USE_SYSTEM_SDL2)
    find_package(SDL2 REQUIRED)
else()
    add_subdirectory(thirdparty/sdl2)
endif()

# project conf
FILE(GLOB_RECURSE SOURCES RELATIVE ${PROJECT_SOURCE_DIR} "src/*.c")

# Audio needs no submodule: music is pre-rendered to Ogg and every clip is
# decoded by the vendored public-domain stb_vorbis.
set(STB_VORBIS ${PROJECT_SOURCE_DIR}/thirdparty/stb/stb_vorbis.c)

add_executable(
    ZumaHD
    ${SOURCES}
    ${STB_VORBIS}
)

target_include_directories(
    ZumaHD PUBLIC

    "${PROJECT_SOURCE_DIR}/src"

    "${expat_SOURCE_DIR}/expat/lib"

    "${PROJECT_SOURCE_DIR}/thirdparty/stb"
)
if(NOT ZUMA_USE_SYSTEM_SDL2)
    target_include_directories(ZumaHD PUBLIC "${sdl2_SOURCE_DIR}/include")
endif()

target_link_libraries(
    ZumaHD PRIVATE
    SDL2::SDL2
    expat
)

# Developer-only asset regeneration. Normal game builds remain C-only and
# require neither a module player nor the optional OpenMPT submodule.
option(ZUMA_BUILD_AUDIO_RENDERER "Build the offline MO3-to-PCM renderer" OFF)
if(ZUMA_BUILD_AUDIO_RENDERER)
    enable_language(CXX)
    include(cmake/libopenmpt.cmake)
    if(NOT TARGET openmpt_small)
        message(FATAL_ERROR "Audio renderer requires thirdparty/openmpt")
    endif()
    add_executable(zuma_render_track tools/render_track.c)
    target_link_libraries(zuma_render_track PRIVATE openmpt_small)
endif()

# Developer-only audio checks. They need no output device and are not part of a
# normal build; CI runs the stream test on both platforms.
option(ZUMA_BUILD_AUDIO_TESTS "Build the offline audio regression tests" OFF)
if(ZUMA_BUILD_AUDIO_TESTS)
    foreach(test audio_stream_test audio_cue_test audio_smoke)
        add_executable(${test} tools/${test}.c ${STB_VORBIS}
            src/global/audio_thin.c)
        target_include_directories(${test} PRIVATE
            "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/thirdparty/stb")
        # The game provides its own main, so SDL must not rename it to SDL_main.
        target_compile_definitions(${test} PRIVATE SDL_MAIN_HANDLED)
        target_link_libraries(${test} PRIVATE SDL2::SDL2)
        # The tests call libm directly (sqrt/isfinite); the game gets it through SDL.
        if(NOT WIN32)
            target_link_libraries(${test} PRIVATE m)
        endif()
    endforeach()
endif()

# copy assets
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/content
    $<TARGET_FILE_DIR:${PROJECT_NAME}>)

# SDL2 links with /NODEFAULTLIB, so MSVC /RTC checks must be off (CMP0184 moved them out of CMAKE_C_FLAGS_DEBUG)
if(NOT ZUMA_USE_SYSTEM_SDL2)
    set_target_properties(SDL2 PROPERTIES MSVC_RUNTIME_CHECKS "")
endif()
