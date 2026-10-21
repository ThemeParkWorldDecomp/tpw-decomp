// Test library for the three CGoldenTicketControl::TellTheWorld overloads (unit w183; libtpw_unit_GoldenTicketTell.dylib, tests/test_GoldenTicketTell.py):
//   src/game/park/GoldenTickets.cpp   TellTheWorld (local, global, secret)
//   src/game/park/GoldenTickets.cpp, src/game/ui/MessageCentre.cpp          CMsgAdvisor (real, as the announcements build it)
//
// Every callee outside the unit is a fake that records what the real one reads: TellIfFirst, CTRACE (the format and its integer arguments), the advisor
// request's two constructors, the message centre (the nothrow new of its 0x1d0 bytes, the first header's constructor for the array construction,
// PostMessage with the posted request's fields).
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

#include "AdvisorQueue.h"
#include "CMessageCentre.h"
#include "CSubscriptionHeader.h"
#include "Debug.h"
#include "MsgAdvisor.h"
#include "ObjectParts.h"
#include "calllog.h"
#include "platform/arena.h"

using namespace calllog;

namespace {

alignas(16) unsigned char gControlBlock[0x40];
alignas(16) unsigned char gCentreBlock[0x1d0];
void* gNewCentre;
void* gAllocated[8];
int32_t gAllocatedCount;

CGoldenTicketControl* Control() { return reinterpret_cast<CGoldenTicketControl*>(gControlBlock); }

}  // namespace

// ---- globals of other files, defined here (zero at start like the Mac's bss)
CMessageCentre* gMessageCentre;

// ---- the fakes
// TellIfFirst: only its receiver.
void CGoldenTicketControl::TellIfFirst() { ev("TellIfFirst", {lab(this)}); }

// CTRACE: the level, the integer arguments the format takes (two for the "Won a" line, one for the unrecognised line, none otherwise) and the text.
void CTRACE(uint32_t level, const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    int32_t first = 0;
    int32_t second = 0;
    if (std::strncmp(format, "Won a", 5) == 0) {
        first = va_arg(ap, int32_t);
        second = va_arg(ap, int32_t);
    } else if (std::strstr(format, "%d") != nullptr) {
        first = va_arg(ap, int32_t);
    }
    va_end(ap);
    ev("CTRACE", {level, first, second}, {format});
}

// The advisor request: the six-argument constructor sets the eight fields as src/game/advisor/Advisor.cpp does; the copy constructor copies the eight fields
// and leaves both padding regions alone (it logs the source's fields).
CAdvisorRequest::CAdvisorRequest(AdvisorMessage message, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thingId, uint32_t secondary, int32_t responseNo,
                                 bool overrideOnlyOnce, int32_t score) {
    ev("RequestCtor", {message, thingId.mIndex, secondary, responseNo, overrideOnlyOnce, score});
    mThingId = thingId.mIndex;
    mSecondary = secondary;
    mResponseNo = responseNo;
    mMessage = message;
    mScore = score;
    mValid = 1;
    mOverrideOnlyOnce = overrideOnlyOnce;
    mScoreOverridden = 0;
}
CAdvisorRequest::CAdvisorRequest(const CAdvisorRequest& other) {
    ev("RequestCopy", {other.mThingId, other.mSecondary, other.mResponseNo, other.mMessage, other.mScore, other.mValid, other.mOverrideOnlyOnce,
                       other.mScoreOverridden});
    mThingId = other.mThingId;
    mSecondary = other.mSecondary;
    mResponseNo = other.mResponseNo;
    mMessage = other.mMessage;
    mScore = other.mScore;
    mValid = other.mValid;
    mOverrideOnlyOnce = other.mOverrideOnlyOnce;
    mScoreOverridden = other.mScoreOverridden;
}

// The message centre: the posted message by its type, flags and the request's eight fields; the lazy allocation of the centre (the nothrow new and the
// first header's constructor, which stands for the array construction).
int32_t CMessageCentre::PostMessage(const CMsg& message) {
    const CAdvisorRequest& r = static_cast<const CMsgAdvisor&>(message).mRequest;
    ev("PostMessage", {lab(this), message.GetType(), message.mFlags, r.mThingId, r.mSecondary, r.mResponseNo, r.mMessage, r.mScore, r.mValid,
                       r.mOverrideOnlyOnce, r.mScoreOverridden});
    return 1;
}
void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    const bool fail = ret("newnull", 0) != 0;
    ev("new", {static_cast<int64_t>(size), fail});
    if (fail) return nullptr;
    gNewCentre = std::malloc(size);
    std::memset(gNewCentre, 0xa5, size);
    gAllocated[gAllocatedCount++] = gNewCentre;
    return gNewCentre;
}
CSubscriptionHeader::CSubscriptionHeader() {
    if (this == gNewCentre) ev("CMessageCentre", {newlab(this), 1, 1, 0x10, 0x1d});
}
CSubscriptionHeader::~CSubscriptionHeader() {}

extern "C" {

// The host address of a block by the module's name for it.
uint64_t tpw_gtt_addr(const char* name) {
    if (std::strcmp(name, "control") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gControlBlock));
    if (std::strcmp(name, "centre") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gCentreBlock));
    return 0;
}

// The per-case globals: the message centre pointer (0 or the block) and the release of the last case's allocation.
void tpw_gtt_prepare(uint64_t centre) {
    for (int32_t i = 0; i < gAllocatedCount; ++i) std::free(gAllocated[i]);
    gAllocatedCount = 0;
    gNewCentre = nullptr;
    gMessageCentre = reinterpret_cast<CMessageCentre*>(static_cast<uintptr_t>(centre));
}
// The centre pointer after the call, as a label.
int64_t tpw_gtt_centre() { return lab(gMessageCentre); }

// The three overloads, one wrapper: which = 0 local, 1 global, 2 secret.
void tpw_gtt_tell(int32_t which, int32_t type, int32_t won) {
    const auto w = static_cast<GoldenTicketTypeWon>(won);
    if (which == 0) Control()->TellTheWorld(static_cast<GoldenTicketTypeLocal>(type), w);
    else if (which == 1) Control()->TellTheWorld(static_cast<GoldenTicketTypeGlobal>(type), w);
    else Control()->TellTheWorld(static_cast<GoldenTicketTypeSecret>(type), w);
}

}  // extern "C"
