# VideoMaster video monitor

Application that displays the content of an input stream captured by a DELTACAST device, interfaced with the VideoMaster SDK.

An SDI device is currently the only valid type of device for this application.

OS Support:
- Windows
- Linux

See https://www.deltacast.tv for more video products.

# How to build

VideoViewer requires some dependencies to be installed on the system:

    cmake v3.20 or higher
    glfw v3.4.0
    Python 3

We recommend using Conan 2.x to retrieve those dependencies:

    conan install . -b missing -pr YOUR_CONAN_PROFILE

## VideoMaster SDK

The VideoMaster SDK is required to build the application.

After installing the SDK according to the official documentation, the libs and headers should be found without further step needed through the `find_package` command.

## Building with CMake

If you used Conan to retrieve your dependencies, you can use the following commands to build the project:

    cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=generators/conan_toolchain.cmake
    cmake --build build

# How to use

All relevant information regarding the application can be found by running the application with the --help option:

    ./VideoMasterInputViewer --help

For example, to run the application with the default settings (same as `--device 0 --input 0`), simply run:

    ./VideoMasterInputViewer

To select the device and connector indexes to use

    ./VideoMasterInputViewer --device 0 --input 0

Use the device of indexe 0 and the reception connector of indexe 0.