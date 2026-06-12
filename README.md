# VideoMaster video monitor

Application that displays the content of an input stream captured by a DELTACAST device, interfaced with the VideoMaster SDK.

SDI, HDMI/DisplayPort and IP ST2110 channels are supported by this application.

OS Support:
- Windows
- Linux
- MacOS

See https://www.deltacast.tv for more video products.

# How to build

VideoViewer requires some dependencies to be installed on the system:

    cmake v3.20 or higher
    glfw v3.4.0
    Python 3
    jinja2 (as a Python package or through your package manager)

We recommend using Conan 2.x to retrieve those dependencies:

    conan install . -b missing

## Dependency compatibility

The table below summarizes the expected compatibility between `video-viewer` and the VideoMaster SDK.

| `video-viewer` version | Supported VideoMaster SDK versions |
| --- | --- |
| `>=2.0.0, <3.0.0` | `>=6.30` and `<6.35.1 beta` |
| `3.0.0` | `>=6.35.1 beta` |

## VideoMaster SDK

The VideoMaster SDK is required to build the application.

After installing the SDK according to the official documentation, the libs and headers should be found without further step needed through the `find_package` command.

## Building with CMake

If you used Conan to retrieve your dependencies, you can use the following commands to build the project:

    cmake --preset conan-release
    cmake --build --preset conan-release

# How to use

All relevant information regarding the application can be found by running the application with the --help option:

    ./videomaster-video-monitor --help

For example, to run the application with the default settings (same as `--device 0 --input 0`), simply run:

    ./videomaster-video-monitor

To select the device and connector indexes to use

    ./videomaster-video-monitor --device 0 --input 0

Use the device at index 0 and the reception connector at index 0.

## IP input examples

### IP with SDP file

Use this method when you already have an SDP describing the stream.

    ./videomaster-video-monitor --device 0 --input 0 --sdp-file ./stream.sdp

Example `stream.sdp` (ST2110-20 main stream):

```sdp
v=0
o=- 0 0 IN IP4 127.0.0.1
s=DELTACAST ST2110-20 RX
t=0 0
a=recvonly
c=IN IP4 239.10.20.30/32
m=video 5004 RTP/AVP 112
a=rtpmap:112 raw/90000
a=fmtp:112 sampling=YCbCr-4:2:2; width=1920; height=1080; exactframerate=60000/1001; depth=8; colorimetry=BT709; PM=2110GPM
```

For main + SPS, the SDP must contain a second `m=video` ST2110-20 media section for SPS.

### IP explicit media configuration ("normal" method)

Use this method when no SDP file is provided and media parameters are passed explicitly.

Main stream only:

    ./videomaster-video-monitor --device 0 --input 0 \
      --ip-main-video-width 1920 --ip-main-video-height 1080 \
      --ip-main-framerate-num 60000 --ip-main-framerate-den 1001 \
      --ip-main-destination 239.10.20.30 --ip-main-udp-port 5004 \
      --ip-main-payload-type 112

Main + SPS stream:

    ./videomaster-video-monitor --device 0 --input 0 \
      --ip-video-width 1920 --ip-video-height 1080 \
      --ip-bit-depth 10 \
      --ip-framerate-num 60000 --ip-framerate-den 1001 \
      --ip-main-destination 239.10.20.30 --ip-main-udp-port 5004 \
      --ip-main-payload-type 112 \
      --ip-sps-video-width 1920 --ip-sps-video-height 1080 \
      --ip-sps-framerate-num 60000 --ip-sps-framerate-den 1001 \
      --ip-sps-destination 239.10.20.31 --ip-sps-udp-port 5006 \
      --ip-sps-payload-type 113

Notes:
- Main media parameters are mandatory in explicit mode.
- SPS media parameters are optional but cannot be used without main media parameters.
- `--ip-main-udp-port` and `--ip-sps-udp-port` are optional (recommended).
- `--ip-main-destination` and `--ip-sps-destination` are optional in unicast implicit mode.
- `--ip-main-payload-type` and `--ip-sps-payload-type` are optional; default is `96`.

## Using an arguments file

When there are many parameters, you can place them in a text file and pass them on the command line.

Example `args.txt`:

    --device
    0
    --input
    0
    --ip-video-width
    1920
    --ip-video-height
    1080
    --ip-bit-depth
    10
    --ip-framerate-num
    60000
    --ip-framerate-den
    1001
    --ip-main-destination
    239.10.20.30
    --ip-main-udp-port
    5004
    --ip-main-payload-type
    112

Windows (PowerShell):

    .\videomaster-video-monitor.exe $(Get-Content args.txt)

Linux/macOS:

    xargs ./videomaster-video-monitor < args.txt
