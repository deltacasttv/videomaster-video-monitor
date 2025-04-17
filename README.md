# VideoMaster video monitor

Application that displays the content of an input stream captured by a DELTACAST device, interfaced with the VideoMaster SDK.

SDI, HDMI or DisplayPort channels are currently the only valid types of channels for this application.

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

As some dependencies are also retrieved through submodules, you will need to initialize them:

    git submodule update --init --recursive

## VideoMaster SDK

The VideoMaster SDK (version >= 6.30) is required to build the application.

After installing the SDK according to the official documentation, the libs and headers should be found without further step needed through the `find_package` command.

## Building with CMake

If you used Conan to retrieve your dependencies, you can use the following commands to build the project:

    cmake --preset YOUR_CMAKE_PRESET
    cmake --build build

# How to use

All relevant information regarding the application can be found by running the application with the --help option:

    ./videomaster-video-monitor --help

For example, to run the application with the default settings (same as `--device 0 --input 0`), simply run:

    ./videomaster-video-monitor

To select the device and connector indexes to use

    ./videomaster-video-monitor --device 0 --input 0

Use the device at index 0 and the reception connector at index 0.
