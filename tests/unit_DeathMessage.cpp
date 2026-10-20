// Test library for unit w123c: src/MsgDeath.cpp (CMsgDeath constructor, GetType and destructor), src/MessageCentre_post.cpp
// (CMessageCentre::PostMessage), src/World_death.cpp (CWorld::AddToPendingDeleteList and GetRand) and src/ParkTime_season.cpp
// (CParkTime::GetSeason). Every callee outside the unit is a recording fake: the assertion, the thing's GetThingId and ReceiveMessage, the
// subscription tree (find, erase, begin, the global increment, Construct and destroy), the pending list's AddHead, the world accessor and the
// two timestamp imports. The death message's GetType is the real one; a non-death post uses a test subclass whose answer the case chooses.
//
// Labels: 1 and 2 the worlds, 3 the clock, 5 the pending list, 6 the message centre, 2 (post) the message, 3 (dtor) the object, 10 and up the
// fake things (by position in the case), 200 and up the scripted tree nodes (200 + slot * 9 + index, index 1 to 8).
#define TPW_CALLLOG_LABELS 1024
#include "calllog.cpp"

#include <cstring>
#include <new>

#include "CMessageCentre.h"
#include "CMsgDeath.h"
#include "CMsgEvent.h"
#include "CSubscriptionHeader.h"
#include "Debug.h"
#include "MslSubscriptionTree.h"
#include "ParkTime.h"
#include "TbTimeDiff.h"
#include "World.h"
#include "WorldSavvy.h"
#include "WorldThings.h"
#include "analyser.h"

using namespace calllog;

using ThingTag = tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>;

CThingArrayEntry gThingArray[kThingArraySize];
TbTList<ThingTag, ThingTag> gPendingDeleteList;   // label 5

namespace {

constexpr int32_t kSlots = 29;
constexpr int32_t kMaxNodes = 8;
constexpr int32_t kTail = 0x1da6f0;   // the world byte a tail block starts at
constexpr int32_t kClockAt = 0x40;
constexpr int32_t kClockSize = kClockAt + 0x24 + 0x40;
constexpr int32_t kThings = 24;

// The scripted subscription trees: node 1 to 8 of each slot, the delivery path (first, then next[] until 0) and the nodes' ids.
struct Chain {
    SubscriptionTree::node nodes[kMaxNodes + 1];
    int32_t next[kMaxNodes + 1];
    int32_t first;
    int32_t count;
};
Chain gChains[kSlots];
alignas(16) unsigned char gCentreBytes[sizeof(CMessageCentre)];
CThing gThings[kThings];
alignas(16) unsigned char gMessageBytes[0x40];
alignas(16) unsigned char gTail[2][0x80];
alignas(16) unsigned char gClockBlock[kClockSize];
alignas(16) unsigned char gObject[0x40];

// The assertion's callback mutations: at call number `call` (counted from 0) a table row takes another fake thing (kind 0) or a node takes
// another id (kind 1). They show when the translation reads each value.
struct Mutation {
    int32_t call, kind, a, b, c;
};
Mutation gMutations[8];
int32_t gMutationCount;
int32_t gAssertCalls;

CMessageCentre* Centre() { return reinterpret_cast<CMessageCentre*>(gCentreBytes); }

// The slot of a tree of the centre.
int32_t SlotOf(const SubscriptionTree* tree) {
    const auto* base = reinterpret_cast<const unsigned char*>(&Centre()->mSubscriptions[0].mTree);
    return static_cast<int32_t>((reinterpret_cast<const unsigned char*>(tree) - base) / sizeof(CSubscriptionHeader));
}

int32_t NodeLabel(int32_t slot, int32_t index) { return 200 + slot * 9 + index; }

CWorld* WorldPtr(int32_t k) { return reinterpret_cast<CWorld*>(reinterpret_cast<uintptr_t>(gTail[k]) - kTail); }

class TestMessage : public CMsgDeath {
public:
    int32_t mType = 0;
    int32_t GetType() const override {
        ev("GetType", {lab(this)});
        return mType;
    }
};

void ApplyMutations() {
    const int32_t call = gAssertCalls++;
    for (int32_t i = 0; i < gMutationCount; ++i) {
        const Mutation& m = gMutations[i];
        if (m.call != call) continue;
        if (m.kind == 0) {
            gThingArray[m.a].mThing = m.b >= 0 ? static_cast<void*>(&gThings[m.b]) : nullptr;
        } else {
            gChains[m.a].nodes[m.b].mValue.mThingId = static_cast<uint16_t>(m.c);
        }
    }
}

}  // namespace

// ---- fakes

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) {
    ev("GAME_ASSERTMSG", {condition & 0xff}, {format});
    ApplyMutations();
}

