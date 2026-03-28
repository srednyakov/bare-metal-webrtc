#pragma once

#include "config-nodes/general.hpp"
#include "config-nodes/x264.hpp"

#include <yaml-cpp/yaml.h>

namespace cn {
struct Config {
    /// @brief General settings
    config::General general;

    /// @brief X264 specific settings
    config::X264 x264;
};
}

namespace YAML {
template<>
struct convert<cn::Config> {
    static auto decode(const Node& node, cn::Config& rhs) -> bool {
        rhs.general = node["general"].as<cn::config::General>(rhs.general);
        rhs.x264 = node["x264"].as<cn::config::X264>(rhs.x264);
        return true;
    }
};
}
