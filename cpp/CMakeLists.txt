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

set(VOSK_DIR "${CMAKE_SOURCE_DIR}/third_party/vosk")

add_executable(MeetingAI
    src/main.cpp
    src/mainwindow.cpp
    src/asrbackend.h
    src/asr.cpp
    src/aliyunasr.cpp
    src/aiclient.cpp
    src/config.cpp
)

target_include_directories(MeetingAI PRIVATE ${VOSK_DIR}/include)
target_link_libraries(MeetingAI PRIVATE Qt6::Widgets Qt6::Multimedia Qt6::Network Qt6::WebSockets)

if (WIN32)
    if (EXISTS "${VOSK_DIR}/lib/libvosk.dll.a")
        target_link_libraries(MeetingAI PRIVATE "${VOSK_DIR}/lib/libvosk.dll.a")
    else()
        target_link_libraries(MeetingAI PRIVATE "${VOSK_DIR}/lib/libvosk.dll")
    endif()
    add_custom_command(TARGET MeetingAI POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${VOSK_DIR}/lib/libvosk.dll" "$<TARGET_FILE_DIR:MeetingAI>/libvosk.dll"
    )
endif()

install(TARGETS MeetingAI RUNTIME DESTINATION .)
