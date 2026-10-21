// Unit v5L31: the arena calls of a unit library that links no arena. InterfaceWindow's class operator new and delete (include/
// InterfaceWindow.h) take the game heap, which is the arena on the host; a test library includes this in one file so its windows come
// from the global operator new and go back to the global operator delete, which the call log counts (tests/calllog.cpp), as before.
#pragma once

#include <cstddef>

namespace tpw {
void* ArenaAlloc(std::size_t size) { return ::operator new(size); }
bool InArena(const void*) { return false; }
void ArenaFree(void* block) { ::operator delete(block); }
}  // namespace tpw
