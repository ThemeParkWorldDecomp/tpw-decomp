// Test library for the message centre's post and the subscription tree (unit w107): src/MessageCentre_post.cpp, src/Thing_receive.cpp,
// src/msl_tree_subscription.cpp and src/msl_red_black_tree.cpp. Real on both sides: the tree code, the headers' constructor and
// destructor, CThing::ReceiveMessage's switch. Recording fakes: GAME_ASSERTMSG, the message's GetType (a test subclass whose answer
// the case chooses) and the eighteen ReceiveMessage targets. Operator delete records through calllog.cpp for every labelled block.
//
// Labels: 1 the centre, 2 the message, 3 a header or object under test, 4 a node's pointer given to the allocator, 10 to 59 the fake
// things (by position in the case), 200 and up the tree nodes (slot * 40 + index).
#define TPW_CALLLOG_LABELS 2048
#include "calllog.cpp"

#include <cstring>
#include <new>

#include "AdvisorQueue.h"
#include "CMessageCentre.h"
#include "CMsgDeath.h"
#include "CMsgEvent.h"
#include "CSubscriptionHeader.h"
#include "Debug.h"
#include "MslSubscriptionTree.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"

using namespace calllog;

CThingArrayEntry gThingArray[kThingArraySize];

namespace {

constexpr int32_t kEnd = 255;          // the dump's index of a tree's end node
constexpr int32_t kScratchSlot = 29;   // a standalone header, built over poison by the constructor tests
constexpr int32_t kHeapSlot = 100;     // a header made with new (for the destructor's delete flag)
constexpr int32_t kMaxNodes = 40;

alignas(16) unsigned char gCentreBytes[sizeof(CMessageCentre)];
alignas(16) unsigned char gScratchHeader[sizeof(CSubscriptionHeader)];
CSubscriptionHeader* gHeapHeader;
SubscriptionTree::node_base* gNodes[kHeapSlot + 1][kMaxNodes + 1];   // the nodes of each slot by index (1 up); 0 is unused
int32_t gNodeCount[kHeapSlot + 1];
CThing gThings[64];

class TestMessage : public CMsgDeath {
public:
    int32_t mType = 0;
    int32_t GetType() const override {
        ev("GetType", {lab(this)});
        return mType;
    }
};
TestMessage gMessage;   // label 2

CMessageCentre* Centre() { return reinterpret_cast<CMessageCentre*>(gCentreBytes); }

CSubscriptionHeader* Header(int32_t slot) {
    if (slot < 0) return nullptr;
    if (slot == kScratchSlot) return reinterpret_cast<CSubscriptionHeader*>(gScratchHeader);
    if (slot == kHeapSlot) return gHeapHeader;
    return &Centre()->mSubscriptions[slot];
}

// The dump's index of a node pointer: 0 for null, kEnd for the tree's end node, the node's index, else -1.
int32_t IndexOf(int32_t slot, SubscriptionTree::node_base* node) {
    if (node == nullptr) return 0;
    if (node == Header(slot)->mTree.end()) return kEnd;
    for (int32_t i = 1; i <= kMaxNodes; ++i)
        if (gNodes[slot][i] == node) return i;
    return -1;
}

SubscriptionTree::node_base* NodeAt(int32_t slot, int32_t index) {
    if (index == kEnd) return Header(slot)->mTree.end();
    return index == 0 ? nullptr : gNodes[slot][index];
}

// mode 0 and 1: count, root, begin (mode 1 only; else 0), the number m of nodes reachable from the root, then m records (index, left,
// right, parent, red, id) in preorder. Mode 2: count, root, 0, 0 (no node is touched: for a tree whose nodes were freed). Mode 3: count,
// root, 0, n, then the records of all n built nodes in index order (for the functions that unlink without freeing).
int32_t Dump(int32_t slot, int32_t* out, int32_t mode) {
    SubscriptionTree& tree = Header(slot)->mTree;
    int32_t n = 0;
    out[n++] = static_cast<int32_t>(tree.mCount);
    out[n++] = IndexOf(slot, tree.mRoot);
    if (mode == 2) {
        out[n++] = 0;
        out[n++] = 0;
        return n;
    }
    out[n++] = mode == 1 ? IndexOf(slot, tree.begin()) : 0;
    auto record = [&](int32_t index, SubscriptionTree::node_base* node) {
        out[n++] = index;
        out[n++] = IndexOf(slot, node->mLeft);
        out[n++] = IndexOf(slot, node->mRight);
        out[n++] = IndexOf(slot, reinterpret_cast<SubscriptionTree::node_base*>(node->mParent & ~uintptr_t(1)));
        out[n++] = static_cast<int32_t>(node->mParent & 1);
        out[n++] = static_cast<SubscriptionTree::node*>(node)->mValue.mThingId;
    };
    if (mode == 3) {
        out[n++] = gNodeCount[slot];
        for (int32_t i = 1; i <= gNodeCount[slot]; ++i) record(i, gNodes[slot][i]);
        return n;
    }
    int32_t countAt = n++;
    int32_t m = 0;
    SubscriptionTree::node_base* stack[kMaxNodes + 2];
    int32_t depth = 0;
    if (tree.mRoot != nullptr) stack[depth++] = tree.mRoot;
    while (depth > 0 && m < kMaxNodes) {
        SubscriptionTree::node_base* node = stack[--depth];
        record(IndexOf(slot, node), node);
        ++m;
        if (node->mRight != nullptr && depth < kMaxNodes) stack[depth++] = node->mRight;
        if (node->mLeft != nullptr && depth < kMaxNodes) stack[depth++] = node->mLeft;
    }
    out[countAt] = m;
    return n;
}

}  // namespace

