// std::basic_string<wchar_t> operator[] 0x10212298 (336 B), main app. The string is one pointer to a representation
// (length +0, capacity +4, references +8, data +0xc on the Mac). A shared representation (references above 1) is copied
// first; the answer is the data address plus the index. The exception cleanup the listing carries is not reachable.
#include "ui_ime.h"

namespace IME {

char16_t* MslWideString_index(MslWideString* self, uint32_t index) {
    MslWideStringRep*& slot = *reinterpret_cast<MslWideStringRep**>(self);
    if (slot->mRefs > 1) {
        uint32_t length = slot->mLength;
        MslWideStringRep* old = slot;
        if (old->mRefs > 1) {
            MslWideStringRep* rep = static_cast<MslWideStringRep*>(MslNewRep());
            if (rep) {
                rep->mCapacity = (length + 3) - (length & 3);
                rep->mRefs = 1;
                rep->mData = static_cast<char16_t*>(MslNew((rep->mCapacity + 1) * 2));
            }
            old->mRefs = old->mRefs - 1;
            uint32_t n = old->mLength < length ? old->mLength : length;
            MslWmemcpy(rep->mData, old->mData, n + 1);
            rep->mLength = old->mLength;
            old = rep;
        }
        slot = old;
    }
    slot->mRefs = 0;
    return slot->mData + index;
}

}  // namespace IME
