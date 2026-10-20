// std::exception::~exception (main app 0x10002184): the MSL exception destructor. A NULL object is left alone; else the vtable
// word is stored and, for a flag above 0 (signed 16 bit), the runtime's operator delete frees it. Answers this.
// The listing's landing pad (__unexpected) is not reachable.
#include "TPMain.h"
#include "ui_ime.h"

namespace TPMainExc {

Exception* ExceptionDtor(Exception* self, int16_t flag) {   // 0x10002184
    if (self != nullptr) {
        self->mVtbl = IME::gMslExcVtblC;
        if (flag > 0) OperatorDelete(self);
    }
    return self;
}

}  // namespace TPMainExc
