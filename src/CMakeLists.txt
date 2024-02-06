cmake_minimum_required(VERSION 3.16)
include(FetchContent)

configure_file(
    ${CMAKE_SOURCE_DIR}/cmake/version.h.in
    ${CMAKE_SOURCE_DIR}/src/version.h
    @ONLY
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
endif()

# project sources
set(${PROJECT_NAME}_SOURCES
    ${CMAKE_SOURCE_DIR}/src/main.cpp
    ${CMAKE_SOURCE_DIR}/src/device.cpp
    ${CMAKE_SOURCE_DIR}/src/rx_renderer.cpp
    ${CMAKE_SOURCE_DIR}/src/rx_stream.cpp
    ${CMAKE_SOURCE_DIR}/src/shared_resources.cpp
    ${CMAKE_SOURCE_DIR}/src/signal_information.cpp
)

add_executable(${PROJECT_NAME} ${${PROJECT_NAME}_SOURCES})

# dependencies
FetchContent_Declare(
  video-viewer
  GIT_REPOSITORY    https://github.com/deltacasttv/video-viewer.git
  GIT_TAG           fix/check_macros_gcc
  SOURCE_DIR        "${CMAKE_SOURCE_DIR}/deps/video-viewer"
)

FetchContent_Declare(
    cli11
    GIT_REPOSITORY  https://github.com/CLIUtils/CLI11.git
    GIT_TAG         v2.3.2
    SOURCE_DIR      "${CMAKE_SOURCE_DIR}/deps/CLI11"
)

FetchContent_Declare(
    VideoMasterAPIHelper
    GIT_REPOSITORY  https://github.com/deltacasttv/videomaster-api-helper.git
    GIT_TAG         feature/initial_implementation
    SOURCE_DIR      "${CMAKE_SOURCE_DIR}/deps/api_helper"
)

# VideoMaster API helper
FetchContent_MakeAvailable(VideoMasterAPIHelper)
target_link_libraries(${PROJECT_NAME} PRIVATE VideoMasterAPIHelper)

# VideoMaster SDK
find_package(VideoMaster REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE VideoMaster::VideoMaster)

# video-viewer
FetchContent_MakeAvailable(video-viewer)
target_link_libraries(${PROJECT_NAME} PRIVATE video-viewer)

# CLI11
FetchContent_MakeAvailable(cli11)
target_link_libraries(${PROJECT_NAME} PRIVATE CLI11::CLI11)