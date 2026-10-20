// std::basic_filebuf<wchar_t>: unshift 0x10226dcc, sync 0x1021e22c, ~basic_filebuf 0x1021cd4c (unit v5743).
#include "platform/arena.h"
#include "std_basic_filebuf.h"
#include "std_basic_ifstream.h"

using namespace filebuf_unshift_callee;

// Writes the codecvt's shift-back bytes to the file. 2 (noconv) answers false; 0 and 1 write the bytes (1 asks again);
// any other answer ends the loop with true.
bool BasicFilebufWchar::unshift() {
    char buf[4];
    int32_t result = 1;
    while (result == 1) {
        char* toNext = buf;
        result = CodecvtUnshift(reinterpret_cast<void*>(tpw::gArenaBase + mCodecvt),
                                &mState[0], buf, buf + 4, &toNext);   // gArenaBase is a byte pointer
        if (result == 2) return false;
        if (result >= 0 && result < 2) {
            const char* p = buf;
            while (p < toNext) {
                char c = *p++;
                if (filebuf_wseek_callee::Fputc(static_cast<int32_t>(c), mFile) == -1) return false;
            }
        }
    }
    return true;
}

int32_t BasicFilebufWchar::sync() {
    bool failed = true;
    if (mFile != 0) {
        if (Fflush(mFile) == 0) failed = false;
    }
    return failed ? -1 : 0;
}

BasicFilebufWchar* BasicFilebufWchar::destroy(int16_t flag) {
    BasicFilebufWchar* self = this;
    if (self != nullptr) {
        // The host's 32-bit value of an address is its offset from the arena base (the globals are outside the arena: wraps).
        const uintptr_t base = reinterpret_cast<uintptr_t>(tpw::gArenaBase);
        uint32_t files = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(gIfsWFiles) - base);
        self->mVtable = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(gIfsWVtblD) - base);
        uint32_t f = self->mFile;
        if (f != files && f != files + 0x54 && f != files + 0xa8) self->close();
        self->mVtable = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(gIfsWVtblC) - base);
        LocaleDtor(reinterpret_cast<uint8_t*>(self) + 0x1c, -1);
        if (flag > 0) filebuf_wseek_callee::Delete(self);
    }
    return self;
}
