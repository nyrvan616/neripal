cmake_minimum_required(VERSION 3.20)
project(NeriPal VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(neripal_core STATIC src/core/Pet.cpp)
target_include_directories(neripal_core PUBLIC include)

add_library(neripal_ui STATIC src/ui/PetView.cpp)
target_include_directories(neripal_ui PUBLIC include)
target_link_libraries(neripal_ui PUBLIC neripal_core)

if(MSVC)
    target_compile_options(neripal_core PRIVATE /W4 /permissive-)
    target_compile_options(neripal_ui PRIVATE /W4 /permissive-)
else()
    target_compile_options(neripal_core PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(neripal_ui PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(WIN32)
    # Console subsystem is intentional: runtime/version diagnostics remain visible
    # while DesktopPlatform owns the separate 240x240 game window.
    add_executable(neripal_simulator
        simulator/main.cpp
        simulator/debug/DebugController.cpp
        src/platform/desktop/DesktopPlatform.cpp
        src/platform/desktop/ScaledClock.cpp
    )
    target_include_directories(neripal_simulator PRIVATE include src simulator/debug)
    target_link_libraries(neripal_simulator PRIVATE neripal_ui user32 gdi32)
    if(NOT MSVC)
        target_link_options(neripal_simulator PRIVATE -static -static-libgcc -static-libstdc++)
    endif()
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(neripal_core_tests tests/unit/core/PetTests.cpp)
    target_include_directories(neripal_core_tests PRIVATE include tests/unit/core)
    target_link_libraries(neripal_core_tests PRIVATE neripal_core)
    if(NOT MSVC)
        target_link_options(neripal_core_tests PRIVATE -static -static-libgcc -static-libstdc++)
    endif()
    add_test(NAME core.unit COMMAND neripal_core_tests)
endif()
