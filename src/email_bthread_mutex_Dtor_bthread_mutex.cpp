// bthread_mutex::~bthread_mutex (0x10200d04), unit v5120. Neighbour of src/email.cpp (lock, unlock).
#include <cstdint>
#include <new>

#include "UI_pump.h"
#include "online.h"

namespace NS_MacDoze {
void DeleteCriticalSection(void** handle);   // 0x1023b338 (glue import)
}

// The Mac's destructor with its hidden delete flag (r4, a short): a NULL object does nothing; otherwise the critical section is
// deleted, and the object freed when the flag is above 0 (extsh, signed).
bthread_mutex* bthread_mutex::Destroy(bthread_mutex* self, int16_t deleteFlag) {
    if (self != nullptr) {
        NS_MacDoze::DeleteCriticalSection(&self->mHandle);
        if (deleteFlag > 0) online_callee::OperatorDelete(static_cast<void*>(self));   // operator delete, 0x10238218
    }
    return self;
}
