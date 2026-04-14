cmake_minimum_required(VERSION 3.16)
include(FetchContent)

configure_file(
    ${CMAKE_SOURCE_DIR}/cmake/version.hpp.in
    ${CMAKE_SOURCE_DIR}/src/version.hpp
    @ONLY
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# project sources
set(${PROJECT_NAME}_SOURCES
    ${CMAKE_SOURCE_DIR}/src/main.cpp
    ${CMAKE_SOURCE_DIR}/src/helper.cpp
    ${CMAKE_SOURCE_DIR}/src/shared_resources.cpp
    ${CMAKE_SOURCE_DIR}/src/windowed_renderer.cpp
    ${CMAKE_SOURCE_DIR}/src/video_monitor.cpp
    ${CMAKE_SOURCE_DIR}/src/input_session/sdi_input_session.cpp
    ${CMAKE_SOURCE_DIR}/src/input_session/dv_input_session.cpp
    ${CMAKE_SOURCE_DIR}/src/input_session/input_session_factory.cpp
)

set(${PROJECT_NAME}_INCLUDE_DIRECTORIES
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/input_session
)

add_executable(${PROJECT_NAME} ${${PROJECT_NAME}_SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE ${${PROJECT_NAME}_INCLUDE_DIRECTORIES})

find_package(CLI11 REQUIRED)
find_package(spdlog REQUIRED)
find_package(fmt REQUIRED)
find_package(video-viewer REQUIRED)
find_package(VideoMasterHD 6.30 REQUIRED)
FetchContent_MakeAvailable(VideoMasterCppApi)

target_link_libraries(${PROJECT_NAME} PRIVATE VideoMasterCppApi video-viewer CLI11::CLI11 spdlog::spdlog fmt::fmt video-viewer::video-viewer)