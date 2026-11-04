# Minimal in-tree libopenmpt ("libopenmpt-small" equivalent from the premake
# build): module rendering with internal minimp3 and stb_vorbis decoders.
# The optional J2B/miniz decoder is omitted; Zuma's MO3 does not need it.
#
# libopenmpt is BSD-3-Clause; see thirdparty/openmpt/LICENSE.

set(OPENMPT_DIR ${PROJECT_SOURCE_DIR}/thirdparty/openmpt)

if(NOT EXISTS ${OPENMPT_DIR}/libopenmpt/libopenmpt.h)
    message(STATUS "thirdparty/openmpt missing: building without open-source audio backend")
    return()
endif()

file(GLOB OPENMPT_SOURCES CONFIGURE_DEPENDS
    ${OPENMPT_DIR}/src/mpt/*.cpp
    ${OPENMPT_DIR}/src/openmpt/*.cpp
    ${OPENMPT_DIR}/common/*.cpp
    ${OPENMPT_DIR}/soundlib/*.cpp
    ${OPENMPT_DIR}/soundlib/plugins/*.cpp
    ${OPENMPT_DIR}/soundlib/plugins/dmo/*.cpp
    ${OPENMPT_DIR}/sounddsp/*.cpp
    ${OPENMPT_DIR}/libopenmpt/libopenmpt_c.cpp
    ${OPENMPT_DIR}/libopenmpt/libopenmpt_cxx.cpp
    ${OPENMPT_DIR}/libopenmpt/libopenmpt_ext_impl.cpp
    ${OPENMPT_DIR}/libopenmpt/libopenmpt_impl.cpp
)

# Subtrees that only the full tracker/player needs.
list(FILTER OPENMPT_SOURCES EXCLUDE REGEX
    "/(crypto|environment|filemode|fs|io_file_atomic|json|library|main|osinfo_hx|osinfo_wine|profiler|terminal|test|textfile|uuid_namespace|sounddevice|soundfile_write|streamencoder)/")

file(GLOB OPENMPT_DECODERS CONFIGURE_DEPENDS
    ${OPENMPT_DIR}/include/minimp3/minimp3.c
    ${OPENMPT_DIR}/include/stb_vorbis/stb_vorbis.c
)

add_library(openmpt_small STATIC ${OPENMPT_SOURCES} ${OPENMPT_DECODERS})

target_include_directories(openmpt_small PUBLIC
    ${OPENMPT_DIR}
    ${OPENMPT_DIR}/include
    ${OPENMPT_DIR}/common
    ${OPENMPT_DIR}/src
    ${OPENMPT_DIR}/libopenmpt
    ${OPENMPT_DIR}/build/svn_version
)

target_compile_features(openmpt_small PUBLIC cxx_std_17)

target_compile_definitions(openmpt_small PUBLIC
    LIBOPENMPT_BUILD
    NOMINMAX
    MPT_WITH_MINIMP3
    MPT_WITH_STBVORBIS
    STB_VORBIS_NO_PULLDATA_API
    STB_VORBIS_NO_STDIO
    # Keep console hosts quiet about the bundled third-party code.
    $<$<CXX_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>
)

# Consumers see declarations only; the implementation is the stb_vorbis.c above,
# which must therefore be compiled without the header-only switch.
set_source_files_properties(${OPENMPT_SOURCES} PROPERTIES
    COMPILE_DEFINITIONS STB_VORBIS_HEADER_ONLY)

if(MSVC)
    # Upstream code base; per-file warnings are not actionable here.
    target_compile_options(openmpt_small PRIVATE /W0)
else()
    target_compile_options(openmpt_small PRIVATE -w)
endif()
