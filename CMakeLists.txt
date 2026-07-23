cmake_minimum_required(VERSION 3.20)

project(stream-torr LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

enable_testing()

# ==============================================================================
# 1. COMPILER WARNINGS & SANITIZERS INTERFACE
# ==============================================================================
add_library(project_warnings INTERFACE)

# GCC warns when a system include directory (e.g. /usr/local/include) is
# passed explicitly, since that's unsafe for cross-compilation setups.
# It's noisy in a normal desktop build and, more importantly, third-party
# code we don't control (libtorrent, SDL2) triggers it — so it's silenced
# globally rather than fought inside their build systems. Clang doesn't
# know this flag at all, hence the GNU-only guard.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-Wno-poison-system-directories)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(project_warnings INTERFACE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
        -Wshadow
        -Wconversion
        -Wsign-conversion
        -Wcast-align
        -Wdouble-promotion
        -Wformat=2
        -Wold-style-cast
        $<$<CONFIG:Debug>:-g3>
        $<$<CONFIG:Debug>:-fno-omit-frame-pointer>
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
    )
    target_link_options(project_warnings INTERFACE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
    )
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(project_warnings INTERFACE /W4 /WX)
endif()

# ==============================================================================
# 2. EXTERNAL DEPENDENCIES (Boost, Libtorrent, SDL2)
# ==============================================================================
include(FetchContent)
include(ProcessorCount)
ProcessorCount(STREAMTORR_NPROC)
if(STREAMTORR_NPROC EQUAL 0)
    set(STREAMTORR_NPROC 4) # sane fallback if detection fails (e.g. some CI containers)
endif()

# --- Boost Dependency (Required by libtorrent) ---
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 OLD) # keep classic FindBoost.cmake on CMake >= 3.30
endif()
# NOTE: removed `cmake_policy(SET CMP0183 OLD)`. CMP0183 does not correspond to
# a documented CMake policy as of the versions in common use — setting an
# unknown policy makes `cmake_policy(SET ...)` a hard configure-time error.
# If a specific new-CMake-version warning shows up in CI, add the *real*
# policy number here instead of guessing.

find_package(Boost QUIET COMPONENTS system)

