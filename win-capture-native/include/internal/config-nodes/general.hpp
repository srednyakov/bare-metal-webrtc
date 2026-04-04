#pragma once

#include <cstdint>

#ifndef YAML_CPP_STATIC_DEFINE
#define YAML_CPP_STATIC_DEFINE
#endif // YAML_CPP_STATIC_DEFINE

#include <yaml-cpp/yaml.h>

namespace cn::config {
/// @brief General settings
struct General {
    /// @brief Encoding width
    uint32_t width = 1920;

    /// @brief Encoding height
    uint32_t height = 1080;

    /// @brief Encoding FPS. Used for calculation of encoding/capture tick (1000ms/fps)
    uint32_t fps = 60;
};
}

namespace YAML {
template<>
struct convert<cn::config::General> {
    static auto decode(Node const& node, cn::config::General& rhs) -> bool {
        rhs.width = node["width"].as<uint32_t>(rhs.width);
        rhs.height = node["height"].as<uint32_t>(rhs.height);
        rhs.fps = node["fps"].as<uint32_t>(rhs.fps);
        return true;
    }
};
}

