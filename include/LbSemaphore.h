// Bullfrog semaphore. Its implementation is outside M1 u11.
#pragma once
#include <cstdint>
class _LbSemaphore {
public:
    _LbSemaphore();
    ~_LbSemaphore();
    uint32_t mState;     // Mac +0, 4 bytes; constructor leaves it untouched
};

// The lock in SharedAccess copies the semaphore word and clears the two status words.
// These library calls are external to the u11 access-class translations.
class _LbSemaLock {
public:
    explicit _LbSemaLock(const _LbSemaphore& semaphore)
        : mState(semaphore.mState), mWaiting(0), mLocked(0) {}
    ~_LbSemaLock();                   // 0x10024b00 (src/bullfrog/LbSemaphore.cpp)
    int32_t Lock(int32_t wait);       // 0x10024b6c (src/bullfrog/LbSemaphore.cpp): always 1; `wait` is not used
    uint32_t mState;                 // Mac +0
    uint32_t mWaiting;               // +4
    uint32_t mLocked;                // +8
};
