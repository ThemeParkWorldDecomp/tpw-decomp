// Unit v51120 (ltms): the small functions of the range 0x100032a8..0x10009400: destructors with the hidden delete flag, the
// reference-releasing smart pointers, two byte swaps, two what() answers, TbMBStringVTag::Format and the resource index's
// static initialiser. (IsDelimiter, SetHorizontalJustification and TbITable::AddRef are in their own files.)
#include <cstdint>
#include <new>
#include <stdexcept>
#include <utility>
#include "TbTextFormatter.h"
#include "TbIRLE4bitFont.h"

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

// ---- the resource destructors: only the vtable steps (the base destructors are inlined or, for the font, called)
// (TbIUnicodeToMB, TbIMBToUnicode and TbIRLE4bitFont destructors now live in src/TbIUnicodeToMB.cpp,
// src/TbIMBToUnicode.cpp and src/TbIRLE4bitFont.cpp; the test links those files.)

template <class T>
TbRefReleaseSmartPtr<T>::~TbRefReleaseSmartPtr() {   // 0x1000514c, 0x10004f64
    if (mPtr != nullptr) mPtr->Release();
}
template struct TbRefReleaseSmartPtr<TbIMBToUnicode>;
template struct TbRefReleaseSmartPtr<TbIUnicodeToMB>;

int32_t TbMBStringVTag::Format(CStaticBuffer& out, TbVarFormat&) {   // 0x1000937c
    out.AppendMB(mString, 0xffffffffu);
    return 0;
}

// ---- the static initialiser
char gRLE4bitFontFactoryVtbl[16];
char gTableFactoryVtbl[16];
char gMBInfoFactoryVtbl[16];
char gUnicodeToMBFactoryVtbl[16];
char gMBToUnicodeFactoryVtbl[16];
const void* gRLE4bitFontFactoryVptr;
const void* gTableFactoryVptr;
const void* gMBInfoFactoryVptr;
const void* gUnicodeToMBFactoryVptr;
const void* gMBToUnicodeFactoryVptr;

void __sinit_TbIResourceIndex_cpp() {   // 0x10004068
    gMBToUnicodeFactoryVptr = gMBToUnicodeFactoryVtbl;
    gUnicodeToMBFactoryVptr = gUnicodeToMBFactoryVtbl;
    gMBInfoFactoryVptr = gMBInfoFactoryVtbl;
    gTableFactoryVptr = gTableFactoryVtbl;
    gRLE4bitFontFactoryVptr = gRLE4bitFontFactoryVtbl;
}
