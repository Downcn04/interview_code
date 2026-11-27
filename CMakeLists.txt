cmake_minimum_required(VERSION 3.19)
project(MeetingAI LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets Multimedia Network WebSockets)

add_executable(MeetingAI
    src/main.cpp
    src/mainwindow.cpp
    src/asrbackend.h
    src/aliyunasr.cpp
    src/aiclient.cpp
    src/config.cpp
)

target_link_libraries(MeetingAI PRIVATE Qt6::Widgets Qt6::Multimedia Qt6::Network Qt6::WebSockets)

install(TARGETS MeetingAI RUNTIME DESTINATION .)
