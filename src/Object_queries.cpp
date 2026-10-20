// Two one-line questions about a ride or shop object (main app, Object.cpp range; unit w151): CObject::IsClosed 0x1010cae8 and
// CObject::IsBrokenDown 0x1010c878 (24 bytes each). Both answer a 0 or 1 byte in r3 (cntlzw of the difference, shifted).
#include <cstdint>

#include "ThingReceivers.h"

// The object is closed when its running word (+0x64, "1 while running": SetState clears it) is 0.
bool CObject::IsClosed() const { return mF64 == 0; }

// The object is broken down when its state word (+0x198) is 1 (kStateBrokenDown).
bool CObject::IsBrokenDown() const { return mState == kStateBrokenDown; }
