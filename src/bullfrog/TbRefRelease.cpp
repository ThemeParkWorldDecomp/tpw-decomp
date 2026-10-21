// TbRefRelease.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of TbRefRelease code, named after it.

#include "TbRefRelease.h"

// TbRefRelease.cpp (decompiled from the bullfrog library of SimTheme Park 2.04b3,
// functions at 0x10015538..0x10015680; original file name unknown, see TbRefRelease.h).

TbRefRelease::TbRefRelease() : mRefCount(1) {}

TbRefRelease::~TbRefRelease() {}

int32_t TbRefRelease::AddRef() {
    return ++mRefCount;
}

int32_t TbRefRelease::Release() {
    if (--mRefCount == 0) {
        delete this;
        return 0;
    }
    return mRefCount;
}
