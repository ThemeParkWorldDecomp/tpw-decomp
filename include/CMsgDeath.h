// CMsgDeath, the "thing destroyed" message (type 0x1b). Unit w123c (src/MsgDeath.cpp) translates its constructor 0x10152264, GetType
// 0x1012ec14 (answers 0x1b) and destructor 0x1012e984; CMessageCentre::PostMessage reads the dying thing's id from it. The Mac's word at
// +0x08 holds the id (a uint16_t, from the tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short> argument); the host's native vtable
// moves it. The Mac vtables (base 0x102a10ec, death 0x102a438c) are never used by the host: the native vtable comes from the key
// function GetType in src/MsgDeath.cpp.
#pragma once
#include <cstdint>

#include "CMsg.h"
#include "analyser.h"

class CMsgDeath : public CMsg {
public:
    CMsgDeath() = default;   // test subclasses only (the tests' TestMessage)
    // 0x10152264 (40 bytes): flags 0 and the id; the argument's two bytes are the HIGH halfword of the Mac's register.
    CMsgDeath(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id);
    int32_t GetType() const override;   // 0x1012ec14 (8 bytes), answers 0x1b; native vtable key function
    // 0x1012e984 (76 bytes): the Mac only resets the vtable to the death one and, for a positive (signed 16 bit) delete flag, calls
    // operator delete; the C++ destructor and delete-expression are those two steps.
    ~CMsgDeath() override;
    uint16_t mThingId;   // 0x08 on the Mac
};
