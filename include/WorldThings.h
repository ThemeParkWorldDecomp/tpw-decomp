// The world's pending-delete list (unit w121, src/game/World.cpp). Included by the unit's source and its test library only: it
// brings analyser.h and TbTList.h, whose POSITION typedef some files that include World.h define themselves.
#pragma once
#include <cstdint>

#include "TbTList.h"
#include "analyser.h"

// The thing ids CThing::KillMe queued for CWorld::DeletePendingThings. Global 0x1034cbd8 (TOC slot 0x10261488), 16 bytes on the Mac
// (vtable, head +4, tail +8, count +0xc), built by __sinit_World_cpp 0x10141c14 (untranslated); original name unknown. Its nodes are packed
// on the Mac: 10 bytes, the id at +0, the next node at +2, the previous at +6.
extern TbTList<tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>> gPendingDeleteList;

// 0x101414a4 (136 bytes, translated, unit w194, src/game/World.cpp): frees the head node with operator delete, counts the list down and answers the id it held
// (a Mac list that is empty answers without writing its result). Declared so that the generic RemoveHead of TbTList.h is not
// instantiated: the launcher gets a stand-in for the original.
template<> tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>
TbTList<tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>>::RemoveHead();

// 0x10141ad4 (140 bytes, translated, unit w194, src/game/World.cpp): allocates a node (10 bytes on the Mac), prepends it and counts the list up. Declared so
// that the generic AddHead of TbTList.h is not instantiated for the pending list: the launcher gets a stand-in for the original, which
// must pair with RemoveHead above (a real AddHead with the RemoveHead stand-in never empties the list).
template<> void
TbTList<tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>>::AddHead(
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> value);

// 0x10141f1c (100 bytes, unit v51048, src/game/ui/ui_visitorStatus.cpp): empties the list: head and tail cleared first, then each node
// freed with operator delete (its next node, at +2 of the packed node, read before the free), the count cleared. Not declared here: the
// generic RemoveAll of TbTList.h does the same on the host, and other unit libraries do not link the src file.
