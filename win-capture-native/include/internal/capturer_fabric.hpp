#pragma once

#include <optional>
#include <utility>
#include <string>

#include "capturer.hpp"

namespace cn {
class CapturerFabric {
    CapturerFabric() = default;

public:
    CapturerFabric(CapturerFabric const&) = delete;
    CapturerFabric(CapturerFabric&&) = delete;

    template<class EncoderType>
    auto CreateCapturer(std::string const& config_path) -> std::pair<Capturer*, CaptureError> {
        auto config = Config{};

        if (!config_path.empty()) {
            try {
                const auto node = YAML::LoadFile(config_path);
                config = node.as<Config>();
            } catch (...) {
                return {nullptr, CaptureError::CaptureErrorFailedConfigLoad};
            }
        }

        return {make_capturer<EncoderType>(std::move(config)), CaptureError::CaptureErrorOK};
    }

    auto DeleteCapturer(Capturer* capturer) -> void;

    static auto Instance() -> CapturerFabric&;
};
}
