cmake_minimum_required(VERSION 3.16)
include(FetchContent)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}")

include(cmake/version.cmake)
git_version(GIT_VERSION)
set_version(${GIT_VERSION})

configure_file(
    ${CMAKE_SOURCE_DIR}/cmake/version.h.in
    ${CMAKE_SOURCE_DIR}/src/version.h
    @ONLY
)

project(
    "VideoMasterInputViewer"
    VERSION ${PROJECT_VERSION}
    DESCRIPTION "Application that displays the content of an input stream captured by a Deltacast.TV device, interfaced with the VideoMaster SDK"
    LANGUAGES CXX
)

if(MSVC)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
endif()


# project sources
set(${PROJECT_NAME}_SOURCES
    ${CMAKE_SOURCE_DIR}/src/main.cpp
    ${CMAKE_SOURCE_DIR}/src/device.cpp
)

add_executable(${PROJECT_NAME} ${${PROJECT_NAME}_SOURCES})

# dependencies
FetchContent_Declare(
  video-viewer
  GIT_REPOSITORY https://github.com/deltacasttv/video-viewer.git
  GIT_TAG        1.0.1
  SOURCE_DIR "${CMAKE_SOURCE_DIR}/deps/video-viewer"
)

FetchContent_Declare(
    cli11
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_TAG        v2.3.2
    SOURCE_DIR "${CMAKE_SOURCE_DIR}/deps/CLI11"
)

# VideoMaster API helper
add_subdirectory("deps/api_helper")
target_link_libraries(${PROJECT_NAME} PRIVATE VideoMasterAPIhelper)
get_target_property(API_HELPER_INCLUDE_DIRS VideoMasterAPIhelper INTERFACE_INCLUDE_DIRECTORIES)
target_include_directories(${PROJECT_NAME} PRIVATE "${API_HELPER_INCLUDE_DIRS}")

# VideoMaster SDK
find_package(VideoMasterHD REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE VideoMasterHD::Core)

# video-viewer
FetchContent_MakeAvailable(video-viewer)
target_link_libraries(${PROJECT_NAME} PRIVATE video-viewer)

# CLI11
FetchContent_MakeAvailable(cli11)
target_link_libraries(${PROJECT_NAME} PRIVATE CLI11::CLI11)