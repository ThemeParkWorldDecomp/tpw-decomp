// MslSupport.cpp: the decomp's stand-ins for MSL library code (the C++ standard library the binary links), not an original source file.

#include "TbIRLE4bitFont.h"
#include "TbResource.h"
#include "TbSoundSampleMPEG2.h"
#include "TbTextFormatter.h"
#include <cstddef>
#include <cstdint>
#include <new>
#include <stdexcept>
#include <utility>

// Unit v51120 (ltms): the small functions of the range 0x100032a8..0x10009400: destructors with the hidden delete flag, the
// reference-releasing smart pointers, two byte swaps, two what() answers, TbMBStringVTag::Format and the resource index's
// static initialiser. (IsDelimiter, SetHorizontalJustification and TbITable::AddRef are in their own files.)

// ---- the Metrowerks vectors: tear_down, then the delete for a flag above 0 (signed)
MslVector<CCharacterInfo>* MslVectorCCharacterInfoDtor(MslVector<CCharacterInfo>* self, int16_t deleteFlag) {   // 0x100060ec
    if (self != nullptr) {
        self->tear_down();
        if (deleteFlag > 0) ::operator delete(static_cast<void*>(self));
    }
    return self;
}

MslVector<CLineInfo>* MslVectorCLineInfoDtor(MslVector<CLineInfo>* self, int16_t deleteFlag) {   // 0x10005fb0
    if (self != nullptr) {
        self->tear_down();
        if (deleteFlag > 0) ::operator delete(static_cast<void*>(self));
    }
    return self;
}

MslVector<TbPoint>* MslVectorTbPointDtor(MslVector<TbPoint>* self, int16_t deleteFlag) {   // 0x10005e7c
    if (self != nullptr) {
        self->tear_down();
        if (deleteFlag > 0) ::operator delete(static_cast<void*>(self));
    }
    return self;
}

CCharInfoList* CCharInfoListDtor(CCharInfoList* self, int16_t deleteFlag) {   // 0x10006060
    if (self != nullptr) {
        self->mChars.tear_down();
        if (deleteFlag > 0) ::operator delete(static_cast<void*>(self));
    }
    return self;
}

// ---- the counted character block (0x10008a10): the last reference frees the characters and the counter
void MslRefCountedCharsDtor(MslRefCountedCharsBlock* self, int16_t deleteFlag) {
    if (self != nullptr) {
        int32_t* count = self->mCount;
        if (count != nullptr) {
            const int32_t before = *count;
            *count = before - 1;
            if (before - 1 == 0) {
                ::operator delete[](static_cast<void*>(self->mPtr));
                ::operator delete(static_cast<void*>(self->mCount));
            }
        }
        if (deleteFlag > 0) ::operator delete(static_cast<void*>(self));
    }
}

MslString* MslStringDtor(MslString* self, int16_t deleteFlag) {   // 0x10008274
    if (self != nullptr) {
        MslRefCountedCharsDtor(&self->mText, -1);
        if (deleteFlag > 0) ::operator delete(static_cast<void*>(self));
    }
    return self;
}

const char* LogicErrorWhat(const LengthError* self) { return self->mMessage.mPtr; }   // 0x100089a0
const char* MslExceptionWhat(const void*) { return "exception"; }                     // 0x100089d8

// ---- the byte swaps in place
void LtmsUSwapInplace32ULong(uint32_t& word) { word = __builtin_bswap32(word); }    // 0x1000439c
void LtmsUSwapInplace16UShort(uint16_t& half) { half = __builtin_bswap16(half); }   // 0x100043e4

// std::exception::~exception (ltms 0x10008304): the MSL exception destructor of the "ltms" library. A NULL object is left alone; else
// the vptr is stored (the vtable at TOC r2 + 0x104c) and, when the hidden delete flag (a short in r4) is positive, the object is freed
// by operator delete (the runtime's, glue 0x1000f694; here the class's static, a fake in the test). The original's exception cleanup
// (__unexpected) is not reachable. Answers this.

struct LtmsStdException {
    static void operator delete(void* object);   // the runtime's operator delete (0x1000f694)
};

// The Mac's vtable object at 0x1003bc3c (r2 0x1003abf0 + 0x104c); only its address is stored.
const void* gLtmsStdExceptionVtbl[2] = {nullptr, nullptr};

void* LtmsStdExceptionDtor(void* self, int16_t deleteFlag);   // 0x10008304 (108)

void* LtmsStdExceptionDtor(void* self, int16_t deleteFlag) {
    if (self != nullptr) {
        *static_cast<const void**>(self) = gLtmsStdExceptionVtbl;
        if (deleteFlag > 0) LtmsStdException::operator delete(self);
    }
    return self;
}

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
