// Argument::Argument(const Argument&) 0x10201878: the copy constructor. Each of the two strings is copied with the library's
// init(first, last), after a length check that throws std::length_error (the fake throw returns, so the copy goes on).
#include <cstdint>

#include "Ptr32.h"
#include "email.h"
#include "online.h"
#include "ui_ime.h"

namespace {

// The length_error the library builds on the stack and throws (the original's message "string constructor: n > max_size").
void ThrowLengthError() {
    static const char kMessage[] = "string constructor: n > max_size";
    static const char kType[] = "!std::exception!!std::logic_error!!std::length_error!!";   // 0x1024f9a0
    IME::MslExceptionObject object;
    object.mVtbl = IME::gMslExcVtblC;   // 0x10278bc0 (dead store in the original too)
    object.mVtbl = IME::gMslExcVtblB;   // 0x10278bb0
    uint32_t size = static_cast<uint32_t>(IME::MslStrlen(kMessage)) + 1;
    char* text = static_cast<char*>(IME::MslNewArray(size));
    IME::MslRefCountedCharsCtor(&object.mRc, text);
    IME::MslStrcpy(object.mRc.mPtr, kMessage);
    object.mVtbl = IME::gMslLengthErrorVtbl;
    IME::MslThrow(kType, &object, IME::gMslLengthErrorDtorTV);
}

}  // namespace

// Callers: std::vector<Argument> (the original takes this and the source, and returns this).
void std_vector_callee::ArgumentCopy(Argument* self, const Argument* other) {
    // The string fields hold 32-bit arena offsets here (Ptr32 style); the representation's data pointer is native at +0x10.
    const IME::MslWideStringRep* narrowRep = reinterpret_cast<const IME::MslWideStringRep*>(
        reinterpret_cast<uintptr_t>(tpw::gArenaBase) + other->mString);
    const char* text = reinterpret_cast<const char*>(narrowRep->mData);
    self->mString = 0;
    uint32_t length = static_cast<uint32_t>(IME::MslStrlen(text));
    if (length > 0xFFFFFFFEu) ThrowLengthError();
    argument_callee::StringInit(&self->mString, text, text + length);

    const IME::MslWideStringRep* wideRep = reinterpret_cast<const IME::MslWideStringRep*>(
        reinterpret_cast<uintptr_t>(tpw::gArenaBase) + other->mWString);
    const char16_t* wtext = wideRep->mData;
    self->mWString = 0;
    uint32_t wlength = online_parser_callee::WcsLen(wtext);
    if (wlength > 0x7FFFFFFEu) ThrowLengthError();
    IME::MslWideString_init(reinterpret_cast<IME::MslWideString*>(&self->mWString), wtext, wtext + wlength);

    self->mCommand = other->mCommand;
}
