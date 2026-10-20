#include "std_cdeque.h"

namespace std {
void __cdeque<const void*, allocator<const void*>>::swap(__cdeque& other) {
    if (this == &other) return;
    uint32_t t = mWord0; mWord0 = other.mWord0; other.mWord0 = t;
    t = mWord4; mWord4 = other.mWord4; other.mWord4 = t;
    t = mWord8; mWord8 = other.mWord8; other.mWord8 = t;
    t = mWordC; mWordC = other.mWordC; other.mWordC = t;
}
}  // namespace std
