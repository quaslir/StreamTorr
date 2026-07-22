include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.7.1
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(Catch2)

add_executable(streamtorr_tests
    decoder/test_demuxer.cpp
)

target_link_libraries(streamtorr_tests PRIVATE
    stream-torr-src
    Catch2::Catch2WithMain
)


target_compile_definitions(streamtorr_tests PRIVATE
    STREAMTORR_TEST_VIDEO_PATH="${CMAKE_SOURCE_DIR}/assets/test_media/sintel_trailer-1080p.mp4"
)

include(CTest)
include(Catch)
catch_discover_tests(streamtorr_tests)
