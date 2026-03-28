#pragma once

#include <string>
#include <cstdint>
#include <yaml-cpp/yaml.h>

namespace cn::config {
/// @brief X264 encoder specific settings
struct X264 {
    /// @brief X264 preset
    std::string preset = "veryfast";

    /// @brief X264 profile
    std::string profile = "high";

    /// @brief Constant Rate Factor (0-51; < == better)
    float rf_constant = 18.f;

    /// @brief Encoding threads count (X264_THREADS_AUTO == 0, X264_SYNC_LOOKAHEAD_AUTO == -1)
    int threads = 0;
};
}

namespace YAML {
template<>
struct convert<cn::config::X264> {
    static auto decode(Node const& node, cn::config::X264& rhs) -> bool {
        rhs.preset = node["preset"].as<std::string>(rhs.preset);
        rhs.profile = node["profile"].as<std::string>(rhs.profile);
        rhs.rf_constant = node["profile"].as<float>(rhs.rf_constant);
        rhs.threads = node["threads"].as<uint32_t>(rhs.threads);
        return true;
    }
};
}
