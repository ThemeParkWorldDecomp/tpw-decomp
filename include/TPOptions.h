// TPOptions.cpp: main app COptions (no state used by its getters or destructor).
#pragma once
#include <cstdint>
class COptions {
public:
    ~COptions(); // 0x1015f1a4
    int32_t GetNumKidSprites() const;   // 0x1015f20c
    int32_t GetNumStaffSprites() const; // 0x1015f30c
};
extern COptions gOptions; // 0x1038070c
