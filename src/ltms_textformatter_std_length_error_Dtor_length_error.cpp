#include "TbTextFormatter.h"
#include <new>
#include "TbResource.h"

// Unit v5749: std::length_error::~length_error (0x10008108).
// The vtable stand-ins the destructor steps through (TOC +0x102c, +0x103c, +0x104c on the Mac).
char gLengthErrorVtbl[16];
char gLogicErrorVtbl[16];
char gExceptionVtbl[16];

// Weak: the runtime's operator delete (glue 0x1000f694); a test library replaces it with a recording fake.
__attribute__((weak)) void LengthErrorOperatorDelete(void* block) { ::operator delete(block); }

LengthError* LengthErrorDtor(LengthError* self, int16_t deleteFlag) {
    if (self != nullptr) {
        self->mVtable = gLengthErrorVtbl;
        self->mVtable = gLogicErrorVtbl;
        MslRefCountedCharsDtor(&self->mMessage, -1);
        self->mVtable = gExceptionVtbl;
        if (deleteFlag > 0) LengthErrorOperatorDelete(self);
    }
    return self;
}
