// Fakes for the key tables unit (src/game/keyboard.cpp): the three actions an entry can hold. Each records its call (which action,
// its argument) and answers from the case's queue; the third also moves the table's entries to a second block, to show that the
// state is stored through the table after the call.
#include "keyboard.h"
#include "calllog.h"
#include <cstring>
using namespace calllog;

namespace {
constexpr int kMaxEntries = 8;
constexpr int kWords = 7;   // id, key, modifiers, state, the word at 0x08, the up action, the down action (actions as 0..3)
CKeyTable gTable;
CKeyTableEntry gBlock[2][kMaxEntries];

int32_t ActionA(int32_t argument) {
    ev("Action", {1, argument});
    return static_cast<int32_t>(ret("Action"));
}

int32_t ActionB(int32_t argument) {
    ev("Action", {2, argument});
    return static_cast<int32_t>(ret("Action"));
}

// Moves the table's entries to block 1 (a copy of block 0 as it is now).
int32_t ActionMove(int32_t argument) {
    ev("Action", {3, argument});
    std::memcpy(gBlock[1], gBlock[0], sizeof gBlock[0]);
    gTable.mEntries = gBlock[1];
    return static_cast<int32_t>(ret("Action"));
}

void* ActionOf(int32_t id) {
    switch (id) {
    case 1: return reinterpret_cast<void*>(ActionA);
    case 2: return reinterpret_cast<void*>(ActionB);
    case 3: return reinterpret_cast<void*>(ActionMove);
    }
    return nullptr;
}

int32_t IdOf(void* action) {
    if (action == reinterpret_cast<void*>(ActionA)) return 1;
    if (action == reinterpret_cast<void*>(ActionB)) return 2;
    if (action == reinterpret_cast<void*>(ActionMove)) return 3;
    return 0;
}

// Appends the words of one block of entries.
void Dump(int32_t* out, int& at, const CKeyTableEntry* entries, int count) {
    for (int i = 0; i < count; ++i) {
        const CKeyTableEntry& e = entries[i];
        uint32_t f08;
        std::memcpy(&f08, e.mF08, 4);
        out[at++] = e.mId;
        out[at++] = e.mKey;
        out[at++] = e.mModifiers;
        out[at++] = e.mState;
        out[at++] = static_cast<int32_t>(f08);
        out[at++] = IdOf(e.mUpAction);
        out[at++] = IdOf(e.mAction);
    }
}
}  // namespace

// Builds a table of `count` entries (block 0 holds `total` records, block 1 is filled with 0xa5 bytes), then runs the calls:
// calls are (0 down or 1 up, key, modifiers) triples. Per call the output holds the answer, which block the table points at, the
// count and enabled words, and then both blocks' records.
extern "C" void tpw_kt_run(int32_t enabled, int32_t count, int32_t total, const int32_t* entries, int32_t ncalls, const int32_t* calls,
                           int32_t* out) {
    std::memset(gBlock, 0xa5, sizeof gBlock);
    for (int i = 0; i < total; ++i) {
        CKeyTableEntry& e = gBlock[0][i];
        const int32_t* w = entries + kWords * i;
        e.mId = static_cast<int16_t>(w[0]);
        e.mKey = static_cast<int16_t>(w[1]);
        e.mModifiers = static_cast<int16_t>(w[2]);
        e.mState = static_cast<int16_t>(w[3]);
        const uint32_t f08 = static_cast<uint32_t>(w[4]);
        std::memcpy(e.mF08, &f08, 4);
        e.mUpAction = ActionOf(w[5]);
        e.mAction = ActionOf(w[6]);
    }
    gTable.mEntries = gBlock[0];
    gTable.mCount = static_cast<int16_t>(count);
    gTable.mEnabled = static_cast<int16_t>(enabled);
    int at = 0;
    for (int c = 0; c < ncalls; ++c) {
        const int16_t key = static_cast<int16_t>(calls[3 * c + 1]);
        const int16_t modifiers = static_cast<int16_t>(calls[3 * c + 2]);
        out[at++] = calls[3 * c] == 0 ? gTable.KeyDown(key, modifiers) : gTable.KeyUp(key, modifiers);
        out[at++] = gTable.mEntries == gBlock[1] ? 1 : 0;
        out[at++] = gTable.mCount;
        out[at++] = gTable.mEnabled;
        Dump(out, at, gBlock[0], total);
        Dump(out, at, gBlock[1], total);
    }
}
