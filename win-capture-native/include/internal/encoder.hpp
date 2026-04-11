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

    std::atomic<CaptureError> _last_error{CaptureError::CaptureErrorOK};
    std::atomic_bool _running{false};
    std::thread _worker{};

    std::atomic_bool _use_cached{false};
    std::atomic_bool _cached_ready{false};

protected:
    auto SetLastError(CaptureError error) noexcept -> void {
        _last_error.store(error, std::memory_order_relaxed);
    }

public:
    IEncoder(ID3D11Device*& device, ID3D11DeviceContext*& context, IDXGIOutputDuplication*& duplication, CapturedBuffer& captured, Config& config) noexcept
        : _device(device),
          _context(context),
          _duplication(duplication),
          _captured(captured),
          _config(config) {}
          
    virtual ~IEncoder() noexcept = default;

    virtual auto Start(int width, int height) noexcept -> CaptureError = 0;
    virtual auto Stop() noexcept -> void = 0;

    virtual auto IsUsingStaging() const noexcept -> bool = 0;

    auto SetUseCached(bool state) noexcept -> void {
        if (GetUseCached() != state) {
            _cached_ready.store(false, std::memory_order_relaxed);
        }
        _use_cached.store(state, std::memory_order_relaxed);
    }

    auto GetUseCached() const noexcept -> bool {
        return _use_cached.load(std::memory_order_relaxed);
    }

    auto IsCachedReady() const noexcept -> bool {
        return _cached_ready.load(std::memory_order_relaxed);
    }
    
    auto GetEncoded() noexcept -> EncodedBuffer& {
        return _encoded;
    }

    auto IsRunning() const noexcept -> bool {
        return _running.load(std::memory_order_relaxed);
    }

    auto GetLastWorkerError() const noexcept -> CaptureError {
        return _last_error.load(std::memory_order_relaxed);
    }
};
}
