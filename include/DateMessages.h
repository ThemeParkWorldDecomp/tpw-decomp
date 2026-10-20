// The park's date messages (main app, unit w129): CMsgEndOfDay (type 0xb), CMsgEndOfMonth (0xc) and CMsgEndOfYear (0xd), posted by CParkTime::Update.
// Each has a constructor (0x10151c24, 0x10151c6c, 0x10151cb4: the base vtable, a zero flags word, the message's own vtable; no payload), GetType (0x10112de0,
// 0x10112da8, 0x10112d70) and a destructor (0x1011261c, 0x101125a4, 0x1011252c) that only stores the vtable and, for a positive (signed 16 bit) delete flag,
// calls operator delete. The pattern is include/ThingMessages.h's; the Mac vtables (0x102a0d44, 0x102a0d20, 0x102a0cfc) are never used by the host.
#pragma once
#include <cstdint>

#include "CMsg.h"

class CMsgEndOfDay : public CMsg {
public:
    CMsgEndOfDay();                       // 0x10151c24 (28 bytes): flags 0
    int32_t GetType() const override;     // 0x10112de0 (8 bytes), answers 0xb; native vtable key function
    ~CMsgEndOfDay() override;             // 0x1011261c (76 bytes)
};

class CMsgEndOfMonth : public CMsg {
public:
    CMsgEndOfMonth();                     // 0x10151c6c (28 bytes): flags 0
    int32_t GetType() const override;     // 0x10112da8 (8 bytes), answers 0xc
    ~CMsgEndOfMonth() override;           // 0x101125a4 (76 bytes)
};

class CMsgEndOfYear : public CMsg {
public:
    CMsgEndOfYear();                      // 0x10151cb4 (28 bytes): flags 0
    int32_t GetType() const override;     // 0x10112d70 (8 bytes), answers 0xd
    ~CMsgEndOfYear() override;            // 0x1011252c (76 bytes)
};
