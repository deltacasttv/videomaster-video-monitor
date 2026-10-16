# 2.1.0

## Added

- Support for DELTA-hmi-e 40 [PR #9]
- Support for DELTA-IP25-44-elp [PR #14]
- ST 2110-20 input, described by an SDP file (`--sdp-file`) or by explicit media options (`--ip-video-*`, `--ip-main-*`)
- ST 2022-7 redundancy: SPS stream on ETH1, from the second media of the SDP or the `--ip-sps-*` options
- Multicast (IGMPv3) and unicast reception, with optional source filtering
- IP port configuration: static address (`--ip-address`, `--ip-subnet`, `--ip-gateway`, SPS equivalents) or DHCP (`--ip-dhcp`, `--ip-sps-dhcp`)
- IP multicast groups joined again after 1 s without frames, so the capture resumes after a link loss
- Interlaced IP video on a board without field merging: fields interleaved before display
- Diagnostics when no IP frame arrives: no packet received, or packets that do not match the configured format
- Support for 3G-SDI/ASI and 12G-SDI/ASI channels, as SDI inputs
- Log level (`--log-level`) and log file directory (`--log-directory`) options

## Improved

- Loopback restored to its initial state on exit, instead of always being re-enabled
- Explicit errors for invalid or inconsistent options, including IP options on a non-IP channel

# 2.0.0

## Added

- Support for DV streams

## Improved

- Complete refactoring to make use of new VideoMaster C++ Wrapper
- Update dependency to video-viewer
- Handle dependencies with conan 2.x and submodule instead of fetch_content
- Update application name to reflect repository name

## Fixed

- Stream would start too soon before the monitor is completely initialized, leading to some drops


# 1.1.0

## Improved

- Update dependency to api-helper
  - find_package for VideoMaster (requires 6.25)
  - Abstraction of SDI-specific properties and configuration
- Cleaning and refactoring for publication 

# 1.0.0

## Added

- Application that is capable of
  - Receiving video from an input
  - Rendering the video data on screen
- Support of Windows and Linux
- Support of SDI devices
- Detection of change in the input format and automatic reconfiguration of the stream
