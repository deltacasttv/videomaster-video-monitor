# Unreleased

## Added

- Support for DELTA-hmi-e 40 [PR #9]

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
