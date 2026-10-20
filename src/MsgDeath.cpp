// CMsgDeath (main app, unit w123c): the constructor 0x10152264, GetType 0x1012ec14 and destructor 0x1012e984. The inventory files them under
// RideDB.cpp and Thing.cpp (unreliable). The three live together because the native vtable needs a key function (GetType) and the
// message is made by CThing::KillMe.
#include "CMsgDeath.h"

// The Mac writes the base class's vtable, a zero flags word, the death vtable and the id at +8, in that order (the vtables are the
// compiler's here). Nothing after the id is written.
CMsgDeath::CMsgDeath(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id) {
    mFlags = 0;
    mThingId = id.mIndex;
}

int32_t CMsgDeath::GetType() const { return 0x1b; }

// The Mac's destructor resets the vtable and deletes the object for a positive flag (the C++ delete-expression does that).
CMsgDeath::~CMsgDeath() {}
