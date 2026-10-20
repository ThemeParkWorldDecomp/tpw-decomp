// The bullfrog library's semaphore lock (bullfrog symbols shared, 0x10024ac4..0x10024b9c, LbMaths.cpp range of the
// inventory). The constructor and Release are one `blr` each (the lock does nothing); the constructor is inline in
// include/LbSemaphore.h.
#include "LbSemaphore.h"

// 0x10024b00: the original's deleting flag (a halfword passed in r4: positive means "delete this") is the compiler's
// handling of `delete`, which a native destructor does not see.
_LbSemaLock::~_LbSemaLock() {}

// 0x10024b6c: always granted.
int32_t _LbSemaLock::Lock(int32_t) { return 1; }
