// VanillaTime.cpp: main app, 0x101686a4..0x1016886c.
#pragma once
#include <cstdint>

class CVanillaTime {
public:
    bool Save(void* file); // 0x101686a4
    bool Load(void* file); // 0x10168770
    uint32_t mTimeOffset;  // 0x00, saved clock minus current clock
};
extern CVanillaTime gVanillaTime; // 0x10380a04
