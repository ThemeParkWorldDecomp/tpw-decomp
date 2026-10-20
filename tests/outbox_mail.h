// The outbox's "mail waits" word for the test libraries that stand a user info up without the real constructors (units w47, w49).
// On the Mac the lobby reads the word at +0x28 of the outbox, the size of its first list of numbers; the host reads that list's
// empty(). A test sets the word through this function: it puts a fresh empty list in place (the store may be raw bytes) and, for a
// nonzero value, gives it libc++'s size word without a node, so that nothing is allocated (some tests count the heap) and nothing
// is freed when the list is dropped (an empty list with a size of 1 links to itself, so clear() frees nothing).
#pragma once
#include <cstddef>
#include <cstdint>
#include <list>
#include <new>

#include "email.h"

namespace outbox_mail {
// libc++'s list: the end node (two links), then the size.
inline std::size_t& SizeWord(std::list<uint32_t>& list) { return reinterpret_cast<std::size_t*>(&list)[2]; }

inline bool LayoutIsLibcxx() {
    std::list<uint32_t> probe;
    probe.push_back(1);
    return SizeWord(probe) == 1;
}

// Checked once when the library loads (the probe allocates, and some tests count the heap).
inline const bool kLayoutIsLibcxx = LayoutIsLibcxx();

inline void Set(Outbox& outbox, int32_t value) {
    new (&outbox.mF28) std::list<uint32_t>();
    if (value != 0 && kLayoutIsLibcxx) SizeWord(outbox.mF28) = 1;
}
}  // namespace outbox_mail
