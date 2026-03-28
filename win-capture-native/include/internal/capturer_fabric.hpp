#pragma once

#include <optional>
#include <utility>
#include <string>

#include "capturer.hpp"

namespace cn {
class CapturerFabric {
    IDXGIFactory1* _factory{nullptr};

private:
    CapturerFabric() = default;

public:
    CapturerFabric(CapturerFabric const&) = delete;
    CapturerFabric(CapturerFabric&&) = delete;

    ~CapturerFabric();

    template<class EncoderType>
    auto CreateCapturer(std::string const& config_path) -> std::pair<Capturer*, CaptureError> {
        if (_factory == nullptr && FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&_factory)))) {
            return {nullptr, CaptureError::CAPTURE_ERROR_FAILED_CREATE_DXGI_FACTORY};
        }

        auto config = Config{};

        if (!config_path.empty()) {
            try {
                const auto node = YAML::LoadFile(config_path);
                config = node.as<Config>();
            } catch (...) {
                return {nullptr, CaptureError::CAPTURE_ERROR_FAILED_CONFIG_LOAD};
            }
        }

        return {make_capturer<EncoderType>(_factory, std::move(config)), CaptureError::CAPTURE_ERROR_OK};
    }

    auto DeleteCapturer(Capturer* capturer) -> void;

    static auto Instance() -> CapturerFabric&;
};
}
