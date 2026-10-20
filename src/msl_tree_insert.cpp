// SubscriptionTree::insert_one, main app 0x101530e4 (280 bytes), unit w124: the unique insertion of the message centre's subscription tree (called by
// CMessageCentre::Subscribe every time). The key compares by its thing id, unsigned 16 bit. The Mac answers {node, inserted} through a hidden buffer.
#include "MslSubscriptionTree.h"

SubscriptionInsertResult SubscriptionTree::insert_one(const CSubscription& key) {
    node_base* parent = end();
    node_base* current = mRoot;
    node_base* candidate = nullptr;
    bool left = true;
    bool leftmost = true;
    while (current != nullptr) {
        parent = current;
        uint16_t value = static_cast<node*>(current)->mValue.mThingId;
        if (key.mThingId < value) {
            left = true;
            current = current->mLeft;
        } else {
            left = false;
            leftmost = false;
            candidate = current;
            current = current->mRight;
        }
    }
    if (candidate != nullptr && static_cast<node*>(candidate)->mValue.mThingId >= key.mThingId) {
        return SubscriptionInsertResult{candidate, 0};
    }
    node_base* inserted = insert_node_at(parent, left, leftmost, key);
    return SubscriptionInsertResult{inserted, 1};
}
