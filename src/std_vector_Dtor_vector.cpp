// std::vector<std::locale::facet*>::~vector 0x100724d0 (104): frees the buffer (word at +8) when set, then the vector itself when the
// signed 16 bit delete flag is positive. Answers this. A NULL this does nothing.
#include <cstdint>
#include <cstring>

#include "std_basic_filebuf.h"

namespace locale_imp_callee {
void* VectorFacetDtor(void* vec, int16_t flag) {
    if (vec != nullptr) {
        void* buffer;
        std::memcpy(&buffer, static_cast<uint8_t*>(vec) + 8, sizeof buffer);   // Mac word at +8 (native pointer here)
        if (buffer != nullptr) Delete(buffer);
        if (flag > 0) Delete(vec);
    }
    return vec;
}
}  // namespace locale_imp_callee
