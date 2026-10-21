// PlaceHolderTypes.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of PlaceHolderTypes code, named after it.

#include "AudioPlaceHolder.h"
#include "AudioPlaceHolderList.h"
#include "helpers/PlaceHolderSentenceHelpers.h"
#include "LbTime.h"
#include "PlaceHolderHelpers.h"
#include "PlaceHolderTypes.h"
#include "platform/arena.h"
#include "TbMapStreamer.h"
#include "TbSoundBank.h"
#include "TbSoundManager.h"
#include <cstring>
#include <new>

// PlaceHolderTypes.cpp: the kinds of playing sound built on CAudioPlaceHolder (unit w79g3, "sound symbols shared", the library's
// CSprocketsModule.cpp): CPlaceHolderOneShot, CPlaceHolderOneShotSentence and ...BranchingSentence, CPlaceHolderSentence and its
// Linear, Branching, Shuffle and CListSentence subclasses. See include/PlaceHolderTypes.h for the class tree, the vtable slots and the
// extra data's +0x1c word.
//
// Conventions of this file. The listing's operation order is kept (the call logs of the test compare it). A virtual call the Mac makes
// through the object's vtable is a virtual call here; a call the Mac makes by address (the base's functions) is a qualified call.
// Mac addresses of the sound data that the code compares or subtracts are compared as arena offsets (the 32-bit value a Ptr32 field
// holds), because the Mac does that arithmetic on 32-bit addresses. The objects are constructed in place into the list's slot
// (operator new(0x60, slot) returns the slot); every constructor leaves the list's words alone. The destructors' vtable stores and
// the base call with flag 0 are the compiler's chain; the flag-1 deleting form is `delete`.

void CPlaceHolderOneShotBranchingSentence::CreateObject(CAudioPlaceHolder* slot) {
    Construct<CPlaceHolderOneShotBranchingSentence>(slot);
}

// ---- CPlaceHolderOneShotBranchingSentence ------------------------------------------------------------------------------------

CPlaceHolderOneShotBranchingSentence::CPlaceHolderOneShotBranchingSentence() : CPlaceHolderOneShotSentence() {}

// ---- CPlaceHolderOneShotSentence ---------------------------------------------------------------------------------------------

CPlaceHolderOneShotSentence::CPlaceHolderOneShotSentence() : CPlaceHolderOneShot() {
    AllocateExtraData();
}

CPlaceHolderOneShot::~CPlaceHolderOneShot() {}

// ---- CPlaceHolderOneShot -----------------------------------------------------------------------------------------------------

CPlaceHolderOneShot::CPlaceHolderOneShot() : CAudioPlaceHolder() {}

void CPlaceHolderSentence::CreateObject(CAudioPlaceHolder* slot) {
    Construct<CPlaceHolderSentence>(slot);
}

// ---- CPlaceHolderSentence ----------------------------------------------------------------------------------------------------

CPlaceHolderSentence::CPlaceHolderSentence() : CAudioPlaceHolder() {}

void CPlaceHolderOneShotSentence::CreateObject(CAudioPlaceHolder* slot) {
    Construct<CPlaceHolderOneShotSentence>(slot);
}

void CPlaceHolderOneShot::CreateObject(CAudioPlaceHolder* slot) {
    Construct<CPlaceHolderOneShot>(slot);
}

void CPlaceHolderLinearSentence::CreateObject(CAudioPlaceHolder* slot) {
    Construct<CPlaceHolderLinearSentence>(slot);
}

// ---- CPlaceHolderLinearSentence ----------------------------------------------------------------------------------------------

CPlaceHolderLinearSentence::CPlaceHolderLinearSentence() : CPlaceHolderSentence() {}

CPlaceHolderSentence::~CPlaceHolderSentence() {}

void CPlaceHolderBranchingSentence::CreateObject(CAudioPlaceHolder* slot) {
    Construct<CPlaceHolderBranchingSentence>(slot);
}
// ---- CPlaceHolderBranchingSentence -------------------------------------------------------------------------------------------

CPlaceHolderBranchingSentence::CPlaceHolderBranchingSentence() : CPlaceHolderSentence() {}

CPlaceHolderOneShotSentenceElement* CPlaceHolderOneShotBranchingSentence::GetElement() {
    return new CPlaceHolderOneShotBranchingSentenceElement(this);
}

uint32_t CPlaceHolderOneShotBranchingSentence::GetClassID() {
    return 6;
}

CPlaceHolderOneShotBranchingSentence::~CPlaceHolderOneShotBranchingSentence() {}

// AudioPlaceHolderList.cpp: CAudioPlaceHolderList, the pool and the priority list of the sound placeholders (unit w79g1, "sound
// symbols shared", file CSprocketsModule.cpp of the library). See include/AudioPlaceHolderList.h for the layout, the handles and
// the pool; the function addresses are in the header.
//
// Conventions of this file. The listing's operation order is kept (the call logs of the test compare it). The virtual calls on a
// placeholder are `lwz r12, 0(this); lwz r12, SLOT(r12); bl 0x1002f154` on the Mac: Release is slot +0x14, PlaySample +0x1c, Stop
// +0x20, Update +0x30, UpdateParameter +0x34 (all declared by include/AudioPlaceHolder.h in that order). Answers a virtual
// returns through a hidden TbError buffer are dropped where the listing drops them. The file's virtuals are Read +0x10, Write
// +0x14, Seek +0x18 and GetPos +0x20 (bfl::FileSystem::CFile). The words of a save block are written with
// SamsUtilities::USwap32 (include/TbSoundBank.h: the file is little-endian, so the host writes native words). The runtime's
// allocator pair: 0x1002ebfc is operator new[] and 0x1002ebcc operator delete[] (CMemBlock::CreateObject allocates the block with the
// first and the list's destructor frees it with the second, without running ~CMemBlock); 0x1002eaac is LbTime_GetClock.

// 0x1001e694 (76): the vptr, and the delete of the deleting form
CAudioPlaceHolderList::CMemBlock::~CMemBlock() {}
