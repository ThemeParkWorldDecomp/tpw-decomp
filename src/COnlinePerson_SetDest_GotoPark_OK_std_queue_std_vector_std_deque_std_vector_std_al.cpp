#include <new>
#include "GameSys.h"
#include "online.h"
#include "platform/samsutils.h"

// 0x10213a08 std::queue<std::vector<Argument>, std::deque<...>>::~queue (deleting form when the flag is > 0).
// The two address tests on the member (this + 4) in the listing can never fail and are dropped.
MslArgumentQueue* ArgumentQueueDestruct(MslArgumentQueue* self, int16_t deleteFlag) {
    if (self != nullptr) {
        online_callee::DequeTearDown(self);
        if (self->mBlocks != 0) online_callee::OperatorDelete(reinterpret_cast<void*>(static_cast<uintptr_t>(self->mBlocks)));
        if (deleteFlag > 0) online_callee::OperatorDelete(self);
    }
    return self;
}
