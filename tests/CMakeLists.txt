include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.7.1
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(Catch2)

set(TEST_SOURCES
    decoder/test_demuxer.cpp
    decoder/test_video_decoder.cpp
    decoder/test_audio_decoder.cpp
    pipeline/test_pipeline.cpp
    media/test_audio_resampler.cpp
)

include(CTest)
include(Catch)

foreach(TEST_SOURCE IN LISTS TEST_SOURCES)
    get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)

    add_executable(${TEST_NAME}
        ${TEST_SOURCE}
    )

    target_link_libraries(${TEST_NAME} PRIVATE
        stream-torr-src
        Catch2::Catch2WithMain
    )

    target_compile_definitions(${TEST_NAME} PRIVATE
        STREAMTORR_TEST_VIDEO_PATH="${CMAKE_SOURCE_DIR}/assets/test_media/sintel_trailer-1080p.mp4"
    )

    catch_discover_tests(${TEST_NAME})
endforeach()
