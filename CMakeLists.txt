cmake_minimum_required(VERSION 3.16)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

include(cmake/version.cmake)
git_version(GIT_VERSION)
set_version(${GIT_VERSION})

project(
    "VideoMasterInputViewer"
    VERSION ${PROJECT_VERSION}
    DESCRIPTION "Application that displays the content of an input stream captured by a Deltacast.TV device, interfaced with the VideoMaster SDK"
    LANGUAGES CXX
)

add_subdirectory("src")