#include "capturer_fabric.hpp"

namespace cn {
auto CapturerFabric::DeleteCapturer(Capturer* capturer) -> void {
    if (capturer != nullptr) {
        delete capturer;
    }
}

auto CapturerFabric::Instance() -> CapturerFabric& {
    static auto instance = CapturerFabric{};
    return instance;
}
}
