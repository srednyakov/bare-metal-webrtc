#pragma once

#include "encoder.hpp"

#include <x264.h>
#include <vector>
#include <thread>

namespace cn {
struct RawCpuFrame {
    uint8_t const* data;
    int            pitch;
    int            width;
    int            height;
    uint64_t       pts;
};

class EncoderX264 : public IEncoder {
    static constexpr auto MAX_LOCK_RETRY_COUNT = 3ul;

    // x264 encoder state
    x264_t* _x264_encoder{nullptr};
    x264_param_t _x264_param{};
    x264_picture_t _x264_pic_in{};
    x264_picture_t _x264_pic_out{};

private:
    auto InitializeX264() -> CaptureError;

    auto HandleRawCpuFrame(RawCpuFrame const& frame) noexcept -> CaptureError;
    auto Worker() noexcept -> void;

public:
    template<class ConfigType>
    EncoderX264(ID3D11Device*& device, ID3D11DeviceContext*& context, IDXGIOutputDuplication*& duplication, CapturedBuffer& captured, ConfigType&& config)
        : IEncoder(device, context, duplication, captured, std::forward<ConfigType>(config)) {};

    ~EncoderX264();

    auto Start() noexcept -> CaptureError override;
    auto Stop() noexcept -> void override;

    auto IsUsingStaging() const noexcept -> bool override;
};
}
