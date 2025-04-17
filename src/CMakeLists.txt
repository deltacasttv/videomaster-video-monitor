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
find_package(VideoMasterHD 6.30 REQUIRED)
FetchContent_MakeAvailable(VideoMasterCppApi)

target_link_libraries(${PROJECT_NAME} PRIVATE VideoMasterCppApi video-viewer CLI11::CLI11)