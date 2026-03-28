#pragma once

#include <rigtorp/SPSCQueue.h>

namespace cn {
struct EncodedFrameInternal {
    std::vector<uint8_t> buffer{};
    uint64_t pts{};
    bool keyframe{};

public:
    template<class Type>
    EncodedFrameInternal(Type&& data_, uint64_t pts_, bool keyframe_) 
        : buffer(std::forward<Type>(data_)), 
          pts(pts_), 
          keyframe(keyframe_) {}
};

using EncodedBuffer = rigtorp::SPSCQueue<EncodedFrameInternal>;
}
