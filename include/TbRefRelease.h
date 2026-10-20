// TbRefRelease.h: a COM-style reference count base (bullfrog library, functions at
// 0x10015538..0x10015680; the library names no file here, TbRefRelease.cpp follows the
// class). Vtable 0x10072690: Release at +8, AddRef at +0xc, the destructor at +0x10.
#pragma once
#include <cstdint>

class TbRefRelease {
public:
    TbRefRelease();                   // 0x10015538: one reference
    // Drops a reference; the last one deletes the object and returns 0.
    virtual int32_t Release();        // 0x10015630
    virtual int32_t AddRef();         // 0x100155f0: returns the new count
    virtual ~TbRefRelease();          // 0x10015578

    int32_t mRefCount;                // 0x04 (vptr at 0x00)
};
