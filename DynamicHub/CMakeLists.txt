cmake_minimum_required(VERSION 3.16)
project(DynamicHub LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_library(dynamic_hub_lib
    src/actuator_profile.cpp
    src/config_parser.cpp
    src/serial_link.cpp
    src/telemetry_router.cpp
    src/game_interfaces/base.cpp
    src/game_interfaces/assetto_corsa.cpp
    src/game_interfaces/assetto_corsa_competizione.cpp
    src/game_interfaces/rfactor2.cpp
)

target_include_directories(dynamic_hub_lib
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

if(UNIX)
    target_link_libraries(dynamic_hub_lib PUBLIC pthread)
endif()

add_executable(dynamic_hub src/main.cpp)
target_link_libraries(dynamic_hub PRIVATE dynamic_hub_lib)
