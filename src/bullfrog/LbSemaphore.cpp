// LbSemaphore.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of LbSemaphore code, named after it.

#include "bfl_FileSystem.h"
#include "LbFile.h"
#include "LbSemaphore.h"
#include "platform/macdoze.h"
#include <cstdlib>
#include <cstring>

// The bullfrog file system's disc side, the part reached through vtables: the access
// object behind a disc file (_TbDiscAccess), the disc storage's members
// (_TbFileStorageAccess: opening, listing, deleting, moving), the storage access base
// (_TbStorageAccess), CAccessItem's own slots, and the CFile and CFileStorage members that
// forward to them (bullfrog shared, 0x10004938..0x10005dc8, 0x10007a9c..0x10008024,
// 0x1000a86c..0x1000ae98, 0x1000b224..0x1000c220). The disk itself is macdoze's Win32
// file API (include/platform/macdoze.h).

// The semaphore is a stub in this build of the library (0x10024a24, 0x10024a54).
_LbSemaphore::_LbSemaphore() {}
_LbSemaphore::~_LbSemaphore() {}

// The bullfrog library's semaphore lock (bullfrog symbols shared, 0x10024ac4..0x10024b9c, LbMaths.cpp range of the
// inventory). The constructor and Release are one `blr` each (the lock does nothing); the constructor is inline in
// include/LbSemaphore.h.

// 0x10024b00: the original's deleting flag (a halfword passed in r4: positive means "delete this") is the compiler's
// handling of `delete`, which a native destructor does not see.
_LbSemaLock::~_LbSemaLock() {}

// 0x10024b6c: always granted.
int32_t _LbSemaLock::Lock(int32_t) { return 1; }
