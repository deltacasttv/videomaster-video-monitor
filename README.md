# VideoMaster input viewer

Application that displays the content of an input stream captured by a Deltacast.TV device, interfaced with the VideoMaster SDK.

# How to build

VideoViewer requires some dependencies to be installed on the system:

    cmake v3.20 or higher
    glfw v3.3.6
    Python 3

Retrieve dependencies with Conan (optional)

To use Conan 1.x to retrieve the dependencies, create the `modules`` directory and use the install command:

mkdir /path/to/modules
cd /path/to/modules
conan install /path/to/video-viewer -b missing -g cmake_find_package

# VideoMaster SDK

The VideoMaster SDK is required to build the application and the following environment variables must be set:

    VIDEOMASTERHD_INCLUDE_DIRS: path to the VideoMaster SDK include directory
    VIDEOMASTERHD_LIBRARIES: path to the VideoMaster SDK library directory

Building with CMake

If you used Conan to retrieve your dependencies, you can use the following commands to build the project:

cd /path/to/video-viewer
cmake -S . -B build -DCMAKE_MODULE_PATH:PATH=/path/to/modules
cmake --build build

# How to use

All relevant information regarding the application can be found by running the application with the --help option:

./VideoMasterInputViewer --help

For example, to run the application with the default settings, simply run:

./VideoMasterInputViewer

To select the device and connector indexes to use

./VideoMasterInputViewer -d 0 -i 0

Use the device of indexe 0 and the reception connector of indexe 0.


