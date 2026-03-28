#include <Windows.h>
#include <d3d11.h>
#include <thread>

#include "encoders/encoder_x264.hpp"
#include "utils/scope_exit.hpp"
#include "utils/timer.hpp"

#include <tracy/Tracy.hpp>

using namespace std::chrono_literals;

namespace cn{
EncoderX264::~EncoderX264() {
    Stop();
}

auto EncoderX264::InitializeX264() -> CaptureError {
    Stop(); // clear before config

    if (x264_param_default_preset(&_x264_param, _config.x264.preset.c_str(), "zerolatency") != 0) {
        return CaptureError::CAPTURE_ERROR_INVALID_X264_PRESET;
    }

#ifdef _DEBUG
    _x264_param.i_log_level = X264_LOG_DEBUG;
#else
    _x264_param.i_log_level = X264_LOG_NONE;
#endif // _DEBUG

    _x264_param.i_width  = _config.general.width;
    _x264_param.i_height = _config.general.height;

    _x264_param.i_fps_num = _config.general.fps;
    _x264_param.i_fps_den = 1;

    _x264_param.i_threads = _config.x264.threads;

    if (x264_param_apply_profile(&_x264_param, _config.x264.profile.c_str()) != 0) {
        return CaptureError::CAPTURE_ERROR_INVALID_X264_PROFILE;
    }

    _x264_param.rc.i_rc_method = X264_RC_CRF; // constant quality
    _x264_param.rc.f_rf_constant = _config.x264.rf_constant; // 0-51 (< == better)

    _x264_param.i_keyint_max = _config.general.fps; // keyframe every second
    _x264_param.i_scenecut_threshold = 0;

    _x264_param.i_bframe = 0; // for lower latency

    _x264_param.b_vfr_input = 0;
    _x264_param.b_repeat_headers = 1;
    _x264_param.b_annexb = 1;

    _x264_param.i_csp = X264_CSP_NV12; // NV12 instead of I420

    _x264_encoder = x264_encoder_open(&_x264_param);
    if (_x264_encoder == nullptr) {
        return CaptureError::CAPTURE_ERROR_FAILED_X264_ENCODER_OPEN;
    }

    x264_picture_init(&_x264_pic_in);
    x264_picture_init(&_x264_pic_out);

    _x264_pic_in.img.i_csp = _x264_param.i_csp;
    _x264_pic_in.img.i_plane = 2; // NV12 has 2 planes

    return CaptureError::CAPTURE_ERROR_OK;
}

auto EncoderX264::Start() noexcept -> CaptureError {
    auto error = InitializeX264();
    if (error != CaptureError::CAPTURE_ERROR_OK) {
        return error;
    }

    _running.store(true, std::memory_order_release);
    _worker = std::thread(&EncoderX264::Worker, this);

    return CaptureError::CAPTURE_ERROR_OK;
}

auto EncoderX264::Stop() noexcept -> void {
    _running.store(false, std::memory_order_relaxed);

    if (_worker.joinable()) {
        _worker.join();
    }

    if (_x264_encoder != nullptr) {
        x264_encoder_close(_x264_encoder);
    }
}

auto EncoderX264::HandleRawCpuFrame(RawCpuFrame const& frame) noexcept -> CaptureError {
    // Setup NV12 planes (pointer arithmetic only, no copy)
    _x264_pic_in.img.plane[0] = const_cast<uint8_t*>(frame.data);
    _x264_pic_in.img.plane[1] = const_cast<uint8_t*>(frame.data) + static_cast<size_t>(frame.pitch) * frame.height;
    _x264_pic_in.img.plane[2] = nullptr;

    _x264_pic_in.img.i_stride[0] = frame.pitch;
    _x264_pic_in.img.i_stride[1] = frame.pitch;
    _x264_pic_in.img.i_stride[2] = 0;

    _x264_pic_in.i_pts = static_cast<int64_t>(frame.pts);

    auto nal = static_cast<x264_nal_t*>(nullptr);
    auto nal_count = 0;

    {
        ZoneScopedNC("HandleRawCpuFrame.x264_encoder_encode", tracy::Color::Red);
        ZoneValue(frame.pts);
        const auto encoded_bytes = x264_encoder_encode(_x264_encoder, &nal, &nal_count, &_x264_pic_in, &_x264_pic_out);
        // encoded_bytes == 0 -> frame not ready / < 0 -> error
        if (encoded_bytes <= 0) {
            return (encoded_bytes < 0) ? CaptureError::CAPTURE_ERROR_FAILED_X264_ENCODE : CaptureError::CAPTURE_ERROR_OK;
        }
    }

    auto buffer = std::vector<uint8_t>{};

    auto total_size = 0;
    for (auto i = 0; i < nal_count; i++) {
        total_size += nal[i].i_payload;
    }

    buffer.resize(total_size);

    // Combine NAL fragments
    auto offset = uint32_t{0};
    for (auto i = 0; i < nal_count; i++) {
        const auto nal_size = nal[i].i_payload;
        const auto nal_data = nal[i].p_payload;
        std::memcpy(buffer.data() + offset, nal_data, nal_size);
        offset += nal_size;
    }

    const auto result = _encoded.try_emplace(std::move(buffer), frame.pts, _x264_pic_out.i_type == X264_TYPE_IDR);
    return result ? CaptureError::CAPTURE_ERROR_OK : CaptureError::CAPTURE_ERROR_FAILED_ENCODER_BUFFER_EMPLACE;
}

auto EncoderX264::Worker() noexcept -> void {
    auto timer = utils::Timer(1s, _config.general.fps, 0);
    auto frame_pts = uint64_t{0};

    while (_running.load(std::memory_order_relaxed)) {
        timer.Wait();

        auto slot = _captured.TryLockLatestSlot(MAX_LOCK_RETRY_COUNT);
        if (slot == nullptr) {
            _last_error.store(CaptureError::CAPTURE_ERROR_EMPTY_CAPTURED_BUFFER, std::memory_order_relaxed);
            continue;
        }

        auto eg1 = utils::make_scope_exit([slot]() {
            slot->Unlock();
        });

        if (slot->staging == nullptr) {
            _last_error.store(CaptureError::CAPTURE_ERROR_INVALID_CAPTURED_TEXTURE, std::memory_order_relaxed);
            continue;
        }

        if (slot->staging_map.pData == nullptr) {
            _last_error.store(CaptureError::CAPTURE_ERROR_INVALID_TEXTURE_MAP, std::memory_order_relaxed);
            continue;
        }

        const auto raw_frame = RawCpuFrame{
            .data   = static_cast<uint8_t const*>(slot->staging_map.pData),
            .pitch  = static_cast<int>(slot->staging_map.RowPitch),
            .width  = static_cast<int>(slot->staging_description.Width),
            .height = static_cast<int>(slot->staging_description.Height),
            .pts    = frame_pts,
        };

        const auto error = HandleRawCpuFrame(raw_frame);
        _last_error.store(error, std::memory_order_relaxed);

        if (error != CaptureError::CAPTURE_ERROR_OK) {
            continue;
        }

        frame_pts += 1;
        FrameMarkNamed("Encoding_X264");
    }
}

auto EncoderX264::IsUsingStaging() const noexcept -> bool {
    return true;
}
}
