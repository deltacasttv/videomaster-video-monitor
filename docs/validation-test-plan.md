# Validation Test Plan

Re-run the affected tests after a change, and extend the plan when a feature is
added. Run capture cases with `--log-level trace` for at least 60 s: they pass
when the picture matches the source, the `Slots count: N (dropped: M)` trace
keeps M under about 1% of N, and the log shows no error. Error cases must fail
cleanly (no crash or hang) with the expected message, and exit with `1` for an
application error or a non-zero CLI11 code for a parse error.

Hardware cases are run by a person at the test bench, not by an AI agent. Never
hard-kill the application (`timeout`, `Stop-Process -Force`, `taskkill /F`):
the board stays open and the driver can be wedged for every later process.
Stop it with Ctrl+C, Esc or the window's close button.

## Build

| Test | Scenario | Expected |
|---|---|---|
| B1 | Windows: `conan install . -b missing`, `cmake --preset conan-release`, `cmake --build --preset conan-release` | Builds; `--version` prints the expected version |
| B2 | Linux, same commands | Same as B1 |
| B3 | macOS, same commands, then a capture | Same as B1; picture renders (render loop on the main thread) |
| B4 | `scripts/check-clang-format.sh` and the CI workflow | No violation, CI green |
| B5 | Fresh `git clone --recursive` | `deps/video-viewer` present, CLI11 comes from Conan |

## Command line

C1 to C11 and C17 to C27 need no signal; C7, C8, C15 and C16 need a board.

| Test | Scenario | Expected |
|---|---|---|
| C1 | `--help` | Lists `--log-level`, `--log-directory`, `--version`, the "Common options" (`-d`, `-i`) and every "IP board options" entry; exit 0 |
| C2 | `-v`, `--version` | Version string; exit 0 |
| C3 | `--log-level` set to `trace`, `debug`, `info`, `warn`, `error`, `critical`, `off` | Console and `video_monitor.log` follow the level; below `info` console lines carry the timestamp pattern |
| C4 | `--log-level verbose` | Parse error, value not allowed |
| C5 | `--log-directory <existing dir>` | `video_monitor.log` written there and overwritten on each run; default is the working directory |
| C6 | `--log-directory <missing dir>` | Parse error, directory does not exist |
| C7 | `-d <board count>` | "Invalid device ID, no board found at index N"; exit 1 |
| C8 | `-i <RX count>` | "Invalid stream ID, RX should be between 0 and N-1 for device D"; exit 1 |
| C9 | `-d -1`, `-i -1`, `-d abc` | Parse error |
| C10 | Unknown option (`--foo`) | Parse error |
| C11 | `--sdp-file <missing file>` | Parse error, file does not exist |
| C12 | `--ip-address` without `--ip-subnet` and the reverse; same for `--ip-sps-address`/`--ip-sps-subnet` | Parse error, "requires" |
| C13 | `--ip-dhcp` with `--ip-address`, `--ip-subnet` or `--ip-gateway` | Parse error, "excludes" |
| C14 | `--ip-sps-dhcp` with `--ip-sps-address`, `--ip-sps-subnet` or `--ip-gateway` | Parse error, "excludes" |
| C15 | `--ip-sps-dhcp` or SPS static address without main DHCP/static configuration | "SPS IP configuration requires main IP configuration"; exit 1 |
| C16 | Malformed IPv4 (`300.1.1.1`) in `--ip-address`, `--ip-subnet`, `--ip-gateway`, `--ip-sps-address`, `--ip-sps-subnet` | "Invalid IPv4 address for <option>"; exit 1 |
| C17 | One explicit media option alone (`--ip-main-destination 239.0.0.5`) | Parse error, requires the six `--ip-video-*` options |
| C18 | Five of the six `--ip-video-*` options | Parse error naming the missing option |
| C19 | Complete explicit options and `--sdp-file` | "Explicit IP media options cannot be combined with --sdp-file"; exit 1 |
| C20 | `--ip-video-*` and SPS options without any main option | "SPS media options require main media options"; exit 1 |
| C21 | UDP port `0` and `65536`, payload type `128` (main and SPS) | Parse error, out of range |
| C22 | `0` for `--ip-video-width`, `-height`, `-bit-depth`, `-framerate-num` or `-framerate-den`, the other five set | Parse error, not in range |
| C23 | `--ip-video-interlaced` set to `0`, `1`, `true`, `false`, then `maybe` | First four accepted, `maybe` is a parse error |
| C24 | `--ip-main-source-filter-mode` other than `include`/`exclude` | Parse error |
| C25 | Filter mode without sources, sources without mode, either without `--ip-main-destination` (same for SPS) | Parse error, "requires" |
| C26 | Malformed IP in `--ip-source-ip`, `--ip-main-destination`, `--ip-sps-destination`, `--ip-*-source-filter-sources` | "Invalid IP address for <option>"; exit 1 |
| C27 | Arguments file from the README (PowerShell `$(Get-Content args.txt)`, Linux `xargs`) | Same behavior as the inline command |

