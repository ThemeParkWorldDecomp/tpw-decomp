// Differential library for unit w122b. External callees record all arguments and selected state at the call.
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <new>

#include "calllog.cpp"
#include "CMessageCentre.h"
#include "CSubscriptionHeader.h"
#include "Debug.h"
#include "MainBalance.h"
#include "ThingReceivers.h"
#include "World.h"

using namespace calllog;

namespace {
alignas(16) unsigned char gBlock[0x21c48 + 0x40];
alignas(16) unsigned char gWorldBlock[0x300];
alignas(16) unsigned char gOtherWorld[0x300];
alignas(16) unsigned char gCentreBlock[0x1d0];
void* gNewCentre;
void* gAllocated[8];
int32_t gAllocatedCount;
int32_t gPowIndex;

// Hash bytes that have not yet been interpreted by the constructor.
uint32_t Bytes(const void* p, int32_t n) {
    uint32_t h = 2166136261u;
    const auto* bytes = static_cast<const unsigned char*>(p);
    for (int32_t i = 0; i < n; ++i) h = (h ^ bytes[i]) * 16777619u;
    return h;
}
uint32_t Word(const void* p, int32_t offset) {
    uint32_t v;
    std::memcpy(&v, static_cast<const unsigned char*>(p) + offset, 4);
    return v;
}
uint64_t Tick(const void* p, int32_t offset) {
    uint64_t v;
    std::memcpy(&v, static_cast<const unsigned char*>(p) + offset, 8);
    return v;
}
// Hash the counters and stamps at each clock call; the tail stays unwritten until the sixth answer is copied.
uint32_t AnalyserState(bool tail) {
    uint32_t h = 2166136261u;
    for (int32_t o = 0xc; o < 0x20; o += 4) h = (h ^ Word(gBlock, o)) * 16777619u;
    for (int32_t o = 0x21bfc; o < 0x21c0c; o += 4) h = (h ^ Word(gBlock, o)) * 16777619u;
    for (int32_t o = 0x21c0c; o < 0x21c3c; o += 8) {
        const uint64_t t = Tick(gBlock, o);
        h = (h ^ static_cast<uint32_t>(t >> 32)) * 16777619u;
        h = (h ^ static_cast<uint32_t>(t)) * 16777619u;
    }
    if (tail) {
        for (int32_t o = 0x21c3c; o < 0x21c48; o += 4) h = (h ^ Word(gBlock, o)) * 16777619u;
    }
    return h;
}
int64_t Bits(double v) {
    int64_t bits;
    std::memcpy(&bits, &v, 8);
    return bits;
}
// At pow, all preceding loans and seven fields of this loan have been stored, as have the bank's outer words.
uint32_t BankState() {
    uint32_t h = 2166136261u;
    for (int32_t o : {0xc, 0x10, 0x114, 0x118, 0x11c, 0x120, 0x124}) h = (h ^ Word(gBlock, o)) * 16777619u;
    for (int32_t o = 0x14; o < 0x14 + 0x20 * gPowIndex; o += 4) h = (h ^ Word(gBlock, o)) * 16777619u;
    for (int32_t o : {0, 4, 8, 0xc, 0x14, 0x18, 0x1c}) h = (h ^ Word(gBlock, 0x14 + 0x20 * gPowIndex + o)) * 16777619u;
    ++gPowIndex;
    return h;
}
}

CWorld* gWorld;
CMessageCentre* gMessageCentre;

// Base construction observes the still-filled object, then stamps exactly its twelve bytes.
CThing::CThing(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell, ThingModel model) {
    ev("CThing", {lab(this), cell.mIndex, model, Bytes(this, 0x40)});
    std::memset(this, 0x5a, 0xc);
}
bool CMessageCentre::Subscribe(const CThing& thing, MsgType type) {
    ev("Subscribe", {lab(this), lab(&thing), type});
    if (ret("centreclear", 0)) gMessageCentre = nullptr;
    return true;
}
int32_t CParkAnalyser::Init() {
    ev("CParkAnalyser::Init", {lab(this), AnalyserState(true)});
    return 1;
}
void CChallengeHQ::Init() {
    ev("CChallengeHQ::Init", {lab(this), static_cast<uint32_t>(mCurrentChallenge),
        static_cast<int64_t>(Tick(this, 0x3d0)), mSystemSwitchedOn, mChallengeOn, mWaitingForReply,
        static_cast<uint32_t>(mValue), mActualThing});
}
TbTimeStamp CParkTime::GetFunnyTime(int32_t* a, int32_t* b, int32_t* c, int32_t* d,
                                 int32_t* e, int32_t* f, int32_t* g, int32_t* h) const {
    const auto base = reinterpret_cast<uintptr_t>(gWorld);
    ev("GetFunnyTime", {lab(gWorld), static_cast<int64_t>(reinterpret_cast<uintptr_t>(this) - base),
        a != nullptr, b != nullptr, c != nullptr, d != nullptr, e != nullptr, f != nullptr, g != nullptr, h != nullptr,
        AnalyserState(false)});
    const int64_t ticks = ret("funny", 0);
    if (ret("worldswap", 0)) {
        gWorld = reinterpret_cast<CWorld*>(gWorld == reinterpret_cast<CWorld*>(gWorldBlock) ? gOtherWorld : gWorldBlock);
    }
    return TbTimeStamp{ticks};
}
void CTRACE(uint32_t level, const char* format, ...) {
    va_list args;
    va_start(args, format);
    const uint32_t fee = va_arg(args, uint32_t);
    va_end(args);
    ev("CTRACE", {level, fee, Bytes(gBlock + 0x118, 4)}, {format});
}
// Only the balance object's storage and virtual members are real in this library.
void CBalanceTable::Init() {}
void CBalanceTable::UnInit() {}