// The thing's id: the queue "id" (one value per call), else the thing's own id.
ThingTag CThing::GetThingId() const {
    const uint16_t answer = static_cast<uint16_t>(ret("id", mThingId));
    ev("GetThingId", {lab(this), answer});
    return {answer};
}

void CThing::ReceiveMessage(const CMsg& message) { ev("ReceiveMessage", {lab(this), lab(&message)}); }

void SubscriptionTree::Construct() {}
void SubscriptionTree::destroy(node*) {}

// The first node of the delivery path (end() when it is empty). The Mac keeps this word in the tree; the fake keeps it in the chain.
SubscriptionTree::node_base* SubscriptionTree::begin() {
    Chain& chain = gChains[SlotOf(this)];
    return chain.first != 0 ? &chain.nodes[chain.first] : end();
}

// Answers the node the case scripts for this slot ("find:<slot>", 0 for end()); records the slot and the key.
SubscriptionTree::node_base* SubscriptionTree::find(const CSubscription& key) {
    const int32_t slot = SlotOf(this);
    ev("find", {slot, key.mThingId});
    const int32_t index = static_cast<int32_t>(ret("find", slot, 0));
    return index != 0 ? &gChains[slot].nodes[index] : end();
}

// Records the slot and the node and takes the node off the delivery path (a node that is not on it changes nothing).
void SubscriptionTree::erase(node_base* position) {
    const int32_t slot = SlotOf(this);
    Chain& chain = gChains[slot];
    ev("erase", {slot, lab(position)});
    int32_t index = 0;
    for (int32_t i = 1; i <= kMaxNodes; ++i)
        if (position == &chain.nodes[i]) index = i;
    if (index == 0) return;
    if (chain.first == index) {
        chain.first = chain.next[index];
    } else {
        for (int32_t i = 1; i <= kMaxNodes; ++i)
            if (chain.next[i] == index) chain.next[i] = chain.next[index];
    }
}

// The successor on the delivery path (end() after the last node).
void MslRedBlackTree::increment(node_base*& iterator) {
    ev("increment", {lab(iterator)});
    for (int32_t slot = 0; slot < kSlots; ++slot) {
        Chain& chain = gChains[slot];
        for (int32_t i = 1; i <= kMaxNodes; ++i) {
            if (iterator == &chain.nodes[i]) {
                iterator = chain.next[i] != 0 ? &chain.nodes[chain.next[i]] : Centre()->mSubscriptions[slot].mTree.end();
                return;
            }
        }
    }
}

// The pending list: records the list and the full id; "padd" changes the count (the list's own state is the callee's).
template<> void TbTList<ThingTag, ThingTag>::AddHead(ThingTag value) {
    ev("AddHead", {lab(this), value.mIndex});
    mCount = static_cast<int32_t>(static_cast<uint32_t>(mCount) + static_cast<uint32_t>(ret("padd", 0)));
}

// The world accessor: records the clock and answers world "wsel"; with "wmut" set it first changes the answer's turn word, the clock's rate and the
// clock's start (the values "wturn", "wrate", "wstart"), as a world that moved on would.
CWorld& CWorldSavvy::GetWorld() const {
    ev("GetWorld", {lab(this)});
    const int32_t selected = static_cast<int32_t>(ret("wsel", 0));
    if (ret("wmut", 0) != 0) {
        const uint32_t turn = static_cast<uint32_t>(ret("wturn", 0));
        std::memcpy(gTail[selected] + (0x1da70c - kTail), &turn, 4);
        const uint32_t rate = static_cast<uint32_t>(ret("wrate", 0));
        std::memcpy(gClockBlock + kClockAt + 0x1c, &rate, 4);
        const int64_t start = ret("wstart", 0);
        std::memcpy(gClockBlock + kClockAt, &start, 8);
    }
    return *WorldPtr(selected);
}

// The sum: the receiver's tick image and the span's, and the answer "plus".
TbTimeStamp TbTimeStamp::operator+(const TbTimeDiff& diff) const {
    ev("TimeStamp+", {mTicks, diff.mTicks});
    return TbTimeStamp{ret("plus", 0)};
}

// The stamp's tick image and the eight output pointers as labels (NULL 0, any other -1). Each non-NULL output takes the next value of "gtout"
// (the month); the TbError is "gterr".
TbError TbTimeStamp::GetTime(int32_t* a, int32_t* b, int32_t* c, int32_t* d, int32_t* e, int32_t* f, int32_t* g, int32_t* h) const {
    ev("GetTime", {mTicks, lab(a), lab(b), lab(c), lab(d), lab(e), lab(f), lab(g), lab(h)});
    for (int32_t* out : {a, b, c, d, e, f, g, h}) {
        if (out != nullptr) *out = static_cast<int32_t>(ret("gtout", 0));
    }
    return TbError(static_cast<int32_t>(ret("gterr", 0)));
}

