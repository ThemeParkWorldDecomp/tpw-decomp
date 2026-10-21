// CMessageCentre::Save 0x10152758, Load 0x10152840, UnsubscribeAll 0x10152448 (unit v5760).
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <new>
#include "AdvisorQueue.h"
#include "CMessageCentre.h"
#include "CMsgDeath.h"
#include "Debug.h"
#include "FS_Online.h"
#include "LbFile.h"
#include "MainBalance.h"
#include "MapCell.h"
#include "ThingReceivers.h"
#include "World.h"
#include "MslSubscriptionTree.h"
#include "SaveLoad.h"
#include "analyser.h"

// 0x10152758: writes the count 0x1d (the Mac's big-endian swap of the word; the host's bytes 1d 00 00 00 are the same),
// then each header; stops at the first nonzero answer and returns it.
int32_t CMessageCentre::Save(void*& file) const {
    uint32_t count = 0x1d;
    uint32_t written;
    LbFile_Write(file, &count, 4, &written);
    for (int32_t i = 0; i < 0x1d; i++) {
        int32_t r = mSubscriptions[i].Save(file);
        if (r != 0) return r;
    }
    return 0;
}

// 0x10152840: clears all 29 trees, reads one word (ignored), then loads each header.
int32_t CMessageCentre::Load(void*& file) {
    for (int32_t i = 0; i < 0x1d; i++) mSubscriptions[i].mTree.clear();
    uint32_t value;
    uint32_t read;
    LbFile_Read(file, &value, 4, &read);
    for (int32_t i = 0; i < 0x1d; i++) {
        int32_t r = mSubscriptions[i].Load(file);
        if (r != 0) return r;
    }
    return 0;
}

// 0x10152448
int32_t CMessageCentre::UnsubscribeAll(const CThing& thing) {
    int32_t total = 0;
    for (int32_t i = 0; i < 0x1d; i++) {
        CSubscription key;
        key.mThingId = thing.GetThingId().mIndex;
        SubscriptionTree& tree = mSubscriptions[i].mTree;
        SubscriptionTree::node_base* it = tree.find(key);
        if (it != tree.end()) {
            tree.erase(it);
            total += 1;
        }
    }
    return total;
}


