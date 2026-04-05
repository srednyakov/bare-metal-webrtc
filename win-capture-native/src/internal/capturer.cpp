#include "capturer.hpp"

#include "utils/scope_exit.hpp"
#include "utils/timer.hpp"

#include <tracy/Tracy.hpp>

using namespace std::chrono_literals;

namespace cn {
Capturer::~Capturer() noexcept {
    Stop();
    ReleaseDuplication();
    delete _encoder;
}

auto Capturer::SelectColorSpace(DXGI_FORMAT format) const noexcept -> DXGI_COLOR_SPACE_TYPE {
    switch (format) {
        case DXGI_FORMAT_R16G16B16A16_FLOAT: return DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709;
        case DXGI_FORMAT_R10G10B10A2_UNORM: return DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020;
        default: return DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
    }
}

auto Capturer::InitializeDuplication() noexcept -> CaptureError {
    ReleaseDuplication();

    if (_factory == nullptr) {
        return CaptureError::CaptureErrorFailedCreateDXGIFactory;
    }

    if (_duplication != nullptr) {
        return CaptureError::CaptureErrorOK;
    }

    auto adapter = Microsoft::WRL::ComPtr<IDXGIAdapter1>{};

    if (FAILED(_factory->EnumAdapters1(0, &adapter))) {
        return CaptureError::CaptureErrorFailedEnumAdapters;
    }

    auto flags = UINT{D3D11_CREATE_DEVICE_BGRA_SUPPORT};
#ifdef _DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    auto feature_level = D3D_FEATURE_LEVEL{};

    const auto result = D3D11CreateDevice(adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, flags,
        nullptr, 0, D3D11_SDK_VERSION, &_device, &feature_level, &_context);

    if (FAILED(result)) {
        return CaptureError::CaptureErrorFailedD3D11CreateDevice;
    }

    // Query video context and device for GPU color conversion
    if (FAILED(_context->QueryInterface(IID_PPV_ARGS(&_video_context1)))) {
        return CaptureError::CaptureErrorFailedD3D11CreateDevice;
    }

    if (FAILED(_device->QueryInterface(IID_PPV_ARGS(&_video_device)))) {
        return CaptureError::CaptureErrorFailedD3D11CreateDevice;
    }

    auto output = Microsoft::WRL::ComPtr<IDXGIOutput>{};

    if (FAILED(adapter->EnumOutputs(0, &output))) {
        return CaptureError::CaptureErrorFailedEnumOutputs;
    }

    // auto outputDescription = DXGI_OUTPUT_DESC{};
    // output->GetDesc(&outputDescription);
    // std::printf("Using output: %ls\n", outputDescription.DeviceName);

    auto output1 = Microsoft::WRL::ComPtr<IDXGIOutput1>{};

    if (FAILED(output.As(&output1))) {
        return CaptureError::CaptureErrorFailedOutputConvert;
    }

    if (FAILED(output1->DuplicateOutput(_device, &_duplication))) {
        return CaptureError::CaptureErrorFailedDuplicateOutput;
    }

    return InitializeVideoProcessor();
}

auto Capturer::InitializeVideoProcessor() noexcept -> CaptureError {
    if (_video_processor != nullptr) {
        return CaptureError::CaptureErrorOK; // already initialized
    }

    if (_video_device == nullptr || _video_context1 == nullptr) {
        return CaptureError::CaptureErrorFailedCreateDXGIFactory;
    }

    const auto description = D3D11_VIDEO_PROCESSOR_CONTENT_DESC{
        .InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE,
        .InputWidth = _config.general.width,
        .InputHeight = _config.general.height,
        .OutputWidth = _config.general.width,
        .OutputHeight = _config.general.height,
        .Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL,
    };

    if (FAILED(_video_device->CreateVideoProcessorEnumerator(&description, &_video_enumerator))) {
        return CaptureError::CaptureErrorFailedD3D11CreateDevice;
    }

    if (FAILED(_video_device->CreateVideoProcessor(_video_enumerator, 0, &_video_processor))) {
        return CaptureError::CaptureErrorFailedD3D11CreateDevice;
    }

    return CaptureError::CaptureErrorOK;
}

auto Capturer::ReleaseDuplication() noexcept -> void {
    RELEASE_POINTER(_video_processor);
    RELEASE_POINTER(_video_enumerator);
    RELEASE_POINTER(_video_context1);
    RELEASE_POINTER(_video_device);
    RELEASE_POINTER(_device);
    RELEASE_POINTER(_context);
    RELEASE_POINTER(_duplication);
}

auto Capturer::HandleCapturedTexture(Microsoft::WRL::ComPtr<ID3D11Texture2D> const& texture, 
    size_t frame_index) noexcept -> CaptureError {

    ZoneScopedNC(__FUNCTION__, tracy::Color::Purple);

    auto slot = _captured.TryLockOldestSlot(MAX_LOCK_RETRY_COUNT);
    if (slot == nullptr) {
        return CaptureError::CaptureErrorFailedCapturedSlotLock;
    }

    auto exit_guard = utils::make_scope_exit([slot]() {
        slot->Unlock();
    });

    if (slot->staging != nullptr && slot->staging_map.pData != nullptr) {
        _context->Unmap(slot->staging, 0); // unmap previous
        slot->staging_map = {};
    }

    auto source_description = D3D11_TEXTURE2D_DESC{};
    texture->GetDesc(&source_description);

    if (slot->texture == nullptr || slot->texture_description != source_description) {
        RELEASE_POINTER(slot->texture);

        auto new_description = D3D11_TEXTURE2D_DESC{
            .Width      = source_description.Width,
            .Height     = source_description.Height,
            .MipLevels  = 1,
            .ArraySize  = 1,
            .Format     = DXGI_FORMAT_NV12,
            .SampleDesc = {1, 0},
            .Usage      = D3D11_USAGE_DEFAULT,
            .BindFlags  = D3D11_BIND_RENDER_TARGET,
        };

        if (FAILED(_device->CreateTexture2D(&new_description, nullptr, &slot->texture))) {
            return CaptureError::CaptureErrorFailedCreateTexture2D;
        }

        slot->texture_description = new_description;
    }

    const auto output_view_desc = D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC{
        .ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D,
        .Texture2D     = {.MipSlice = 0},
    };

    auto output_view = Microsoft::WRL::ComPtr<ID3D11VideoProcessorOutputView>{};
    if (FAILED(_video_device->CreateVideoProcessorOutputView(slot->texture, _video_enumerator, &output_view_desc, &output_view))) {
        return CaptureError::CaptureErrorFailedCreateOutputView;
    }

    const auto input_view_desc = D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC{
        .FourCC        = 0,
        .ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D,
        .Texture2D     = {.MipSlice = 0},
    };

    auto input_view = Microsoft::WRL::ComPtr<ID3D11VideoProcessorInputView>{};
    if (FAILED(_video_device->CreateVideoProcessorInputView(texture.Get(), _video_enumerator, &input_view_desc, &input_view))) {
        return CaptureError::CaptureErrorFailedCreateInputView;
    }

    _video_context1->VideoProcessorSetStreamColorSpace1(_video_processor, 0, SelectColorSpace(source_description.Format));
    _video_context1->VideoProcessorSetOutputColorSpace1(_video_processor, DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P709);

    const auto stream = D3D11_VIDEO_PROCESSOR_STREAM{
        .Enable       = TRUE,
        .pInputSurface = input_view.Get(),
    };

    if (FAILED(_video_context1->VideoProcessorBlt(_video_processor, output_view.Get(), 0, 1, &stream))) {
        return CaptureError::CaptureErrorFailedVideoProcessorBlt;
    }

    if (!_encoder->IsUsingStaging()) {
        slot->SetIndex(frame_index);
        return CaptureError::CaptureErrorOK;
    }

    if (slot->staging == nullptr || slot->staging_description != slot->texture_description) {
        RELEASE_POINTER(slot->staging);

        auto new_description = slot->texture_description;

        new_description.Usage = D3D11_USAGE_STAGING;
        new_description.BindFlags = 0;
        new_description.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        new_description.MiscFlags = 0;

        if (FAILED(_device->CreateTexture2D(&new_description, nullptr, &slot->staging))) {
            return CaptureError::CaptureErrorFailedCreateTexture2D;
        }

        slot->staging_description = new_description;
    }

    _context->CopyResource(slot->staging, slot->texture);

    if (FAILED(_context->Map(slot->staging, 0, D3D11_MAP_READ, 0, &slot->staging_map))) {
        return CaptureError::CaptureErrorFailedD3D11Map;
    }

    slot->SetIndex(frame_index);
    return CaptureError::CaptureErrorOK;
}

auto Capturer::CaptureTexture(uint64_t frame_index) noexcept -> CaptureError {
    if (_duplication == nullptr) {
        return CaptureError::CaptureErrorInvalidDuplicate;
    }

    auto frame_info = DXGI_OUTDUPL_FRAME_INFO{};
    auto resource = Microsoft::WRL::ComPtr<IDXGIResource>{};

    {
        ZoneScopedNC("CaptureTexture.AcquireNextFrame", tracy::Color::Azure);
        const auto result = _duplication->AcquireNextFrame(INFINITE, &frame_info, &resource);

        if (result == DXGI_ERROR_WAIT_TIMEOUT) {
            return CaptureError::CaptureErrorOK; // no new frame
        } else if (result == DXGI_ERROR_ACCESS_LOST) {
            return CaptureError::CaptureErrorAccessLost; // need to reinitialize duplication
        } else if (FAILED(result)) {
            return CaptureError::CaptureErrorFailedAcquireNextFrame;
        }
    }

    auto exit_guard = utils::make_scope_exit([this]() {
        _duplication->ReleaseFrame();
    });

    auto texture = Microsoft::WRL::ComPtr<ID3D11Texture2D>{};

    if (FAILED(resource.As(&texture))) {
        return CaptureError::CaptureErrorFailedResourceConvert;
    }

    return HandleCapturedTexture(texture, frame_index);
}

auto Capturer::Worker() noexcept -> void {
    auto timer = utils::Timer(950ms, _config.general.fps, 0); // capturer`s ticker a little faster than encoder
    auto captured_index = uint64_t{0};

    while (_encoder->IsRunning()) {
        timer.Wait();

        const auto error = CaptureTexture(captured_index);
        _last_error.store(error, std::memory_order_relaxed);

        FrameMarkNamed("DXGI_Capture");

        if (error == CaptureError::CaptureErrorOK) {
            captured_index += 1;
            continue;
        }

        if (error != CaptureError::CaptureErrorAccessLost || !_encoder->IsRunning()) {
            continue;
        }
        
        // reinit duplication
        InitializeDuplication();
    }
}

auto Capturer::Start() noexcept -> CaptureError {
    auto error = InitializeDuplication();
    if (error != CaptureError::CaptureErrorOK) {
        return error;
    }

    error = InitializeVideoProcessor();
    if (error != CaptureError::CaptureErrorOK) {
        return error;
    }

    error = _encoder->Start();
    if (error != CaptureError::CaptureErrorOK) {
        return CaptureError::CaptureErrorFailedEncoderInitialization;
    }

    _worker = std::thread(&Capturer::Worker, this);
    return CaptureError::CaptureErrorOK;
}

auto Capturer::Stop() noexcept -> void {
    _encoder->Stop();

    for (auto i = size_t{0}; i < _captured.GetSize(); ++i) {
        auto slot = _captured.GetSlotByIndexWithoutLock(i);
        RELEASE_POINTER(slot->texture);
        slot = {};
    }

    if (_worker.joinable()) {
        _worker.join();
    }
}

auto Capturer::GetEncoded() noexcept -> EncodedBuffer& {
    return _encoder->GetEncoded();
}

auto Capturer::GetLastCapturerWorkerError() const noexcept -> CaptureError {
    return _last_error.load(std::memory_order_relaxed);
}

auto Capturer::GetLastEncoderWorkerError() const noexcept -> CaptureError {
    return _encoder->GetLastWorkerError();
}
}
