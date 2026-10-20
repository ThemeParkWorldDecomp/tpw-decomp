// Coaster instance teardown (main app TPCoaster.cpp, unit w105): DestroyCoasterInstance 0x100437cc (236 bytes),
// DestroyCoasterInstanceFromEntranceID 0x100438fc (92) and DestroyCoasterInstances 0x10043994 (84).
#include "TPCoaster.h"
#include "TPEngine.h"

// 0x100437cc. The object table loses the instance's number and the count goes down before the parts are destroyed. The list is
// unlinked from the head, or by a walk of the whole list (it does not stop at the match; the predecessor is always set by then because
// the head is not the instance), then the queues and the instance itself are freed.
void DestroyCoasterInstance(tag_sCoasterInstance* instance) {
    RemoveObjectInstanceFromTable(gObjectTableCount, gObjectTableFirstFree, gObjectTableLowest, gObjectTable,
                                  instance->mObjectInstanceID);
    --gCoasterInstanceCount;
    DestroyCoasterTrains(instance);
    DestroyCoasterTrack(instance);
    DestroyCoasterPylons(instance);
    tag_sCoasterInstance* head = gCoasterInstances;
    if (instance == head) {
        gCoasterInstances = instance->mNext;
    } else {
        tag_sCoasterInstance* previous = nullptr;
        for (tag_sCoasterInstance* node = head; node != nullptr; node = node->mNext) {
            if (node == instance) previous->mNext = node->mNext;
            previous = node;
        }
    }
    DestroyQueue(&instance->mQueueB0);
    DestroyQueue(&instance->mQueueD0);
    MReleaseFreeF(instance);
}

// 0x100438fc. The first instance of the list with that entrance id, if any.
void DestroyCoasterInstanceFromEntranceID(uint32_t entrance) {
    tag_sCoasterInstance* found = nullptr;
    for (tag_sCoasterInstance* node = gCoasterInstances; node != nullptr; node = node->mNext) {
        if (node->mEntranceID == entrance) {
            found = node;
            break;
        }
    }
    if (found != nullptr) DestroyCoasterInstance(found);
}

// 0x10043994. The next instance is read before each is destroyed; the head is cleared at the end.
void DestroyCoasterInstances() {
    tag_sCoasterInstance* node = gCoasterInstances;
    while (node != nullptr) {
        tag_sCoasterInstance* next = node->mNext;
        DestroyCoasterInstance(node);
        node = next;
    }
    gCoasterInstances = nullptr;
}
