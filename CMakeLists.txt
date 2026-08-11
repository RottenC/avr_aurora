cmake_minimum_required(VERSION 3.16)

project(avr_aurora LANGUAGES CXX)

include(CTest)

option(AURORA_BUILD_SIMULATOR "Build AVR Aurora native simulator" OFF)
option(AURORA_BUILD_DESKTOP "Build AVR Aurora desktop simulator" OFF)
option(TuneMode "Build the AVR serial-command tuning firmware" OFF)
if(AURORA_BUILD_DESKTOP)
    set(AURORA_BUILD_SIMULATOR ON)
endif()

add_library(aurora_core STATIC
    src/core/aurora_runtime.cpp
    src/core/portable_math.cpp
    src/core/renderer.cpp
    src/pc_state.cpp
    src/effect_controller.cpp
    src/power_led_tracker.cpp
    src/hdd_activity.cpp
    src/effects/aurora_field.cpp
    src/effects/effects.cpp
)

add_library(aurora::core ALIAS aurora_core)

target_compile_features(aurora_core PUBLIC cxx_std_17)
set_target_properties(aurora_core PROPERTIES CXX_EXTENSIONS OFF)
target_include_directories(aurora_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
)

if(MSVC)
    target_compile_options(aurora_core PRIVATE /W4)
else()
    target_compile_options(aurora_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(AURORA_BUILD_SIMULATOR)
    add_subdirectory(native_simulator)
endif()

if(TuneMode)
    add_library(aurora_tune_command STATIC
        src/tune/tune_command.cpp
    )
    target_compile_features(aurora_tune_command PUBLIC cxx_std_17)
    set_target_properties(aurora_tune_command PROPERTIES CXX_EXTENSIONS OFF)
    target_include_directories(aurora_tune_command
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    if(MSVC)
        target_compile_options(aurora_tune_command PRIVATE /W4)
    else()
        target_compile_options(aurora_tune_command
            PRIVATE -Wall -Wextra -Wpedantic
        )
    endif()

    find_program(AURORA_PLATFORMIO_EXECUTABLE
        NAMES pio platformio
        HINTS
            "$ENV{USERPROFILE}/.platformio/penv/Scripts"
            "$ENV{HOME}/.platformio/penv/bin"
        DOC "PlatformIO executable used for the AVR TuneMode firmware"
    )
    if(AURORA_PLATFORMIO_EXECUTABLE)
        add_custom_target(aurora_tune_firmware ALL
            COMMAND ${AURORA_PLATFORMIO_EXECUTABLE}
                    run --environment pro16MHzatmega328_tune
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            COMMENT "Building AVR Aurora TuneMode firmware"
            USES_TERMINAL
        )
    else()
        message(WARNING
            "TuneMode is enabled, but PlatformIO was not found. "
            "The portable command parser will be built; set "
            "AURORA_PLATFORMIO_EXECUTABLE or use the "
            "pro16MHzatmega328_tune PlatformIO environment to build firmware."
        )
    endif()
endif()

if(BUILD_TESTING)
    add_executable(aurora_core_smoke test/cmake/aurora_core_smoke.cpp)
    target_link_libraries(aurora_core_smoke PRIVATE aurora::core)
    add_test(NAME aurora_core_smoke COMMAND aurora_core_smoke)

    if(TuneMode)
        add_executable(aurora_tune_command_test
            test/cmake/tune_command_test.cpp
        )
        target_link_libraries(aurora_tune_command_test
            PRIVATE aurora_tune_command
        )
        add_test(NAME aurora_tune_command COMMAND aurora_tune_command_test)
    endif()
endif()
