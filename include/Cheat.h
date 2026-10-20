// CCheat: main app 0x10146f24..0x10147294. Original file name unknown.
#pragma once
#include <cstdint>

class CCheat {
public:
    CCheat();
    void SetCheatStatus(bool cheating);
    // The original returns the stored byte without normalising data loaded from files.
    uint8_t IsCheating();
    int32_t Load(void*& file);
    int32_t Save(void*& file) const;

    uint8_t mUnknown[9];   // 0x00  untouched by the recovered functions
    uint8_t mFlag09;       // 0x09  cleared by construction, not saved
    uint8_t mCheating;     // 0x0a
    uint8_t mFlag0B;       // 0x0b  second saved byte, meaning not yet recovered
};
static_assert(sizeof(CCheat) == 12, "CCheat layout");
