
cmake_minimum_required(VERSION 3.20)

set(STREAMTORR_LIB_NAME stream-torr-src)

set(TORRENT_SOURCES torrent/torrent.cpp)

add_library(${STREAMTORR_LIB_NAME} STATIC ${TORRENT_SOURCES})

target_include_directories(${STREAMTORR_LIB_NAME} PUBLIC ${CMAKE_SOURCE_DIR}/include)

target_link_libraries(${STREAMTORR_LIB_NAME} PUBLIC
    project_warnings
    torrent-rasterbar
    SDL2::SDL2
    ffmpeg_interface
)

add_executable(stream-torr main.cpp)
target_link_libraries(stream-torr PRIVATE ${STREAMTORR_LIB_NAME})