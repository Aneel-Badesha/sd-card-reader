cmake_minimum_required(VERSION 3.20)
project(host_tests C)

include(FetchContent)
FetchContent_Declare(
    unity
    GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
    GIT_TAG        v2.6.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(unity)

add_executable(host_tests
    run_tests.c
    test_sdcard.c
    test_thumbstick.c
    test_button.c
    stubs/stubs.c
    ../components/sdcard/sdcard.c
    ../components/thumbstick/thumbstick.c
    ../components/button/button.c
)

# stubs/ must come first so it shadows any system headers with the same name
target_include_directories(host_tests BEFORE PRIVATE
    stubs
    ../components/sdcard/include
    ../components/thumbstick/include
    ../components/button/include
)

target_link_libraries(host_tests PRIVATE unity)

# Treat warnings as errors to catch stub gaps early
target_compile_options(host_tests PRIVATE -Wall -Wextra -Werror
    -Wno-unused-parameter
    -Wno-missing-field-initializers
)
