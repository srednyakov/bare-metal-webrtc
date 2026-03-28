#pragma once

#include <immintrin.h>
#include <d3d11.h>

#include <vector>
#include <atomic>
#include <array>

#include "types.h"

namespace cn {
class CapturedSlot {
    std::atomic_bool _locked{false};
    std::atomic_uint64_t _index{0};

public:
    // GPU texture & description
    D3D11_TEXTURE2D_DESC texture_description{};
    ID3D11Texture2D* texture{nullptr};

    // used for CPU enocders
    ID3D11Texture2D* staging{nullptr};
    D3D11_TEXTURE2D_DESC staging_description{};
    D3D11_MAPPED_SUBRESOURCE staging_map{};

    auto GetIndex() const -> uint64_t;
    auto SetIndex(uint64_t index) -> void;

    auto IsLocked() const -> bool;

    auto TryLock(uint64_t expected_index) -> bool;
    auto Unlock() -> void;
};

class CapturedBuffer {
    std::array<CapturedSlot, 4> _slots{};

public:
    constexpr auto GetSize() const noexcept -> size_t {
        return _slots.size();
    }

    /// @brief Gets slot by index without lock (use only for initialization)
    /// @param[in] index - number of slot
    /// @return Frame pointer or nullptr if lock failed
    auto GetSlotByIndexWithoutLock(size_t index) noexcept -> CapturedSlot*;

    /// @brief Locks frame with maximal index (used for read)
    /// @param[in] try_count - Optional. Count of lock attempts (1 by default)
    /// @return Slot pointer or nullptr if lock failed
    auto TryLockLatestSlot(size_t try_count = 1) noexcept -> CapturedSlot*;

    /// @brief Locks frame with minimal index (used for write)
    /// @param[in] try_count - Optional. Count of lock attempts (1 by default)
    /// @return Slot pointer or nullptr if lock failed
    auto TryLockOldestSlot(size_t try_count = 1) noexcept -> CapturedSlot*;
};
}
