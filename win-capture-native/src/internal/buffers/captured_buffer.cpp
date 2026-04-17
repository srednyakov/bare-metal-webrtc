#include "buffers/captured_buffer.hpp"

namespace cn {
auto CapturedSlot::GetIndex() const noexcept -> uint64_t {
    // relaxed sufficient: racing reads are safe (atomic), TryLock will verify via expected_index
    return _index.load(std::memory_order_relaxed);
}

auto CapturedSlot::SetIndex(uint64_t index) noexcept -> void {
    // call only when _locked=true, we can use relaxed
    _index.store(index, std::memory_order_relaxed);
}

auto CapturedSlot::IsLocked() const noexcept -> bool {
    return _locked.load(std::memory_order_acquire);
}

auto CapturedSlot::TryLock(uint64_t expected_index) noexcept -> bool {
    auto expected = false;
    if (!_locked.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        return false;
    }

    if (expected_index != GetIndex()) {
        Unlock();
        return false;
    }

    return true;
}

auto CapturedSlot::Unlock() noexcept -> void {
    return _locked.store(false, std::memory_order_release);
}

auto CapturedSlot::Release(ID3D11DeviceContext* context) noexcept -> void {
    if (staging != nullptr && staging_map.pData != nullptr) {
        context->Unmap(staging, 0);
    }      

    RELEASE_POINTER(texture);
    RELEASE_POINTER(staging);

    texture_description = {};
    staging_description = {};
    staging_map = {};
}

auto CapturedBuffer::GetSlotByIndexWithoutLock(size_t index) noexcept -> CapturedSlot* {
    if (index >= _slots.size()) {
        return nullptr;
    }

    return &_slots[index];
}
    
auto CapturedBuffer::TryLockLatestSlot(size_t try_count) noexcept -> CapturedSlot*{
    for (auto i = size_t{0}; i < try_count; ++i) {
        auto best = static_cast<CapturedSlot*>(nullptr);
        auto best_index = uint64_t{0};

        for (decltype(auto) slot : _slots) {
            if (slot.IsLocked()) {
                continue;
            }

            const auto index = slot.GetIndex();
            if (index == 0) {
                continue;
            }

            if (best == nullptr || index > best_index) {
                best = &slot;
                best_index = index;
            }
        }

        if (best == nullptr) {
            return nullptr;
        }

        if (best->TryLock(best_index)) {
            return best;
        }

        _mm_pause();
    }

    return nullptr;
}

auto CapturedBuffer::TryLockOldestSlot(size_t try_count) noexcept -> CapturedSlot* {
    for (auto i = size_t{0}; i < try_count; ++i) {
        auto best = static_cast<CapturedSlot*>(nullptr);
        auto best_index = uint64_t{0};

        for (decltype(auto) slot : _slots) {
            if (slot.IsLocked()) {
                continue;
            }

            const auto index = slot.GetIndex();
            if (best == nullptr || index < best_index) {
                best = &slot;
                best_index = index;
                if (best_index == 0) {
                    break;
                }
            }
        }

        if (best == nullptr) {
            return nullptr;
        }

        if (best->TryLock(best_index)) {
            return best;
        }

        _mm_pause();
    }

    return nullptr;
}
}
