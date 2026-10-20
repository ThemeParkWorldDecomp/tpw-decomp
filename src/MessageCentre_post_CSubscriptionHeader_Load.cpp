// CSubscriptionHeader::Save (0x10152928) and Load (0x10152a10): the subscriptions of one message type as a file image: the count, then
// one CSubscription each in id order. The file is little-endian: the Mac swaps its big-endian count on the way out and back in, and on
// the host the count is written and read as it is (swapping it here read a park's 1 as 0x01000000; v5L05, the park's load).
#include "CSubscriptionHeader.h"
#include "LbFile.h"

// Writes the node count, then every subscription in order. The results of the writes are not looked at. Answers 0.
int32_t CSubscriptionHeader::Save(void*& file) const {
    uint32_t swapped = mTree.mCount;
    uint32_t transferred;
    LbFile_Write(file, &swapped, 4, &transferred);
    SubscriptionTree& tree = const_cast<SubscriptionTree&>(mTree);
    SubscriptionTree::node_base* position = tree.begin();
    while (position != tree.end()) {
        static_cast<SubscriptionTree::node*>(position)->mValue.Save(file);
        MslRedBlackTree::increment(position);
    }
    return 0;
}

// Reads the count (the read's result is not looked at), then loads and inserts that many subscriptions (the count compared signed).
// Answers -1 when an insert reports not inserted (an id already there), else 0.
int32_t CSubscriptionHeader::Load(void*& file) {
    uint32_t count;
    uint32_t transferred;
    LbFile_Read(file, &count, 4, &transferred);
    const int32_t total = static_cast<int32_t>(count);
    for (int32_t i = 0; i < total; i++) {
        CSubscription subscription;
        subscription.Load(file);
        SubscriptionInsertResult result = mTree.insert_one(subscription);
        if (!result.mInserted) return -1;
    }
    return 0;
}
