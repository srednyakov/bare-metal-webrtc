#pragma once

#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

#include <optional>
#include <atomic>
#include <thread>

#include "buffers/captured_buffer.hpp"
#include "buffers/encoded_buffer.hpp"

#include "config.hpp"

#define RELEASE_POINTER(ptr)\
if (ptr != nullptr) {\
    ptr->Release();\
    ptr = nullptr;\
}

namespace cn {
inline auto operator==(D3D11_TEXTURE2D_DESC const& a, D3D11_TEXTURE2D_DESC const& b) -> bool {
    return (a.Width == b.Width) && (a.Height == b.Height);
}

enum class EncoderType {
    ENCODER_X264,
};

struct EncoderConfig {
    uint32_t width;
    uint32_t height;
    uint32_t fps;
};

class IEncoder {
protected:
    ID3D11Device*& _device;
    ID3D11DeviceContext*& _context;
    IDXGIOutputDuplication*& _duplication;

    Config& _config;
    
    CapturedBuffer& _captured;
    EncodedBuffer _encoded{64};

    std::atomic<CaptureError> _last_error{CaptureError::CAPTURE_ERROR_OK};
    std::atomic_bool _running{false};
    std::thread _worker{};

public:
    IEncoder(ID3D11Device*& device, ID3D11DeviceContext*& context, IDXGIOutputDuplication*& duplication, CapturedBuffer& captured, Config& config) noexcept
        : _device(device),
          _context(context),
          _duplication(duplication),
          _captured(captured),
          _config(config) {}
          
    virtual ~IEncoder() noexcept = default;

    virtual auto Start() noexcept -> CaptureError = 0;
    virtual auto Stop() noexcept -> void = 0;

    virtual auto IsUsingStaging() const noexcept -> bool = 0;

    auto GetEncoded() -> EncodedBuffer& {
        return _encoded;
    }

    auto IsRunning() -> bool {
        return _running.load(std::memory_order_relaxed);
    }

    auto GetLastWorkerError() const -> CaptureError {
        return _last_error.load(std::memory_order_relaxed);
    }
};
}
