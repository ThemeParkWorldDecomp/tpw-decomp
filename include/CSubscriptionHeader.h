// CSubscriptionHeader: PARTIAL, message subscriptions. Constructor 0x1000a3f8, destructor 0x101522e0 (unit w107; Save 0x10152928,
// Load 0x10152a10 and the subscribe path, insert_one 0x101530e4 and insert_node_at 0x101538d8, are untranslated).
// Each original header is 0x10 bytes: +0 a word nothing touches, +4 the subscription tree (count, root, begin). The host keeps the
// 0x10 bytes (CMessageCentre's size is relied on elsewhere): see include/MslSubscriptionTree.h for what the tree keeps.
#pragma once
#include <cstdint>

#include "MslSubscriptionTree.h"

class CSubscriptionHeader {
public:
    CSubscriptionHeader();
    ~CSubscriptionHeader();
    union {
        uint8_t mState[0x10];       // 0x00  the raw bytes (the older test shims read and write them as words)
        SubscriptionTree mTree;     // host layout: count at +0, root at +8; the Mac's tree sits at +4
    };
    int32_t Save(void*& file) const;   // 0x10152928 (180): the count, then every subscription; answers 0
    int32_t Load(void*& file);         // 0x10152a10 (196): the count, then that many inserts; -1 when one is refused
};
static_assert(sizeof(CSubscriptionHeader) == 0x10, "a header is 0x10 bytes");
