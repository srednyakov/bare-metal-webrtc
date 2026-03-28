#include "capturer_fabric.hpp"

namespace cn {
CapturerFabric::~CapturerFabric() {
    RELEASE_POINTER(_factory);
}

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
