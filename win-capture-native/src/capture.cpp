#include "encoders/encoder_x264.hpp"
#include "capture.h"
#include "capturer_fabric.hpp"

#define CAPTURER_POINTER(ptr) (reinterpret_cast<cn::Capturer*>(ptr))

CAPTURE_API int capture_native_create_capturer_x264(const char* config_path, void** out_capturer_handle) {
    auto [capturer, create_error] = cn::CapturerFabric::Instance().CreateCapturer<cn::EncoderX264>(config_path);
    if (create_error != CaptureError::CaptureErrorOK) {
        return static_cast<int>(create_error);
    }

    *out_capturer_handle = capturer;
    return static_cast<int>(CaptureError::CaptureErrorOK);
}

CAPTURE_API void capture_native_delete_capturer(void* capturer_handle) {
    cn::CapturerFabric::Instance().DeleteCapturer(CAPTURER_POINTER(capturer_handle));
}

CAPTURE_API int capture_native_start_capturer(void* capturer_handle) {
    const auto error = CAPTURER_POINTER(capturer_handle)->Start();
    return static_cast<int>(error);
}

CAPTURE_API void capture_native_stop_capturer(void* capturer_handle) {
    CAPTURER_POINTER(capturer_handle)->Stop();
}

CAPTURE_API struct EncodedFrame capture_native_get_frame(void* capturer_handle) {
    decltype(auto) encoded = CAPTURER_POINTER(capturer_handle)->GetEncoded();

    auto result = EncodedFrame{};
    auto slot = encoded.front();

    if (slot != nullptr) {
        result.data = slot->buffer.data();
        result.size = slot->buffer.size();
        result.keyframe = slot->keyframe;
        result.pts = slot->pts;
    }

    return result;
}

CAPTURE_API void capture_native_pop_frame(void* capturer_handle) {
    decltype(auto) encoded = CAPTURER_POINTER(capturer_handle)->GetEncoded();
    encoded.pop();
}

CAPTURE_API int capture_native_get_last_capturer_error(void* capturer_handle) {
    const auto error = CAPTURER_POINTER(capturer_handle)->GetLastCapturerWorkerError();
    return static_cast<int>(error);
}

CAPTURE_API int capture_native_get_last_encoder_error(void* capturer_handle) {
    const auto error = CAPTURER_POINTER(capturer_handle)->GetLastEncoderWorkerError();
    return static_cast<int>(error);
}


