// The message centre's post and the message and subscription objects it uses (main app, unit w107): CMessageCentre::PostMessage
// 0x1015256c, the CMsgEvent constructor 0x10151df8 and destructor 0x100f39cc (and GetType 0x100f4470, which its vtable needs), the
// CSubscriptionHeader constructor 0x1000a3f8 and destructor 0x101522e0. The inventory files them under RideDB.cpp and BlockSave.cpp.
#include "CMessageCentre.h"
#include "CMsgDeath.h"
#include "CMsgEvent.h"
#include "CSubscriptionHeader.h"
#include "Debug.h"
#include "World.h"
#include "analyser.h"

// The Mac sets the vtable (twice, through the base class's), a zero flags word and the event type, in this order.
CMsgEvent::CMsgEvent(EventType type) {
    mFlags = 0;
    mEvent = type;
}

// The Mac only resets the vtable and, for a positive flag, deletes the object (the C++ delete does that).
CMsgEvent::~CMsgEvent() {}

int32_t CMsgEvent::GetType() const { return 0x13; }

// An empty tree. The Mac constructs the tree at +4 of the header and touches nothing else.
CSubscriptionHeader::CSubscriptionHeader() { mTree.Construct(); }

// Frees every node when there is a root; the tree's own words are left as they were. The Mac's `this != 0` and `this + 4 != 0` checks
// are the compiler's (a delete of null); the delete flag is the C++ delete.
CSubscriptionHeader::~CSubscriptionHeader() {
    if (mTree.mRoot != nullptr) mTree.destroy(static_cast<SubscriptionTree::node*>(mTree.mRoot));
}

// Type 0x1b is the "thing destroyed" message (CMsgDeath, the dying thing's id as a uint16_t at +8). Before it is delivered the dying
// thing's subscription is removed from every header: the key searched for is the id the THING reports (CThing::GetThingId on the
// thing table's entry for the message's id), one search and at most one erase per header. Then every subscriber of the message's own
// type, in id order, gets it: the thing table's entry of the node's id (five words per entry on the Mac). Quirks kept: a type above
// 0x1c indexes past the 29 headers; a dying thing missing from the table is dereferenced; the table's entry 0 must be free and the
// check only reports (and the report's condition is read again before every delivery).
int32_t CMessageCentre::PostMessage(const CMsg& message) {
    static const char kZeroEntryText[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";
    const int32_t type = message.GetType();
    if (type == 0x1b) {
        const uint16_t dyingId = static_cast<const CMsgDeath&>(message).mThingId;
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kZeroEntryText);
        const CThing* dying = static_cast<const CThing*>(gThingArray[dyingId].mThing);
        for (int32_t i = 0; i < 0x1d; i++) {
            CSubscription key;
            key.mThingId = dying->GetThingId().mIndex;
            SubscriptionTree& tree = mSubscriptions[i].mTree;
            SubscriptionTree::node_base* found = tree.find(key);
            if (found != tree.end()) tree.erase(found);
        }
    }
    SubscriptionTree& tree = mSubscriptions[type].mTree;
    SubscriptionTree::node_base* position = tree.begin();
    while (position != tree.end()) {
        const uint16_t id = static_cast<SubscriptionTree::node*>(position)->mValue.mThingId;
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kZeroEntryText);
        static_cast<CThing*>(gThingArray[id].mThing)->ReceiveMessage(message);
        MslRedBlackTree::increment(position);
    }
    return 1;
}
