#include "capture.h" // using external API for test
#include "config.hpp"

#include "utils/timer.hpp"

#include <tracy/Tracy.hpp>
#include <thread>
#include <print>

using namespace std::chrono_literals;

namespace {
constexpr auto EXPECTED_SECONDS = 60;
constexpr auto WRITE_TO_FILE = true;
}

auto main() -> int {
    auto config = cn::Config{};

    try {
        const auto node = YAML::LoadFile("smoke_config.yaml");
        config = node.as<cn::Config>();
    } catch (...) {
        std::println("failed to load config");
        return 1;
    }

    auto capturer = static_cast<void*>(nullptr);

    auto error = capture_native_create_capturer_x264("smoke_config.yaml", &capturer);
    if (error != CaptureError::CaptureErrorOK) {
        std::println("failed capture_native_create_capturer_x264: {}", error);
        return 1;
    }

    error = capture_native_start_capturer(capturer);
    if (error != CaptureError::CaptureErrorOK) {
        std::println("failed capture_native_start_capturer: {}", error);
        return 1;
    }

    auto file = static_cast<FILE*>(nullptr);

    if constexpr (WRITE_TO_FILE) {
        file = std::fopen("smoke_output.h264", "wb");
    }

    auto timer = cn::utils::Timer(1s, config.general.fps);

    const auto expected_pts = (EXPECTED_SECONDS * config.general.fps);
    auto pts = uint64_t{0};

    while (pts != expected_pts) {
        timer.Wait();

        auto frame = EncodedFrame{};

        {
            ZoneScopedNC("capture_native_get_frame", tracy::Color::SpringGreen);
            frame = capture_native_get_frame(capturer);
        }

        if (frame.data == nullptr) {
            const auto capturer_error = capture_native_get_last_capturer_error(capturer);
            const auto encoder_error = capture_native_get_last_encoder_error(capturer);
            if (capturer_error != CaptureError::CaptureErrorOK || encoder_error != CaptureError::CaptureErrorOK) {
                std::println("capturer_error: {} | encoder_error: {}", capturer_error, encoder_error);
            }
            continue;
        }

        if (file != nullptr) {
            std::fwrite(frame.data, 1, frame.size, file);
        }

        if (frame.pts != 0 && frame.pts % 60 == 0) {
            std::println("Done {}/{}", frame.pts, expected_pts);
        }

        pts = frame.pts;
        capture_native_pop_frame(capturer);
    }

    if (file != nullptr) {
        std::fclose(file);
    }

    capture_native_delete_capturer(capturer);
    std::this_thread::sleep_for(2s);
    
    return 0;
}
