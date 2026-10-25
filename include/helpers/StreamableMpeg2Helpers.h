// StreamableMpeg2Helpers.h: file-local helpers that src/sound/StreamableMpeg2.cpp shares with AdvisorScoring.cpp and
// TbSoundSampleMPEG2.cpp. Before the regroup, the functions using them shared a source file (1 file), which had one
// copy of each; that copy now lives here, static inline, so each file that includes this header keeps its own internal
// copy.
#pragma once

#include <cstdint>
#include <new>

// A size of 0 counts as 1; a block of the class's size comes from the list of idle objects when there is one, anything else
// from the runtime's operator new (import 0x1002eac4, __nw__FUl).
template <class P>
static inline void* PoolNew(std::size_t size) {
    if (size == 0) size = 1;
    if (size != sizeof(typename P::Object)) return ::operator new(size);
    if (P::first() == nullptr) return ::operator new(size);
    --P::current();
    if (P::current() < P::low()) P::low() = P::current();
    ++P::total();
    typename P::Object* object = P::first();
    P::first() = P::link(object);
    return object;
}

// NULL is ignored; a block of the class's size goes on the list, anything else back to the runtime (import 0x1002ea94).
template <class P>
static inline void PoolDelete(void* object, std::size_t size) {
    if (object == nullptr) return;
    if (size != sizeof(typename P::Object)) {
        ::operator delete(object);
        return;
    }
    typename P::Object* idle = static_cast<typename P::Object*>(object);
    P::link(idle) = P::first();
    P::first() = idle;
    ++P::current();
    if (P::high() < P::current()) P::high() = P::current();
}

// Counts the idle objects, allocates blocks up to the level (the runtime's operator new directly: a NULL answer would be
// written through), then raises the high mark and, when the low mark is 0, sets it to the count.
template <class P>
static inline void PoolTopUp() {
    int32_t count = 0;
    for (typename P::Object* object = P::first(); object != nullptr; object = P::link(object)) ++count;
    for (; count < P::level(); ++count) {
        typename P::Object* object = static_cast<typename P::Object*>(::operator new(sizeof(typename P::Object)));
        P::link(object) = P::first();
        ++P::current();
        P::first() = object;
    }
    if (P::high() < P::current()) P::high() = P::current();
    if (P::low() == 0) P::low() = P::current();
}

// Frees the idle objects (a plain delete, not through the list) and zeroes the current and total counts only.
template <class P>
static inline void PoolClearCache() {
    while (P::first() != nullptr) {
        typename P::Object* object = P::first();
        P::first() = P::link(object);
        ::operator delete(object);
    }
    P::current() = 0;
    P::total() = 0;
}