// Each allocation is counted and filled; null results are controlled by the case's queue.
void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    const bool fail = ret("newnull", 0) != 0;
    ev("new", {static_cast<int64_t>(size), fail});
    if (fail) return nullptr;
    gNewCentre = std::malloc(size);
    std::memset(gNewCentre, 0xa5, size);
    gAllocated[gAllocatedCount++] = gNewCentre;
    ++tpw_cl_heap[0];
    return gNewCentre;
}
CSubscriptionHeader::CSubscriptionHeader() {
    if (this == gNewCentre) ev("CMessageCentre", {newlab(this), 1, 1, 0x10, 0x1d});
}
CSubscriptionHeader::~CSubscriptionHeader() {}

// Match the Mac pow call's argument bits; optional answers exercise the real unsigned conversion helper.
extern "C" double pow(double x, double y) {
    ev("pow", {Bits(x), Bits(y), BankState()});
    if (ret("powoverride", 0)) {
        const int64_t bits = ret("powbits", 0);
        double value;
        std::memcpy(&value, &bits, 8);
        return value;
    }
    using PowFn = double (*)(double, double);
    static const PowFn real = reinterpret_cast<PowFn>(dlsym(RTLD_NEXT, "pow"));
    return real(x, y);
}

extern "C" {
void* tpw_tc2_block() { return gBlock; }
void* tpw_tc2_world(int32_t second) { return second ? gOtherWorld : gWorldBlock; }
void* tpw_tc2_centre() { return gCentreBlock; }
void tpw_tc2_load(const uint8_t* bytes, int32_t n) { std::memcpy(gBlock, bytes, n); }
void tpw_tc2_dump(uint8_t* out, int32_t n) { std::memcpy(out, gBlock, n); }
void tpw_tc2_prepare(int32_t world, int32_t centre, const uint32_t* balance) {
    for (int32_t i = 0; i < gAllocatedCount; ++i) std::free(gAllocated[i]);
    gAllocatedCount = 0;
    gNewCentre = nullptr;
    gPowIndex = 0;
    gWorld = reinterpret_cast<CWorld*>(world ? gOtherWorld : gWorldBlock);
    gMessageCentre = centre ? reinterpret_cast<CMessageCentre*>(gCentreBlock) : nullptr;
    for (int32_t i = 0; i < 42; ++i) gMainBalance.mValues[100 + i].mInteger = balance[i];
}
void tpw_tc2_globals(int64_t* out) {
    out[0] = lab(gWorld);
    out[1] = lab(gMessageCentre);
    out[2] = tpw_cl_heap[0];
}
// The order matches ADDR in test_ThingCtors2.py. Leaves are constructed directly over the same fill and guards.
void tpw_tc2_call(int32_t op, uint32_t fee) {
    switch (op) {
    case 0: new (gBlock) CParkAnalyser; break;
    case 1: new (gBlock) CBankAccount; break;
    case 2: new (gBlock) CChallengeHQ; break;
    case 3: new (gBlock) CHistory<float, 144>; break;
    case 4: new (gBlock) CHistory<int, 144>; break;
    case 5: new (gBlock) CHistory<int, 30>; break;
    case 6: new (gBlock) CHistory<int, 5>; break;
    case 7: new (gBlock) CHistory<int, 50>; break;
    case 8: new (gBlock) CHistory<uint32_t, 30>; break;
    case 9: new (gBlock) CParkAnalyserVal<int>; break;
    case 10: new (gBlock) CParkAnalyserCell; break;
    case 11: new (gBlock) CParkAnalyserVal<float>; break;
    case 12: new (gBlock) CHistory<unsigned char, 50>; break;
    case 13: new (gBlock) CHistory<tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>, 3>; break;
    case 14: new (gBlock) CChallenge; break;
    case 15: reinterpret_cast<CBankAccount*>(gBlock)->SetAdmissionFee(fee); break;
    }
}
}
