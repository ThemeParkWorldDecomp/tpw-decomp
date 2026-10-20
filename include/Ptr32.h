// Ptr32<T>: a pointer field of a structure that lives inside a loaded file image.
//
// Several of the game's file formats (meshes, animations, sprites) are read into one block
// and used in place: the file holds offsets where the structures hold pointers, and the
// loader turns each offset into a pointer by adding the block's address. Those structures
// are 32-bit through and through (record sizes, offsets counted back from a record, bit
// fields packed next to pointers), and a lot of engine code walks them with the file's
// own arithmetic. A native pointer does not fit in such a field, and widening the fields
// would mean rebuilding every record of every format somewhere else.
//
// So these structures keep the file's layout exactly, and a pointer field is a Ptr32: the
// 32-bit offset of its target from the base of the platform's arena (platform/arena.h),
// 0 for null. Everything such a field may point to therefore has to come from the arena:
// the file blocks do (the engine's allocator), and so must any object whose address the
// game stores into one of these fields. Storing an address from outside the arena stops
// the program at the store, naming the address.
//
// The convention for everything else stands: objects the code builds have native
// pointers (and are larger than on the Mac). Ptr32 is only for fields inside file images.
//
// A Ptr32 reads like the pointer it stands for (->, *, [], comparison, conversion to T*)
// and is trivially copyable, so records can be copied and compared as bytes.
#pragma once
#include <cstddef>
#include <cstdint>

#include "platform/arena.h"

template <class T>
struct Ptr32 {
    uint32_t mOffset;   // from tpw::gArenaBase; 0: null

    T* get() const { return mOffset != 0 ? reinterpret_cast<T*>(tpw::gArenaBase + mOffset) : nullptr; }
    void set(T* target) { mOffset = target != nullptr ? tpw::ArenaOffset(target) : 0; }

    Ptr32& operator=(T* target) {
        set(target);
        return *this;
    }
    operator T*() const { return get(); }
    T* operator->() const { return get(); }
    template <class U = T>
    U& operator*() const {
        return *get();
    }
    template <class Index, class U = T>
    U& operator[](Index index) const {
        return get()[index];
    }
    explicit operator bool() const { return mOffset != 0; }

    // The loaders' step: a file offset (0 stays null) becomes a pointer into the block the
    // file was read into.
    void Relocate(const void* block) {
        if (mOffset != 0) mOffset += tpw::ArenaOffset(block);
    }
};
static_assert(sizeof(Ptr32<int>) == 4, "Ptr32 is the 32-bit field of the file's layout");
