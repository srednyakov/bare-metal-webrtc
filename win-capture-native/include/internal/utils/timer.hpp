#pragma once

#include <Windows.h>
#include <synchapi.h>
#include <chrono>

namespace cn::utils {
class Timer {
    HANDLE _timer{NULL};

public:

    Timer(std::chrono::milliseconds period, uint32_t ticks, int64_t wait_on_start = -1000000LL /*=100ms*/) {
        _timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);    
        auto start = LARGE_INTEGER{.QuadPart = wait_on_start};
        const auto ticker_delay_ms = (period / ticks).count();
        SetWaitableTimerEx(_timer, &start, ticker_delay_ms, nullptr, nullptr, nullptr, 0);
    }

    ~Timer() {
        CloseHandle(_timer);
    }

    auto Wait() noexcept -> DWORD {
        return WaitForSingleObject(_timer, INFINITE);
    }
};
}
