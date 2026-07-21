cmake_minimum_required(VERSION 3.20)

project(stream-torr-src)

set(TORRENT_SOURCES torrent/torrent.cpp)

add_library(${PROJECT_NAME} STATIC ${TORRENT_SOURCES})

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_SOURCE_DIR}/include)

add_executable(stream-torr main.cpp)
target_link_libraries(stream-torr PRIVATE ${PROJECT_NAME})