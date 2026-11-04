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
add_subdirectory(thirdparty/expat/expat)
add_subdirectory(thirdparty/sdl2)

# project conf
FILE(GLOB_RECURSE SOURCES RELATIVE ${PROJECT_SOURCE_DIR} "src/*.c")

# Audio decodes the pre-rendered music and effects with the vendored,
# public-domain stb_vorbis, so no module player, codec DLL or submodule is needed.
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
    "${sdl2_SOURCE_DIR}/include"

    "${PROJECT_SOURCE_DIR}/thirdparty/stb"
)

target_link_libraries(
    ZumaHD PRIVATE
    SDL2::SDL2
    expat
)

# Developer-only asset regeneration. The game itself stays C-only and needs no
# module player; this optional target decodes content/music/zuma.mo3 again.
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

# copy assets
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/content
    $<TARGET_FILE_DIR:${PROJECT_NAME}>)

# SDL2 links with /NODEFAULTLIB, so MSVC /RTC checks must be off (CMP0184 moved them out of CMAKE_C_FLAGS_DEBUG)
set_target_properties(SDL2 PROPERTIES MSVC_RUNTIME_CHECKS "")
