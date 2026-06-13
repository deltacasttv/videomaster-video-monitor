---
applyTo: "**"
description: "Instructions for GitHub Copilot when reviewing or contributing to this repository."
---

# Copilot Instructions — videomaster-video-monitor

## Project Overview

This is a **C++ video monitoring application** that uses the **VideoMaster SDK**
and its **C++ wrapper VideoMasterCppAPI** — both proprietary libraries from DELTACAST.TV
that are **not publicly available**.

The SDK provides proper CMake integration. `VideoMasterCppApi` is fetched via
`FetchContent_MakeAvailable(VideoMasterCppApi)` and linked as:

```cmake
target_link_libraries(${PROJECT_NAME} PRIVATE
    VideoMasterCppApi
    CLI11::CLI11
    spdlog::spdlog
    fmt::fmt
    video-viewer
    ipaddress::ipaddress
)
````
> Do not:
>
> * Flag VideoMaster or VideoMasterCppApi symbols, types, or functions as unknown
> * Suggest replacing them with open-source alternatives
> * Suggest adding includes for them — they are resolved by CMake at build time
> * Flag their API usage as incorrect or deprecated
 
> Assume all VideoMaster/VideoMasterCppApi usage is intentional and correct unless surrounding code shows a clear logical error (e.g. unchecked return value compared to identical calls in the same file).

### Unit testing

We are aware that the lack of unit tests is a shortcoming. Please don't comment on that. We will work to address this issue in the future.

## Languages and Tooling

* C++ — primary language
* CMake — build system (CMakePresets.json is present, use --preset)
* Conan 2 — dependency manager (conan install before CMake configure)
* clangd — a .clangd file is present at the root; respect it
* clang-format — a .clang-format file is present at the root; it is the single source of truth for formatting

## Build Workflow

### Install dependencies
`conan install conanfile.txt -b missing -s build_type=<Debug|Release>`

### Configure
`cmake --preset conan-<release|debug>`

### Build
`cmake --build --preset conan-<release|debug>`

> Due to private dependency (VideoMaster SDK), this is intentional that no github action appears to build and test the code.

## Coding Style

### Naming Convention

| Construct | Convention |
|---|---|
| Types (classes, structs, enums, typedefs, templates) | UpperCamelCase |
| Variables, functions, methods, parameters, enum values | lower_snake_case |
| Namespaces | lower_snake_case (short, meaningful) |
| Macros | UPPER_SNAKE_CASE (avoid when possible) |
| Member variables | may use m_ prefix |
| CMake targets, variables, functions | lower_snake_case |
| Conan recipe names and options | lower_snake_case |

Any existing code following this convention is **intentional and correct**.
Do not flag it as a style issue.

### Formatting

The `.clang-format` file defines all formatting rules and is checked through CI/CD job. Do not make formatting suggestions — assume committed code has already been
passed through clang-format.

### General Code Quality

* Names must clearly express intent; prefer clarity over brevity
* Declare variables in the narrowest possible scope
* Prefer const / immutable variables whenever possible
* Functions must do one thing; maintain a single level of abstraction
* A function is either an action (side effects, returns void/status) or a question (no side effects, returns a value) — not both
* Do not comment bad code: rewrite it
* Comments must explain intent or non-obvious design decisions — never metadata, never commented-out code

## Code Review Focus

When reviewing, focus exclusively on:

* Logic correctness and undefined behavior in C++
* Resource leaks and missing RAII — especially VideoMaster handles which must be explicitly released
* Thread safety issues
* CMake target visibility (PUBLIC / PRIVATE / INTERFACE misuse)
* CMake issue or good practices
* Conan dependency declarations inconsistent with actual use

#### Do not comment on:

* Naming or formatting — enforced by tooling
* VideoMaster / VideoMasterCppApi symbols or usage patterns
* FetchContent usage for VideoMasterCppApi — this is intentional
* Patterns that appear consistently across the codebase — they are intentional conventions, not mistakes

### Reducing Review Noise

* If a pattern appears unchanged and consistent across multiple files, it is intentional — do not flag it
* Do not suggest changes that would require access to the proprietary SDK internals to validate
* Prefer no comment over a speculative or low-confidence comment
* When uncertain whether something is a bug or a convention, ask rather than flag

## General Behavior

* Write all code, comments, documentation, and technical text in English
* Prefer clarity, determinism, and explicit behavior over cleverness
* When in doubt about intent, ask for clarification before suggesting changes
* Respect established patterns — do not introduce deviations unless explicitly directed