# --- Libtorrent Dependency ---
message(STATUS "Configuring libtorrent...")
set(build_examples OFF CACHE BOOL "" FORCE)
set(build_tests OFF CACHE BOOL "" FORCE)
set(build_tools OFF CACHE BOOL "" FORCE)
set(python-bindings OFF CACHE BOOL "" FORCE)
set(deprecated-functions OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    libtorrent
    GIT_REPOSITORY https://github.com/arvidn/libtorrent.git
    GIT_TAG        v2.0.10
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(libtorrent)

# Belt-and-suspenders: whatever the actual origin of these warnings inside
# libtorrent's own source tree, explicitly silence its own compilation
# (`-w`, PRIVATE — doesn't affect consumers of the target). A warning
# inside code we don't own and can't fix shouldn't spam the build log.
if(TARGET torrent-rasterbar)
    target_compile_options(torrent-rasterbar PRIVATE -w)
endif()

# libtorrent's own headers/sources trigger warnings under -Wswitch-default
# and similar strict flags (that's their code, not ours, and out of our
# control). Marking the include dirs SYSTEM tells the compiler to treat
# them as third-party and suppress warnings originating from them,
# regardless of what flags libtorrent's own CMake happens to set.
if(TARGET torrent-rasterbar)
    get_target_property(_lt_includes torrent-rasterbar INTERFACE_INCLUDE_DIRECTORIES)
    if(_lt_includes)
        set_target_properties(torrent-rasterbar PROPERTIES
            INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${_lt_includes}"
        )
    endif()
endif()

# --- SDL2 Dependency ---
message(STATUS "Configuring SDL2...")
set(SDL_TEST OFF CACHE BOOL "" FORCE)
set(SDL_SHARED ON CACHE BOOL "" FORCE)

FetchContent_Declare(
    SDL2
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        release-2.30.4
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(SDL2)

if(TARGET SDL2)
    get_target_property(_sdl2_includes SDL2 INTERFACE_INCLUDE_DIRECTORIES)
    if(_sdl2_includes)
        set_target_properties(SDL2 PROPERTIES
            INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${_sdl2_includes}"
        )
    endif()
endif()

# ==============================================================================
# 3. HYBRID FFMPEG DEPENDENCY (System PkgConfig -> Static Fallback)
# ==============================================================================
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(FFMPEG IMPORTED_TARGET
        libavcodec
        libavformat
        libavutil
        libswscale
        libswresample
    )
endif()

# Create a unified interface target for the rest of the project
add_library(ffmpeg_interface INTERFACE)

if(FFMPEG_FOUND)
    message(STATUS "Found system FFmpeg via PkgConfig.")
    target_link_libraries(ffmpeg_interface INTERFACE PkgConfig::FFMPEG)
else()
    message(STATUS "System FFmpeg not found. Building statically via ExternalProject...")
    include(ExternalProject)

    set(FFMPEG_PREFIX ${CMAKE_CURRENT_BINARY_DIR}/ffmpeg_ext)
    set(FFMPEG_INSTALL_DIR ${FFMPEG_PREFIX}/install)

    if(APPLE)
        set(FFMPEG_SYS_LIBS "-framework CoreVideo -framework Foundation -framework AVFoundation -framework CoreMedia -framework VideoToolbox -framework CoreFoundation -framework AudioToolbox -framework Security -liconv -lz -lbz2")
    else()
        set(FFMPEG_SYS_LIBS "-lm -lz -lpthread -ldl")
    endif()

    ExternalProject_Add(
        ffmpeg_static
        URL https://ffmpeg.org/releases/ffmpeg-7.0.tar.xz
        PREFIX ${FFMPEG_PREFIX}
        CONFIGURE_COMMAND <SOURCE_DIR>/configure
            --prefix=${FFMPEG_INSTALL_DIR}
            --enable-static
            --disable-shared
            --disable-programs
            --disable-doc
            --disable-debug
            --disable-autodetect
            --extra-ldflags=${FFMPEG_SYS_LIBS}
        BUILD_COMMAND make -j${STREAMTORR_NPROC}
        INSTALL_COMMAND make install
        BUILD_BYPRODUCTS
            ${FFMPEG_INSTALL_DIR}/lib/libavcodec.a
            ${FFMPEG_INSTALL_DIR}/lib/libavformat.a
            ${FFMPEG_INSTALL_DIR}/lib/libavutil.a
            ${FFMPEG_INSTALL_DIR}/lib/libswscale.a
            ${FFMPEG_INSTALL_DIR}/lib/libswresample.a
    )

    file(MAKE_DIRECTORY ${FFMPEG_INSTALL_DIR}/include)

    macro(add_ffmpeg_lib LIB_NAME)
        add_library(FFmpeg::${LIB_NAME} STATIC IMPORTED)
        set_target_properties(FFmpeg::${LIB_NAME} PROPERTIES
            IMPORTED_LOCATION ${FFMPEG_INSTALL_DIR}/lib/lib${LIB_NAME}.a
            INTERFACE_INCLUDE_DIRECTORIES ${FFMPEG_INSTALL_DIR}/include
        )
        add_dependencies(FFmpeg::${LIB_NAME} ffmpeg_static)
    endmacro()

    add_ffmpeg_lib(avcodec)
    add_ffmpeg_lib(avformat)
    add_ffmpeg_lib(avutil)
    add_ffmpeg_lib(swscale)
    add_ffmpeg_lib(swresample)

    target_link_libraries(ffmpeg_interface INTERFACE
        FFmpeg::avformat
        FFmpeg::avcodec
        FFmpeg::swscale
        FFmpeg::swresample
        FFmpeg::avutil
    )

    if(APPLE)
        target_link_options(ffmpeg_interface INTERFACE ${FFMPEG_SYS_LIBS})
    else()
        target_link_libraries(ffmpeg_interface INTERFACE m z pthread dl)
    endif()
endif()

# ==============================================================================
# 4. SUBDIRECTORIES
# ==============================================================================
enable_testing()

add_subdirectory(src)
    add_subdirectory(tests)
