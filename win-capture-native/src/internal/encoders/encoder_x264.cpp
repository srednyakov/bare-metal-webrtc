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

auto EncoderX264::InitializeX264(int width, int height) noexcept -> CaptureError {
    if (_x264_encoder != nullptr) {
        x264_encoder_close(_x264_encoder);
        _x264_encoder = nullptr;
    }

    if (x264_param_default_preset(&_x264_param, _config.x264.preset.c_str(), "zerolatency") != 0) {
        return CaptureError::CaptureErrorInvalidX264Preset;
    }

#ifdef _DEBUG
    _x264_param.i_log_level = X264_LOG_DEBUG;
#else
    _x264_param.i_log_level = X264_LOG_NONE;
#endif // _DEBUG

    _x264_param.i_width  = width;
    _x264_param.i_height = height;

    _x264_param.i_fps_num = _config.output.fps;
    _x264_param.i_fps_den = 1;

    _x264_param.i_threads = _config.x264.threads;

    if (x264_param_apply_profile(&_x264_param, _config.x264.profile.c_str()) != 0) {
        return CaptureError::CaptureErrorInvalidX264Profile;
    }

    _x264_param.rc.i_rc_method = X264_RC_CRF; // constant quality
    _x264_param.rc.f_rf_constant = _config.x264.rf_constant; // 0-51 (< == better)

    _x264_param.i_keyint_max = _config.output.fps; // keyframe every second
    _x264_param.i_scenecut_threshold = 0;

    _x264_param.i_bframe = 0; // for lower latency

    _x264_param.b_vfr_input = 0;
    _x264_param.b_repeat_headers = 1;
    _x264_param.b_annexb = 1;

    _x264_param.i_csp = X264_CSP_NV12; // NV12 instead of I420

    _x264_encoder = x264_encoder_open(&_x264_param);
    if (_x264_encoder == nullptr) {
        return CaptureError::CaptureErrorFailedX264EncoderOpen;
    }

    x264_picture_init(&_x264_pic_in);
    x264_picture_init(&_x264_pic_out);

    _x264_pic_in.img.i_csp = _x264_param.i_csp;
    _x264_pic_in.img.i_plane = 2; // NV12 has 2 planes

    return CaptureError::CaptureErrorOK;
}

auto EncoderX264::Start(int width, int height) noexcept -> CaptureError {
    auto error = InitializeX264(width, height);
    if (error != CaptureError::CaptureErrorOK) {
        return error;
    }

    _running.store(true, std::memory_order_release);
    _worker = std::thread(&EncoderX264::Worker, this);

    return CaptureError::CaptureErrorOK;
}

auto EncoderX264::Stop() noexcept -> void {
    _running.store(false, std::memory_order_relaxed);

    if (_worker.joinable()) {
        _worker.join();
    }

    if (_x264_encoder != nullptr) {
        x264_encoder_close(_x264_encoder);
        _x264_encoder = nullptr;
    }
}

auto EncoderX264::HandleRawCpuFrame(RawCpuFrame const& frame) noexcept -> CaptureError {
    // reinit encoder after input resolution change
    if (_x264_param.i_width != frame.width || _x264_param.i_height != frame.height) {
        const auto error = InitializeX264(frame.width, frame.height);
        if (error != CaptureError::CaptureErrorOK) {
            return error;
        }
    }

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
            return (encoded_bytes < 0) ? CaptureError::CaptureErrorFailedX264Encode : CaptureError::CaptureErrorOK;
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

    const auto is_keyframe = _x264_pic_out.i_type == X264_TYPE_IDR;

    const auto result = _encoded.try_emplace(std::move(buffer), frame.pts, is_keyframe);
    return result ? CaptureError::CaptureErrorOK : CaptureError::CaptureErrorFailedEncoderBufferEmplace;
}

auto EncoderX264::CopyToCached(CapturedSlot const* captured, uint64_t frame_pts) noexcept -> void {
    const auto total_size = captured->staging_map.RowPitch * (captured->staging_description.Height * 3 / 2);

    _cached_buffer.resize(total_size);
    std::memcpy(_cached_buffer.data(), captured->staging_map.pData, total_size);

    _cached_frame = RawCpuFrame{
        .data   = static_cast<uint8_t const*>(_cached_buffer.data()),
        .pitch  = static_cast<int>(captured->staging_map.RowPitch),
        .width  = static_cast<int>(captured->staging_description.Width),
        .height = static_cast<int>(captured->staging_description.Height),
        .pts    = frame_pts,
    };
}

auto EncoderX264::HandleCachedMode(uint64_t frame_pts) -> CaptureError {
    if (IsCachedReady()) {
        if (_cached_frame.pts == 0) {
            return CaptureError::CaptureErrorNoCachedFrame;
        }
        _cached_frame.pts = frame_pts;
        return HandleRawCpuFrame(_cached_frame);
    }

    // all slots must be free while GetUseCached() == true
    auto slot = _captured.TryLockLatestSlot(MAX_LOCK_RETRY_COUNT);
    if (slot == nullptr) {
        _cached_frame.pts = 0;
        // failed to get cached frame, store true to prevent infinite capturer lock
        _cached_ready.store(true, std::memory_order_relaxed);
        return CaptureError::CaptureErrorNoCachedFrame;
    }

    CopyToCached(slot, frame_pts);
    _cached_ready.store(true, std::memory_order_relaxed);
    
    return HandleRawCpuFrame(_cached_frame);
}

auto EncoderX264::Worker() noexcept -> void {
    auto timer = utils::Timer(975ms, _config.output.fps, 0);
    auto frame_pts = uint64_t{0};

    while (IsRunning()) {
        timer.Wait();

        if (GetUseCached()) {
            const auto error = HandleCachedMode(frame_pts);
            frame_pts += (error == CaptureError::CaptureErrorOK);
            SetLastError(error);
            continue;
        }

        auto slot = _captured.TryLockLatestSlot(MAX_LOCK_RETRY_COUNT);
        if (slot == nullptr) {
            SetLastError(CaptureError::CaptureErrorEmptyCapturedBuffer);
            continue;
        }

        auto eg1 = utils::make_scope_exit([slot]() {
            slot->Unlock();
        });

        if (slot->staging == nullptr) {
            SetLastError(CaptureError::CaptureErrorInvalidCapturedTexture);
            continue;
        }

        if (slot->staging_map.pData == nullptr) {
            SetLastError(CaptureError::CaptureErrorInvalidTextureMap);
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
        SetLastError(error);

        frame_pts += (error == CaptureError::CaptureErrorOK);
        FrameMarkNamed("Encoding_X264");
    }
}

auto EncoderX264::IsUsingStaging() const noexcept -> bool {
    return true;
}
}