// ---- fakes

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {condition & 0xff}, {format}); }

#define RECEIVER(cls) \
    void cls::ReceiveMessage(const CMsg& message) { ev(#cls "::ReceiveMessage", {lab(this), lab(&message)}); }
RECEIVER(CPerson)
RECEIVER(CObject)
RECEIVER(CMechanic)
RECEIVER(CHandyman)
RECEIVER(CEntertainer)
RECEIVER(CGuard)
RECEIVER(CResearcher)
RECEIVER(CStaffHQ)
RECEIVER(CMechanicHQ)
RECEIVER(CAdvisor)
RECEIVER(CTagSystem)
RECEIVER(CParkAnalyser)
RECEIVER(CResearchLab)
RECEIVER(CWeather)
RECEIVER(CBankAccount)
RECEIVER(CUIMsgReceiver)
RECEIVER(COnlinePerson)
RECEIVER(CChallengeHQ)

extern "C" {

// ---- trees

// Forgets the trees, the thing table and the fake things; every header of the centre is an empty tree again.
void tpw_mc_begin() {
    std::memset(gNodes, 0, sizeof gNodes);
    std::memset(gNodeCount, 0, sizeof gNodeCount);
    std::memset(gThingArray, 0, sizeof gThingArray);
    std::memset(gThings, 0, sizeof gThings);
    std::memset(gCentreBytes, 0xa5, sizeof gCentreBytes);
    for (CSubscriptionHeader& header : Centre()->mSubscriptions) header.mTree.Construct();
    std::memset(gScratchHeader, 0xa5, sizeof gScratchHeader);
    gHeapHeader = nullptr;
    gMessage.mThingId = 0;
    gMessage.mFlags = 0xa5a5a5a5;
    gMessage.mType = 0;
    tpw_cl_label(1, Centre());
    tpw_cl_label(2, &gMessage);
}

// Builds slot's tree from n nodes of five ints each (id, red, left, right, parent; indexes from 1, 0 none, parent 0 for the root).
// Each node is labelled label_base + index and its other bytes are poisoned. The slot's header is a fresh one for kHeapSlot.
void tpw_mc_build(int32_t slot, int32_t n, int32_t root, const int32_t* nodes, int32_t label_base, int32_t count) {
    if (slot == kHeapSlot) {
        gHeapHeader = new CSubscriptionHeader;
        tpw_cl_label(3, gHeapHeader);
    }
    SubscriptionTree& tree = Header(slot)->mTree;
    tree.Construct();
    gNodeCount[slot] = n;
    for (int32_t i = 1; i <= n; ++i) {
        auto* node = new SubscriptionTree::node;
        std::memset(node, 0xa5, sizeof *node);
        gNodes[slot][i] = node;
        tpw_cl_label(label_base + i, node);
    }
    for (int32_t i = 1; i <= n; ++i) {
        const int32_t* r = nodes + 5 * (i - 1);
        auto* node = static_cast<SubscriptionTree::node*>(gNodes[slot][i]);
        node->mValue.mThingId = static_cast<uint16_t>(r[0]);
        node->mLeft = NodeAt(slot, r[2]);
        node->mRight = NodeAt(slot, r[3]);
        node->mParent = reinterpret_cast<uintptr_t>(r[4] == 0 ? tree.end() : gNodes[slot][r[4]]) | (r[1] != 0 ? 1 : 0);
    }
    tree.mCount = static_cast<uint32_t>(count);
    tree.mRoot = root == 0 ? nullptr : gNodes[slot][root];
}

int32_t tpw_mc_dump(int32_t slot, int32_t* out, int32_t mode) { return Dump(slot, out, mode); }

// The tree operations on slot: 0 find (a = the id), 1 increment from node a, 2 erase node a, 3 clear, 4 destroy the subtree at node a,
// 5 balance_erase(node a, root b), 6 rotate_left(node a, a root word holding node b), 7 rotate_right likewise. The answer is an index
// (find, increment, the rotations' root word) or 0.
int32_t tpw_mc_tree_op(int32_t op, int32_t slot, int32_t a, int32_t b) {
    SubscriptionTree& tree = Header(slot)->mTree;
    switch (op) {
    case 0: {
        CSubscription key;
        key.mThingId = static_cast<uint16_t>(a);
        return IndexOf(slot, tree.find(key));
    }
    case 1: {
        SubscriptionTree::node_base* it = NodeAt(slot, a);
        MslRedBlackTree::increment(it);
        return IndexOf(slot, it);
    }
    case 2:
        tree.erase(NodeAt(slot, a));
        return 0;
    case 3:
        tree.clear();
        return 0;
    case 4:
        tree.destroy(static_cast<SubscriptionTree::node*>(NodeAt(slot, a)));
        return 0;
    case 5:
        MslRedBlackTree::balance_erase(NodeAt(slot, a), NodeAt(slot, b));
        return 0;
    case 6:
    case 7: {
        SubscriptionTree::node_base* root = NodeAt(slot, b);
        if (op == 6)
            MslRedBlackTree::rotate_left(NodeAt(slot, a), root);
        else
            MslRedBlackTree::rotate_right(NodeAt(slot, a), root);
        return IndexOf(slot, root);
    }
    }
    return -1;
}

// The constructors and destructors. The header constructor runs over 0xa5 bytes in the scratch slot (Header(kScratchSlot)); the tree
// constructor (0x1000ff04) on the same slot; the destructor on a slot of the centre (delete flag <= 0) or the heap header (flag > 0).
void tpw_mc_header_ctor() {
    std::memset(gScratchHeader, 0xa5, sizeof gScratchHeader);
    new (gScratchHeader) CSubscriptionHeader;
}

void tpw_mc_tree_ctor() {
    std::memset(gScratchHeader, 0xa5, sizeof gScratchHeader);
    reinterpret_cast<CSubscriptionHeader*>(gScratchHeader)->mTree.Construct();
}

void tpw_mc_header_dtor(int32_t slot, int32_t flag) {
    if (flag > 0)
        delete Header(slot);
    else
        Header(slot)->~CSubscriptionHeader();
}

// ---- messages and things

// CMsgEvent built in a poisoned buffer: out = flags, event, and 1 when the bytes after the object are untouched.
void tpw_mc_event_ctor(int32_t type, uint32_t* out) {
    alignas(16) unsigned char buffer[64];
    std::memset(buffer, 0xa5, sizeof buffer);
    auto* message = new (buffer) CMsgEvent(static_cast<EventType>(type));
    out[0] = message->mFlags;
    out[1] = static_cast<uint32_t>(message->mEvent);
    bool intact = true;
    for (size_t i = sizeof(CMsgEvent); i < sizeof buffer; ++i) intact = intact && buffer[i] == 0xa5;
    out[2] = intact ? 1 : 0;
    out[3] = static_cast<uint32_t>(message->GetType());
}

// A CMsgEvent destroyed with the delete flag: a positive flag deletes the object (label 3), else it stays where it is.
void tpw_mc_event_dtor(int32_t flag, int32_t present) {
    alignas(16) static unsigned char buffer[sizeof(CMsgEvent)];
    if (flag > 0) {
        CMsgEvent* message = present != 0 ? new CMsgEvent(static_cast<EventType>(7)) : nullptr;
        if (message != nullptr) --tpw_cl_heap[0];   // the test's own allocation is not the function's
        tpw_cl_label(3, message);
        delete message;
    } else {
        auto* message = new (buffer) CMsgEvent(static_cast<EventType>(7));
        message->~CMsgEvent();
    }
}

// std::allocator::deallocate (op 0) on a new node (label 4) and std::allocator<CSubscription>::destroy (op 1).
void tpw_mc_allocator_op(int32_t op, uint32_t count) {
    if (op == 0) {
        auto* node = new SubscriptionTree::node;
        --tpw_cl_heap[0];   // the test's own allocation is not the function's
        tpw_cl_label(4, node);
        SubscriptionAllocator::deallocate(node, count);
    } else {
        CSubscription value;
        SubscriptionAllocator::destroy(&value);
    }
}

// A thing whose id word is own_id and kind byte kind, standing in table entry `entry` (label label).
void tpw_mc_set_thing(int32_t index, int32_t entry, int32_t kind, int32_t ownId, int32_t label) {
    gThings[index].mThingId = static_cast<uint16_t>(ownId);
    gThings[index].mKind = static_cast<uint8_t>(kind);
    if (entry >= 0) gThingArray[entry].mThing = &gThings[index];
    tpw_cl_label(label, &gThings[index]);
}

uint32_t tpw_mc_get_thing_id(int32_t index) { return gThings[index].GetThingId().mIndex; }

void tpw_mc_receive(int32_t index) { gThings[index].ReceiveMessage(gMessage); }

int32_t tpw_mc_post(int32_t type, int32_t dyingId) {
    gMessage.mType = type;
    gMessage.mThingId = static_cast<uint16_t>(dyingId);
    return Centre()->PostMessage(gMessage);
}

}  // extern "C"

// Unit w123a: test-only key function for canonical CMsgDeath; TestMessage overrides it.
int32_t CMsgDeath::GetType() const { return 0x1b; }
// Unit w123c: the death destructor is out of line now (src/MsgDeath.cpp); this library keeps its own empty one.
CMsgDeath::~CMsgDeath() {}
