#include "DrawRenderClasses.h"
#include "TbRaConvert.h"
#include "LbString.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>

// 0x10066200 (152 B): copies src into dst, at most size - 1 bytes plus a NUL.
// Returns 1 when the whole string fit (strlen < size - 1, unsigned compare), else 0.
// size == 0 wraps to a huge limit: the whole string is copied (original's behaviour).
int32_t LbString_strnzcpy(char* dst, const char* src, uint32_t size) {
    uint32_t length = static_cast<uint32_t>(std::strlen(src));
    uint32_t limit = size - 1;
    if (length < limit) {
        std::memcpy(dst, src, length + 1);
        return 1;
    }
    std::memcpy(dst, src, limit);
    dst[size - 1] = 0;
    return 0;
}
