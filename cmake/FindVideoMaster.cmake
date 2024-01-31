# Find the main root of the SDK
find_path(VideoMaster_SDK_DIR NAMES Include/VideoMasterHD_Core.h PATHS /usr/local/deltacast REQUIRED)

find_path(VideoMaster_INCLUDE_DIR NAMES VideoMasterHD_Core.h PATHS ${VideoMaster_SDK_DIR}/Include REQUIRED)
mark_as_advanced(VideoMaster_INCLUDE_DIR)

find_library(videomasterhd_core
    NAMES libvideomasterhd.so
    PATHS /usr/lib
    REQUIRED
)

if(videomasterhd_core)
  add_library(VideoMaster::videomasterhd_core SHARED IMPORTED)
  set_target_properties(VideoMaster::videomasterhd_core PROPERTIES
            IMPORTED_LOCATION "${videomasterhd_core}"
            INTERFACE_INCLUDE_DIRECTORIES "${VideoMaster_INCLUDE_DIR}"
        )
  set(VideoMaster_FOUND 1)
endif()


# Generate VideoMaster_FOUND
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(VideoMaster
    FOUND_VAR VideoMaster_FOUND
    REQUIRED_VARS videomasterhd_core
)