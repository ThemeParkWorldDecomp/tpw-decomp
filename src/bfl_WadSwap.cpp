#include "platform/samsutils.h"

namespace SamsUtilities {
template<class T> void USwapBlock32(T* words, int32_t count) {
    _USwapBlock32(reinterpret_cast<uint32_t*>(words), count);
}
template<class T> void USwapInplace32(T& word) {
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    const uint32_t value = static_cast<uint32_t>(word);
    word = static_cast<T>((value >> 24) | ((value >> 8) & 0xff00) |
                          ((value << 8) & 0xff0000) | (value << 24));
#else
    (void)word;
#endif
}
template void USwapBlock32<uint32_t>(uint32_t*, int32_t);
template void USwapInplace32<uint32_t>(uint32_t&);
}