extern "C" {

// ---- the death message's own functions

// The constructor over 0xa5 bytes: out = flags, id, the virtual GetType through the base, 1 when every byte outside the vptr, flags and id is
// untouched, 1 when two objects share one non-null vptr.
void tpw_dm_ctor(uint32_t id, uint32_t* out) {
    std::memset(gObject, 0xa5, sizeof gObject);
    auto* message = new (gObject) CMsgDeath(ThingTag{static_cast<uint16_t>(id)});
    const CMsg& base = *message;
    out[0] = message->mFlags;
    out[1] = message->mThingId;
    out[2] = static_cast<uint32_t>(base.GetType());
    bool intact = true;
    const auto begin = reinterpret_cast<unsigned char*>(message);
    for (size_t i = 0; i < sizeof(CMsgDeath); ++i) {
        const unsigned char* byte = begin + i;
        const bool member = (byte >= reinterpret_cast<const unsigned char*>(&message->mFlags) &&
                             byte < reinterpret_cast<const unsigned char*>(&message->mFlags) + sizeof message->mFlags) ||
                            (byte >= reinterpret_cast<const unsigned char*>(&message->mThingId) &&
                             byte < reinterpret_cast<const unsigned char*>(&message->mThingId) + sizeof message->mThingId) ||
                            i < sizeof(void*);
        if (!member && *byte != 0xa5) intact = false;
    }
    for (size_t i = sizeof(CMsgDeath); i < sizeof gObject; ++i)
        if (gObject[i] != 0xa5) intact = false;
    out[3] = intact ? 1 : 0;
    CMsgDeath other(ThingTag{1});
    void* vptr1;
    void* vptr2;
    std::memcpy(&vptr1, static_cast<const void*>(message), sizeof vptr1);
    std::memcpy(&vptr2, static_cast<const void*>(&other), sizeof vptr2);
    out[4] = (vptr1 != nullptr && vptr1 == vptr2) ? 1 : 0;
}

// GetType on an object with the given flags and id, called twice through the base: out = the two answers, flags, id.
void tpw_dm_gettype(uint32_t flags, uint32_t id, uint32_t* out) {
    auto* message = new (gObject) CMsgDeath(ThingTag{static_cast<uint16_t>(id)});
    message->mFlags = flags;
    const CMsg& base = *message;
    out[0] = static_cast<uint32_t>(base.GetType());
    out[1] = static_cast<uint32_t>(base.GetType());
    out[2] = message->mFlags;
    out[3] = message->mThingId;
}

// The destructor with the Mac's delete flag: the C++ equivalent is a delete-expression for a positive (signed 16 bit) flag, else an explicit
// destructor call; a NULL receiver is the adapter's own test (the Mac tests it inside). out = the flags and id left in the object (an explicit
// destruction only).
void tpw_dm_dtor(int32_t flag, int32_t present, uint32_t* out) {
    out[0] = out[1] = 0;
    if (present == 0) return;
    if (static_cast<int16_t>(flag) > 0) {
        auto* message = new CMsgDeath(ThingTag{0x1234});
        --tpw_cl_heap[0];   // the test's own allocation is not the function's
        tpw_cl_label(3, message);
        delete message;
        return;
    }
    auto* message = new (gObject) CMsgDeath(ThingTag{0x1234});
    message->mFlags = 0x11223344;
    message->mThingId = 0x5566;
    message->~CMsgDeath();
    uint32_t flags;
    uint16_t id;
    std::memcpy(&flags, gObject + (reinterpret_cast<unsigned char*>(&message->mFlags) - reinterpret_cast<unsigned char*>(message)), 4);
    std::memcpy(&id, gObject + (reinterpret_cast<unsigned char*>(&message->mThingId) - reinterpret_cast<unsigned char*>(message)), 2);
    out[0] = flags;
    out[1] = id;
}

// ---- the post

// Forgets the table, the things, the scripted trees and the mutations; the centre is poisoned and every header an empty tree.
void tpw_dm_post_begin() {
    std::memset(gThingArray, 0, sizeof gThingArray);
    std::memset(static_cast<void*>(gThings), 0, sizeof gThings);
    std::memset(static_cast<void*>(gChains), 0, sizeof gChains);
    std::memset(gCentreBytes, 0xa5, sizeof gCentreBytes);
    gMutationCount = 0;
    gAssertCalls = 0;
    tpw_cl_label(6, Centre());
    for (int32_t slot = 0; slot < kSlots; ++slot) {
        for (int32_t i = 1; i <= kMaxNodes; ++i) tpw_cl_label(NodeLabel(slot, i), &gChains[slot].nodes[i]);
    }
}

// A fake thing j with its own id, standing in table row `row` (-1: not in the table), label 10 + j.
void tpw_dm_set_thing(int32_t j, int32_t row, int32_t own) {
    gThings[j].mThingId = static_cast<uint16_t>(own);
    gThings[j].mKind = 0;
    if (row >= 0) gThingArray[row].mThing = &gThings[j];
    tpw_cl_label(10 + j, &gThings[j]);
}

// A scripted tree: n nodes with the ids ids[0..n-1] (nodes 1 to n), the delivery path as the next indexes nexts[0..n-1] (0 ends it) from `first`.
void tpw_dm_set_chain(int32_t slot, int32_t n, const int32_t* ids, const int32_t* nexts, int32_t first) {
    Chain& chain = gChains[slot];
    chain.first = first;
    chain.count = n;
    for (int32_t i = 1; i <= n; ++i) {
        chain.nodes[i].mValue.mThingId = static_cast<uint16_t>(ids[i - 1]);
        chain.next[i] = nexts[i - 1];
    }
}

void tpw_dm_mut(int32_t call, int32_t kind, int32_t a, int32_t b, int32_t c) {
    if (gMutationCount < 8) gMutations[gMutationCount++] = {call, kind, a, b, c};
}

// Posts a message of the given type (a death message with the dying thing's row when isDeath). The message is labelled 2 and its flags word
// holds a marker; out = the flags and id afterwards.
int32_t tpw_dm_post(int32_t type, int32_t dyingRow, int32_t isDeath, uint32_t* out) {
    int32_t answer;
    if (isDeath != 0) {
        auto* message = new (gMessageBytes) CMsgDeath(ThingTag{static_cast<uint16_t>(dyingRow)});
        message->mFlags = 0xa5a5a5a5;
        tpw_cl_label(2, message);
        answer = Centre()->PostMessage(*message);
        out[0] = message->mFlags;
        out[1] = message->mThingId;
    } else {
        auto* message = new (gMessageBytes) TestMessage;
        message->mFlags = 0xa5a5a5a5;
        message->mThingId = 0xa5a5;
        message->mType = type;
        tpw_cl_label(2, message);
        answer = Centre()->PostMessage(*message);
        out[0] = message->mFlags;
        out[1] = message->mThingId;
    }
    return answer;
}

// The scripted tree of a slot afterwards: first, then the next index of nodes 1 to 8.
void tpw_dm_chain(int32_t slot, int32_t* out) {
    out[0] = gChains[slot].first;
    for (int32_t i = 1; i <= kMaxNodes; ++i) out[i] = gChains[slot].next[i];
}

int32_t tpw_dm_row_label(int32_t row) { return lab(gThingArray[row].mThing); }

// ---- the pending list

// Sets the list's count and labels it 5; the receiver is nothing (0), a world block (1) or an arbitrary address (2).
void tpw_dm_pending_set(int32_t count) {
    gPendingDeleteList.mCount = count;
    gPendingDeleteList.mHead = nullptr;
    gPendingDeleteList.mTail = nullptr;
    tpw_cl_label(5, &gPendingDeleteList);
}

void tpw_dm_pending(int32_t receiver, uint32_t id) {
    CWorld* world = receiver == 0 ? nullptr : receiver == 1 ? WorldPtr(0) : reinterpret_cast<CWorld*>(uintptr_t(0x12345670));
    world->AddToPendingDeleteList(ThingTag{static_cast<uint16_t>(id)});
}

int32_t tpw_dm_pending_count() { return gPendingDeleteList.mCount; }
int32_t tpw_dm_pending_links() { return gPendingDeleteList.mHead == nullptr && gPendingDeleteList.mTail == nullptr ? 1 : 0; }

// ---- the clock and the worlds

unsigned char* tpw_dm_tail(int32_t k) { return gTail[k]; }
unsigned char* tpw_dm_clock() { return gClockBlock; }

void tpw_dm_label_clock() {
    tpw_cl_label(1, WorldPtr(0));
    tpw_cl_label(2, WorldPtr(1));
    tpw_cl_label(3, gClockBlock + kClockAt);
}

uint32_t tpw_dm_rand(int32_t k) { return WorldPtr(k)->GetRand(); }

int32_t tpw_dm_season() { return reinterpret_cast<const CParkTime*>(gClockBlock + kClockAt)->GetSeason(); }

}  // extern "C"
