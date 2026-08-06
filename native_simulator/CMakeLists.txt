cmake_minimum_required(VERSION 3.24)

project(avr-aurora-native-simulator LANGUAGES CXX)
find_package(SDL2 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
add_executable(aurora_simulator
    src/main.cpp
    src/simulator_app.cpp
    src/simulator_session.cpp
    src/led_view.cpp
)

target_compile_features(aurora_simulator PRIVATE cxx_std_17)
set_target_properties(aurora_simulator PROPERTIES CXX_EXTENSIONS OFF)

target_link_libraries(aurora_simulator
    PRIVATE
        aurora::core
        $<TARGET_NAME_IF_EXISTS:SDL2::SDL2main>
        SDL2::SDL2
        imgui::imgui
)

if(MSVC OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(aurora_simulator PRIVATE /W4)
else()
    target_compile_options(aurora_simulator PRIVATE -Wall -Wextra -Wpedantic)
endif()
