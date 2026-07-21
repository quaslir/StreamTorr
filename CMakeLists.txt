cmake_minimum_required(VERSION 3.20)

project(stream-torr)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

enable_testing()
include(FetchContent)

# 1. Create the brutal warning interface
add_library(project_warnings INTERFACE)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(project_warnings INTERFACE
        -Wall
        -Wextra
        -Wpedantic
        -Werror             # Treats ALL warnings as hard errors. The build will stop.
        -Wshadow            # Catches local variables shadowing outer variables.
        -Wconversion        # Catches implicit data loss (e.g., uint64_t to int32_t).
        -Wsign-conversion   # Catches mixing signed and unsigned ints (critical for loop logic).
        -Wcast-align        # Warns if you cast pointers in a way that breaks CPU alignment.
        -Wdouble-promotion  # Warns if a float implicitly promotes to double, killing performance.
        -Wformat=2          # Maximum security checks for string formatting.
        -Wold-style-cast    # Forces you to use static_cast/reinterpret_cast, not (int)var.
    )

    # 2. Memory & Undefined Behavior Sanitizers (Only applied in Debug mode)
    # These inject runtime checks to catch segfaults, buffer overflows, and out-of-bounds reads.
    target_compile_options(project_warnings INTERFACE
        $<$<CONFIG:Debug>:-g3>
        $<$<CONFIG:Debug>:-fno-omit-frame-pointer> # Gives you perfect stack traces on crashes
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
    )
    
    # Sanitizers require linker flags too
    target_link_options(project_warnings INTERFACE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
    )
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(project_warnings INTERFACE /W4 /WX)
endif()

# add_subdirectory(external)
add_subdirectory(src)
# add_subdirectory(tests)