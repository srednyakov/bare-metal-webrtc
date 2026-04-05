#pragma once

#include <stdint.h>

#include "errors.h"

// encoded H.264 frame
struct EncodedFrame {
    uint8_t const* data;
    uint32_t       size;
    uint64_t       pts;
    int            keyframe;
};
