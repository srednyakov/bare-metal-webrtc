#pragma once

#define WIN32_LEAN_AND_MEAN

#include <Windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <d3d11_4.h>
#include <wrl/client.h>

#include <type_traits>
#include <optional>
#include <atomic>
#include <thread>

#include "encoder.hpp"
#include "config.hpp"

namespace cn {
class Capturer {
    static constexpr auto MAX_LOCK_RETRY_COUNT = 3ul;

    ID3D11Device* _device{nullptr};
    ID3D11DeviceContext* _context{nullptr};
    IDXGIOutputDuplication* _duplication{nullptr};

    // video processor for GPU color conversion to NV12
    ID3D11VideoContext1* _video_context1{nullptr};
    ID3D11VideoDevice* _video_device{nullptr};
    ID3D11VideoProcessor* _video_processor{nullptr};
    ID3D11VideoProcessorEnumerator* _video_enumerator{nullptr};

    CapturedBuffer _captured{};
    Config _config;

    IDXGIFactory1* _factory;
    IEncoder* _encoder;

    std::atomic<CaptureError> _last_error{CaptureError::CAPTURE_ERROR_OK};
    std::thread _worker{};

private:
    auto SelectColorSpace(DXGI_FORMAT format) const noexcept -> DXGI_COLOR_SPACE_TYPE;

    auto InitializeDuplication() noexcept -> CaptureError;
    auto InitializeVideoProcessor() noexcept -> CaptureError;
    auto ReleaseDuplication() noexcept -> void;

    auto HandleCapturedTexture(Microsoft::WRL::ComPtr<ID3D11Texture2D> const& texture, 
        size_t frame_index) noexcept -> CaptureError;

    auto CaptureTexture(uint64_t frame_index) noexcept -> CaptureError;
    auto Worker() noexcept -> void;

public:
    template<class EncoderType, class ConfigType>
    Capturer(IDXGIFactory1* factory, ConfigType&& config, std::type_identity<EncoderType>) noexcept
        : _factory(factory),
          _config(std::forward<ConfigType>(config)),
          _encoder(new EncoderType(_device, _context, _duplication, _captured, _config)) {}

    ~Capturer() noexcept;

    auto Start() noexcept -> CaptureError;
    auto Stop() noexcept -> void;

    auto GetEncoded() noexcept -> EncodedBuffer&;
    auto GetLastCapturerWorkerError() const noexcept -> CaptureError;
    auto GetLastEncoderWorkerError() const noexcept -> CaptureError;

    Capturer(Capturer const&) = delete;
    Capturer(Capturer&&) = delete;
};

template<class EncoderType, class ConfigType>
static auto make_capturer(IDXGIFactory1* factory, ConfigType&& config) -> Capturer* {
    return new Capturer(factory, std::forward<ConfigType>(config), std::type_identity<EncoderType>{});
}
}
