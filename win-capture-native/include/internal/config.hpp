#pragma once

#include "config-nodes/output.hpp"
#include "config-nodes/x264.hpp"

#ifndef YAML_CPP_STATIC_DEFINE
#define YAML_CPP_STATIC_DEFINE
#endif // YAML_CPP_STATIC_DEFINE

#include <yaml-cpp/yaml.h>

namespace cn {
struct Config {
    /// @brief General settings
    config::Output output;

    /// @brief X264 specific settings
    config::X264 x264;
};
}

namespace YAML {
template<>
struct convert<cn::Config> {
    static auto decode(const Node& node, cn::Config& rhs) -> bool {
        rhs.output = node["output"].as<cn::config::Output>(rhs.output);
        rhs.x264 = node["x264"].as<cn::config::X264>(rhs.x264);
        return true;
    }
};
}
