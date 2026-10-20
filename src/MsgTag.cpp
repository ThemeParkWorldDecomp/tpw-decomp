// The advisor's tag message (CMsgTag on the Mac, "MsgTag.cpp?" in the inventory): its constructor 0x10151f7c (52 bytes). GetType
// (0x1000e90c, 0x18) is inline in include/AdvisorQueue.h.
#include <cstddef>
#include <cstdint>

#include "AdvisorQueue.h"
#include "ThingReceivers.h"

// CTagSystem::ReceiveMessage reads the message as a CMsgTagShow: the two classes must lay their fields out alike.
static_assert(offsetof(CMsgTagObj, mTag) == offsetof(CMsgTagShow, mTag), "tag");
static_assert(offsetof(CMsgTagObj, mThing) == offsetof(CMsgTagShow, mThing), "thing");
static_assert(offsetof(CMsgTagObj, mTime) == offsetof(CMsgTagShow, mTime), "time");
static_assert(offsetof(CMsgTagObj, mValue) == offsetof(CMsgTagShow, mValue), "value");

// 0x10151f7c: a zero flags word, the tag, the thing id (the high half of its word: the Mac stores the halfword it reads back from
// the stack), then `extra` at +0x10 and `secondary` at +0x14. The halfword after the thing id is never written.
CMsgTagObj::CMsgTagObj(int32_t tag, uint32_t thingWord, uint32_t secondary, int32_t extra) {
    mFlags = 0;
    mTag = tag;
    mThing = static_cast<uint16_t>(thingWord >> 16);
    mTime = extra;
    mValue = secondary;
}
