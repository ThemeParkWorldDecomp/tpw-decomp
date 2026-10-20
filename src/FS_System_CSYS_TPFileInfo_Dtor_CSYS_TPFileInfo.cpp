#include "FS_System.h"
#include "Game.h"
#include "LbFile.h"
#include "TPGlobals.h"
#include "TPResource.h"
#include "TPString.h"

// 0x10005288: the destructor is defined inline in FS_System.h (the Mac's NULL test and the delete flag belong to
// the delete-expression; the five strings are destroyed by the members), so every library that uses it links.

// Weak: a test library replaces it with a recording fake.
__attribute__((weak)) void CSYS_TPFileInfo::operator delete(void* object) { ::operator delete(object); }
