#pragma once

#include "types.h"

#ifdef CAPTURE_NATIVE_BUILD
    #define CAPTURE_API __declspec(dllexport)
#else
    #define CAPTURE_API __declspec(dllimport)
#endif

#ifdef __cplusplus
extern "C" {
#endif

/// @brief Create a new capturer with x264 encoder
/// @param config_path Path to YAML config file (NULL or empty for defaults)
/// @param out_capturer_handle Output handle to the created capturer
/// @return CaptureError code (0 = success)
CAPTURE_API int capture_native_create_capturer_x264(const char* config_path, void** out_capturer_handle);

/// @brief Delete a capturer and free resources
/// @param capturer_handle Capturer handle from create function
CAPTURE_API void capture_native_delete_capturer(void* capturer_handle);

/// @brief Start capturing
/// @param capturer_handle Capturer handle
/// @return CaptureError code (0 = success)
CAPTURE_API int capture_native_start_capturer(void* capturer_handle);

/// @brief Stop capturing
/// @param capturer_handle Capturer handle
CAPTURE_API void capture_native_stop_capturer(void* capturer_handle);

/// @brief Get next encoded frame (does not remove from queue)
/// @param capturer_handle Capturer handle
/// @return EncodedFrame struct
CAPTURE_API struct EncodedFrame capture_native_get_frame(void* capturer_handle);

/// @brief Remove front frame from the encoded queue
/// @param capturer_handle Capturer handle
CAPTURE_API void capture_native_pop_frame(void* capturer_handle);

/// @brief Get last capturer error
/// @param capturer_handle Capturer handle
/// @return CaptureError code
CAPTURE_API int capture_native_get_last_capturer_error(void* capturer_handle);

/// @brief Get last encoder error
/// @param capturer_handle Capturer handle
/// @return CaptureError code
CAPTURE_API int capture_native_get_last_encoder_error(void* capturer_handle);

#ifdef __cplusplus
}
#endif
