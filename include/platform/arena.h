// The arena: one reserved range of address space that every block the game loads a file
// into comes from, so that a pointer stored INSIDE such a block fits in the 32 bits the
// file's layout gives it (see include/Ptr32.h for the why and the rules).
//
// A plain allocator otherwise: ArenaAlloc and ArenaFree behave as malloc and free
// (blocks aligned to 16 bytes, contents undefined), ArenaSize is the size asked for.
// Implemented in platform/arena.cpp; any program that uses Ptr32 links that file.
#pragma once
#include <cstddef>
#include <cstdint>
#include <new>

namespace tpw {

void* ArenaAlloc(size_t size);            // nullptr when the arena is used up
void* ArenaAllocZeroed(size_t size);
void ArenaFree(void* block);              // nullptr is fine
size_t ArenaSize(const void* block);      // the size given to ArenaAlloc
bool InArena(const void* address);

// The arena's first byte (reserved at the first call of any function above). A Ptr32 is
// an offset from here; offset 0 is the null pointer, so no block starts at the base.
uint8_t* ArenaBase();
extern uint8_t* gArenaBase;               // ArenaBase() once reserved, nullptr before

// The offset of an address in the arena. Stops the program, naming the address, when it
// is not in the arena: such a pointer cannot be kept in a 32-bit field.
uint32_t ArenaOffset(const void* address);

// An object the game would keep on the stack whose address still goes into a 32-bit field (the save's memory file, which every
// bump-map save chunk keeps through a Ptr32): it lives in the arena for as long as the holder does.
template<class T> class ArenaHeld {
public:
    ArenaHeld() : mObject(new (ArenaAlloc(sizeof(T))) T()) {}
    ~ArenaHeld() {
        mObject->~T();
        ArenaFree(mObject);
    }
    ArenaHeld(const ArenaHeld&) = delete;
    ArenaHeld& operator=(const ArenaHeld&) = delete;
    T& operator*() const { return *mObject; }

private:
    T* mObject;
};

}  // namespace tpw
