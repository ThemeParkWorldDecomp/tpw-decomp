// CMsg: PARTIAL, untranslated message base. Original vtable at +0, flags at +4.
// Native C++ supplies the vtable; pointer size changes the host layout.
#pragma once
#include <cstdint>

class CMsg {
public:
    virtual ~CMsg() = default;
    virtual int32_t GetType() const = 0;
    uint32_t mFlags;   // 0x04
};
