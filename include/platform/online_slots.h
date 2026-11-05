// online_slots.h: the strings the online client keeps as one 32-bit word (a "slot": COnlineControl's player name at +8 and chat
// server at +0xc, an Argument's two strings, the locals the client builds its requests from). Implemented in platform/online_slots.cpp.
//
// The one data model. A slot holds the arena offset of a representation laid out as the host's IME::MslWideStringRep (include/ui_ime.h):
//   +0x00 length, +0x04 capacity, +0x08 count of users, +0x0c the characters' arena offset (the Mac's word),
//   +0x10 the characters' native pointer (what the translated string library reads).
// Both the representation and the characters are in the arena, so the word at +0xc is also the low half of the pointer at +0x10
// (the arena starts on a 4 GB boundary): readers of either layout see the same text. Narrow strings use the same layout with char.
// A slot of 0 reads as the empty string.
#pragma once
#include <cstddef>
#include <cstdint>

namespace tpw::slots {

uint32_t MakeWide(const char16_t* text, size_t length);   // a new representation (one user), registered; its arena offset
uint32_t MakeNarrow(const char* text, size_t length);
const char16_t* WideText(uint32_t word);                   // the characters of a slot's representation ("" for 0)
const char* NarrowText(uint32_t word);
uint32_t Length(uint32_t word);
// The word at `slot`, given up when it names a representation made above (the last user frees it; the slot is then 0). Anything
// else (a zero word, a representation the translated library made, an object of another layout) is left alone: false.
bool Release(void* slot);
bool IsLive(uint32_t word);                                // a representation made above and not yet freed
size_t LiveCount();                                        // how many there are (the tests' check that nothing leaks)

}  // namespace tpw::slots
