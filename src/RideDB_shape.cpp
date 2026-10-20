// RideDB.cpp, main app: ProcessShape 0x101557a4 (536 bytes).
#include "TPEngine.h"

void CRideDatabase::ProcessShape(RideInfo** ride) {
    auto* shape = static_cast<RideShape*>((*ride)->Value(kRideSlot_Shape).mShape);
    int32_t entryX = 0;
    int32_t entryY = 0;
    uint8_t entryFlag = 0;
    bool entrance = false;
    for (int32_t x = 0; x < shape->mWidth && !entrance; ++x) {
        for (int32_t y = 0; y < shape->mHeight; ++y) {
            if (shape->mCells[y][x].mValue == 9) {
                entryX = x;
                entryY = y;
                entryFlag = shape->mCells[y][x].mFlag;
                entrance = true;
                break;
            }
        }
    }
    if (entrance) {
        (*ride)->mEntryX = entryX;
        (*ride)->mEntryY = entryY;
        (*ride)->mEntryFlag = entryFlag;
        shape = static_cast<RideShape*>((*ride)->Value(kRideSlot_Shape).mShape);
        bool exit = false;
        int32_t exitX = 0;
        int32_t exitY = 0;
        uint8_t exitFlag = 0;
        for (int32_t x = 0; x < shape->mWidth && !exit; ++x) {
            for (int32_t y = 0; y < shape->mHeight; ++y) {
                if (shape->mCells[y][x].mValue == 10) {
                    exitX = x;
                    exitY = y;
                    exitFlag = shape->mCells[y][x].mFlag;
                    exit = true;
                    break;
                }
            }
        }
        (*ride)->mExitX = exit ? exitX : entryX;
        (*ride)->mExitY = exit ? exitY : entryY;
        (*ride)->mExitFlag = exit ? exitFlag : entryFlag;
    } else {
        (*ride)->mEntryX = 0;
        (*ride)->mEntryY = 0;
        (*ride)->mEntryFlag = 1;
        (*ride)->mExitX = 0;
        (*ride)->mExitY = 0;
        (*ride)->mExitFlag = 0x10;
    }
    const int32_t offsetY = (*ride)->Value(kRideSlot_MapOffsetY).mInteger;
    const int32_t offsetX = (*ride)->Value(kRideSlot_MapOffsetX).mInteger;
    (*ride)->mMapOffsetX = offsetX;
    (*ride)->mMapOffsetY = offsetY;
    (*ride)->mId = static_cast<uint16_t>((*ride)->Value(kRideSlot_Id).mInteger);
    (*ride)->mUIType = (*ride)->Value(kRideSlot_WhichUIType).mInteger;
}
