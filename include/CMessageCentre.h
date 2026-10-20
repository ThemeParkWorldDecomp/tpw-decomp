// CMessageCentre: PARTIAL. PostMessage at 0x1015256c. Its lazy allocation constructs
// 29 CSubscriptionHeader objects (0x1d0 bytes on PowerPC), with no separate constructor.
#pragma once
#include <cstdint>

#include "CMsg.h"
#include "CSubscriptionHeader.h"

class CThing;
// The Mac symbol is Subscribe__14CMessageCentreFRC6CThing7MsgType: a global type named MsgType (one register; the callers pass small
// constants; unit w122a).
enum MsgType : int32_t {};

class CMessageCentre {
public:
    // 0x10152388 (128 bytes, untranslated): with type < 0x1d (signed), puts the thing's id in the subscription tree of that type's header.
    bool Subscribe(const CThing& thing, MsgType type);
    // 0x1015256c (unit w107): the answer is always 1. Type 0x1b (CMsgDeath) first removes the dying thing's subscription from all 29 headers.
    int32_t PostMessage(const CMsg& message);
    int32_t Load(void*& file);   // 0x10152840 (184 bytes, stand-in): 0 when loaded
    int32_t Save(void*& file) const;   // 0x10152758 (184 bytes, stand-in; unit v5628): 0 when saved
    CSubscriptionHeader mSubscriptions[0x1d];   // 0x00
    // Unit v5760: 0x10152448 (228 bytes): erases the thing's id from every header's tree (header i's tree sits at Mac offset 0x10 * i + 4);
    // answers how many trees held it.
    int32_t UnsubscribeAll(const CThing& thing);
};

static_assert(sizeof(CMessageCentre) == 0x1d0, "29 headers of 0x10 bytes");

extern CMessageCentre* gMessageCentre;   // main app BSS 0x1037f6c4, name not recovered
