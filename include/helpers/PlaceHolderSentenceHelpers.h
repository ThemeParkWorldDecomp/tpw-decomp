// PlaceHolderSentenceHelpers.h: file-local helpers that src/sound/PlaceHolderSentence.cpp shares with
// AdvisorScoring.cpp, ListSentence.cpp and PlaceHolderTypes.cpp. Before the regroup, the functions using them shared a
// source file (1 file), which had one copy of each; that copy now lives here, static inline, so each file that includes
// this header keeps its own internal copy.
#pragma once

#include "PlaceHolderTypes.h"

// operator new(0x60, slot) and the constructor: the slot itself, or NULL when there is no slot (nothing is constructed then).
template <class T>
static inline T* Construct(CAudioPlaceHolder* slot) {
    static_assert(sizeof(T) == sizeof(CAudioPlaceHolder), "a placeholder subclass adds no data member (the list's slot holds one)");
    return slot != nullptr ? new (slot) T : nullptr;
}

// The tail all eight four-argument CreateObject statics share: Init through the vtable (+0x18) with the container, the sound data,
// the event's position (+0xc) and its float (+0x18); the object when Init did not fail, else NULL (the object stays constructed).
static inline CAudioPlaceHolder* InitObject(CAudioPlaceHolder* object, CCatagoryContainer* container, void* sound, const sound::TbAudioEvent& event) {
    if (object == nullptr) return nullptr;
    const TbError error = object->Init(container, sound, event.mPosition, event.mW);
    return static_cast<int32_t>(error) >= 0 ? object : nullptr;
}

// The shuffle of a placeholder is kept in the extra data's +0x1c word.
static inline CPlaceHolderSentenceShuffle::CListShuffle* ShuffleOf(CAudioPlaceHolder* holder) {
    return reinterpret_cast<CPlaceHolderSentenceShuffle::CListShuffle*>(holder->mExtra->mParent);
}
