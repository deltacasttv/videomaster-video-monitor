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
)
add_executable(${PROJECT_NAME} ${${PROJECT_NAME}_SOURCES})

# dependencies

# VideoMaster SDK
find_package(VideoMasterHD 6.26 REQUIRED)
FetchContent_MakeAvailable(VideoMasterCppApi)

# video-viewer
FetchContent_Declare(
  video-viewer
  GIT_REPOSITORY    https://github.com/deltacasttv/video-viewer.git
  GIT_TAG           1.1.2
)
FetchContent_MakeAvailable(video-viewer)

# CLI11
FetchContent_Declare(
    cli11
    GIT_REPOSITORY  https://github.com/CLIUtils/CLI11.git
    GIT_TAG         v2.3.2
)
FetchContent_MakeAvailable(cli11)

target_link_libraries(${PROJECT_NAME} PRIVATE VideoMasterCppApi video-viewer CLI11::CLI11)