## General and regression (SDI, HDMI and IP)

| Test | Scenario | Expected |
|---|---|---|
| G1 | No argument | Same as `-d 0 -i 0` |
| G2 | Ctrl+C while waiting for a signal | "Stop requested while waiting for initial signal"; exit -1 (255 on Linux) |
| G3 | Ctrl+C while rendering | "Stop requested, leaving monitor loop"; exit 0; no SDK error; next run starts normally |
| G4 | Close the window, then Esc on a new run | Exit 0 |
| G5 | Unplug the input while rendering, replug the same signal | No crash; picture resumes |
| G6 | Change the source format while rendering | SDI/HDMI: "Incoming signal changed on RX<n>, restarting capture cycle", window reopened at the new format. IP: not detected, no crash |
| G7 | Board with loopback (SDI/HDMI): watch the loop-through output before, during and after a run | Loopback disabled while capturing, restored after exit (normal, Ctrl+C, error) |
| G8 | Second instance on the same board and RX | Second one fails cleanly; first keeps rendering |
| G9 | Two instances on two RX of the same board | Both render |
| G10 | `main` build vs branch build, same signal and channel (SDI, HDMI), default log level | Same picture, window size (half the resolution) and drop rate; same board details and detected signal with readable names. Known differences: spdlog output format; API version and device count only at trace level; invalid device exits with 1 instead of -1 |
| G11 | 30 min run at the highest supported format | Drops stay under 1%, no memory growth, no crash |
| G12 | Resize, minimize, move the window to another screen | Picture keeps updating, no extra drop |

## SDI

