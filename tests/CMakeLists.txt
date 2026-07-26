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

set(STREAMTORR_TEST_VIDEO_PATH "${CMAKE_SOURCE_DIR}/assets/test_media/sintel_trailer-1080p.mp4")

if(NOT EXISTS ${STREAMTORR_TEST_VIDEO_PATH})
    message(STATUS "Downloading test video...")
    file(DOWNLOAD
        "https://download.blender.org/durian/trailer/sintel_trailer-1080p.mp4"
        "${STREAMTORR_TEST_VIDEO_PATH}"
        SHOW_PROGRESS
        STATUS download_status
        TLS_VERIFY ON
    )
    list(GET download_status 0 download_code)
    list(GET download_status 1 download_message)
    if(NOT download_code EQUAL 0)
        message(FATAL_ERROR "Failed to download test video: code=${download_code}, message=${download_message}")
    endif()
endif()

if(EXISTS "${STREAMTORR_TEST_VIDEO_PATH}")
    file(SIZE "${STREAMTORR_TEST_VIDEO_PATH}" downloaded_size)
    message(STATUS "Test video size: ${downloaded_size} bytes")
    if(downloaded_size LESS 1000000)
        message(FATAL_ERROR "Test video is suspiciously small (${downloaded_size} bytes)")
    endif()
else()
    message(FATAL_ERROR "Test video does not exist at ${STREAMTORR_TEST_VIDEO_PATH} after download attempt")
endif()


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
