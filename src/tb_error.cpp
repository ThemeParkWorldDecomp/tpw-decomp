// tb_error.cpp (decompiled from the engine library of SimTheme Park 2.04b3).
// PARTIAL: CErrorHandler::Push (0x10008904), Pop (0x10008964), the static initialiser
// (0x10008a7c) and SError's inline constructor, translated with tb_capture.cpp;
// GetAltTabStatus (0x100089bc), translated with tb_resource_system.cpp.
#include "tb_error.h"

#include "tb_resource_system.h"

CErrorHandler gei_ErrorHandler;

void CErrorHandler::Push(int32_t level, int32_t code) {
    if (mCount >= 64) return;
    mStack[mCount].mLevel = level;
    mStack[mCount].mCode = code;
    mCount++;
}

SError* CErrorHandler::Pop() {
    if (mCount <= 0) return nullptr;
    mCount--;
    return &mStack[mCount];
}

int32_t CErrorHandler::GetAltTabStatus() {
    // Removes every lost-surface error (each is replaced by the current top entry, walking
    // down from the top) and reports whether there was one.
    bool found = false;
    SError* p = &mStack[mCount];
    for (int32_t i = mCount; i != 0; i--) {
        p--;
        if (static_cast<uint32_t>(p->mCode) == kErrSurfaceLost) {
            found = true;
            mCount--;
            *p = mStack[mCount];
        }
    }
    if (!found) return 0;
    gei_System.mFlags |= CSystem::kAltTab;
    return 1;
}