| Test | Scenario | Expected |
|---|---|---|
| SDI1 | 1080p50, 1080p59.94, 1080p60, also on a 3G-SDI/ASI or 12G-SDI/ASI channel | Picture correct; "Detected SDI signal on RX<n>" with readable standard, clock divisor and interface names, then "Rendering 1920x1080 stream (interlaced=false, ...)" |
| SDI2 | 720p50, 720p59.94 | Same as SDI1 |
| SDI3 | 1080i50, 1080i59.94 on a board supporting field merging | "interlaced=true", no field merging warning, full-height frame; combing on motion is expected (video-viewer#48) |
| SDI4 | Interlaced signal on a board without field merging | "Field merging mode is not supported on this board" warning; picture shown as the board delivers it (fields not reordered on SDI and HDMI, like the FFmpeg demuxer); no crash |
| SDI5 | 3G-A 1080p50/60, 3G-B if the source can | Picture correct |
| SDI6 | 12G 2160p50/59.94 | Picture correct, 1920x1080 window, drops under 1% |
| SDI7 | SD 576i50, 480i59.94 | Picture correct, no shear |
| SDI8 | 23.98, 24, 29.97 | Picture correct |
| SDI9 | Switch 1080p60 and 1080p59.94 while rendering | Detected as a change (clock divisor), capture restarts |
| SDI10 | Switch 3G-A and 3G-B while rendering | Detected as a change (interface), capture restarts |
| SDI11 | Every RX index of the board | Each renders its own signal |

## HDMI and DisplayPort

| Test | Scenario | Expected |
|---|---|---|
| HDMI1 | 1080p60 RGB 4:4:4 source | Colors correct |
| HDMI2 | YCbCr 4:2:2 and 4:4:4 sources | Colors correct |
| HDMI3 | 2160p60 4:2:0 source (FRL) | Picture correct, drops under 1% |
| HDMI4 | 720p, 1080p50, PC formats (1280x1024, 1920x1200) | Window matches the active size |
| HDMI5 | Interlaced 1080i | Same as SDI3/SDI4 |
| HDMI6 | Change the resolution, then only the color space, then only the sampling, while rendering | Each change restarts the capture, "DV signal change details" in the debug log |
| HDMI7 | DisplayPort channel | Same as HDMI1 |

## IP (ST 2110-20)

| Test | Scenario | Expected |
|---|---|---|
| IP1 | `--sdp-file`, main stream only, multicast | Picture correct; "Configuring main ST2110 media: destination ..., UDP ..., payload ..." and "Detected ST2110 video standard ..." |
| IP2 | SDP with a second ST2110-20 `m=video` (SPS); unplug and replug one link | "Parsed SDP file contains a second media description for SPS stream"; no visible glitch |
| IP3 | SDP `a=source-filter` include on the sender, include on a wrong sender, exclude on the sender | Picture; no picture; no picture; never a crash |
| IP4 | SDP with three media | Warning, only the first 2 are used; picture |
| IP5 | SDP whose first or second media is not ST2110-20 | "must be of type ST2110-20"; exit 1 |
| IP6 | Empty or malformed SDP | Clean error; exit 1 |
| IP7 | IP channel without `--sdp-file` nor explicit options | "Either SDP file path or IP media configuration must be provided"; exit 1 |
| IP8 | Explicit mode, main only (README command) | Same as IP1 |
| IP9 | Explicit mode, main and SPS | Same as IP2 |
| IP10 | Explicit, `--ip-main-payload-type` omitted, then wrong | Default 96 used; wrong type gives no picture, no crash, and one "No packet received on the main stream" warning |
| IP11 | Explicit, `--ip-main-udp-port` omitted | Picture |
| IP12 | Explicit unicast: `--ip-main-destination <board IP>` with `--ip-source-ip <sender>`, then without it | Picture; without it, "Source IP address is unspecified" warning and picture |
| IP13 | Explicit, no `--ip-main-destination` | Picture |
| IP14 | `--ip-main-source-filter-mode include` on the sender, include on a wrong sender, exclude on the sender (same for SPS) | Picture; no picture; no picture |
| IP15 | More source filter addresses than the SDK maximum | "Too many main source filter IP addresses"; exit 1 |
| IP16 | Source filter with an IPv6 destination | "Source filtering is only supported for IPv4 addresses"; exit 1 |
| IP17 | Frame rate notations `30/1`, `30000/1000`, `60/2`, `60000/1001`, `30000/1001` | Each selects the sender's standard |
| IP18 | Format matching no standard (`31/1`, 1921x1080) | "No ST2110-20 video standard matches ..."; exit 1 |
| IP19 | Explicit format different from the stream (1080p25 on 1080p30, 8-bit on 10-bit) | No crash; wrong or no picture is acceptable; without picture, one "Packets are received on the main stream but no frame is complete" warning |
| IP20 | `--ip-video-bit-depth` `8` and `10` on matching streams, then `12` | Picture; picture; "Unsupported bit depth: 12" |
| IP21 | 1080i50, 1080i59.94 through SDP and through `--ip-video-interlaced 1` | Same as SDI3; on a board without field merging, "Interlaced fields are received one after the other" and a full-height picture. Record which rate notation (frame or field rate) selects the standard |
| IP22 | `--ip-address`, `--ip-subnet`, `--ip-gateway` | Port configured with these values (trace log); picture |
| IP23 | `--ip-dhcp` with a DHCP server, then without | "DHCP lease acquired"; without server, "Timed out waiting for DHCP to assign an IP address" after 10 s, exit 1 |
| IP24 | SPS port static (`--ip-sps-address`, `--ip-sps-subnet`) and DHCP (`--ip-sps-dhcp`), with an SPS stream | SPS port configured; SPS reception works |
| IP25 | No network option | "Pre-configured network settings will be used" warning; picture |
| IP26 | `--sdp-file`, IP network or IP media options on an SDI or HDMI channel | "... only valid for IP 2110 channels"; exit 1 |
| IP27 | Stop (Ctrl+C or window), restart immediately | Multicast groups left on close; second run receives |
| IP28 | One instance per video channel (`-i 0` to `-i 3`), each on a different stream, all at once | Each window shows its own stream; main always on ETH0, SPS always on ETH1 |
| IP29 | Two channels on the same multicast group, stop one; then a second instance on a channel already in use | In both cases the remaining instance keeps receiving (no multicast leave affecting it); the second instance fails cleanly |
