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

if(MSVC)
    target_compile_options(aurora_simulator PRIVATE /W4)
else()
    target_compile_options(aurora_simulator PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(BUILD_TESTING)
    add_executable(aurora_simulator_session_test
        test/simulator_session_test.cpp
        src/simulator_session.cpp
    )
    target_compile_features(aurora_simulator_session_test PRIVATE cxx_std_17)
    set_target_properties(aurora_simulator_session_test
        PROPERTIES CXX_EXTENSIONS OFF
    )
    target_include_directories(aurora_simulator_session_test
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_link_libraries(aurora_simulator_session_test PRIVATE aurora::core)
    if(MSVC)
        target_compile_options(aurora_simulator_session_test PRIVATE /W4)
    else()
        target_compile_options(aurora_simulator_session_test
            PRIVATE -Wall -Wextra -Wpedantic
        )
    endif()
    add_test(NAME aurora_simulator_session
             COMMAND aurora_simulator_session_test)
endif()
