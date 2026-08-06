cmake_minimum_required(VERSION 3.16)

project(avr_aurora LANGUAGES CXX)

include(CTest)

option(AURORA_BUILD_SIMULATOR "Build AVR Aurora native simulator" OFF)
option(AURORA_BUILD_DESKTOP "Build AVR Aurora desktop simulator" OFF)
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

if(BUILD_TESTING)
    add_executable(aurora_core_smoke test/cmake/aurora_core_smoke.cpp)
    target_link_libraries(aurora_core_smoke PRIVATE aurora::core)
    add_test(NAME aurora_core_smoke COMMAND aurora_core_smoke)
endif()
