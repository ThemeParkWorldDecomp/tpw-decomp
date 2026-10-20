// std::basic_ostream<char> constructor (0x10072828) and destructor (0x10070000), unit v5116.
#include <cstdint>

#include "email.h"

// Data blocks the vtable words point at (the Mac's 0x1029d0b0, 0x1029d0d8, 0x103bd034).
char gOstreamVt1[0x28];
char gOstreamVt2[0x20];
char gIosBaseVt[0x20];

namespace {
uint32_t Word(const void* address) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(address)); }
}  // namespace

// 0x10070000, 152 bytes. deleteFlags is the hidden short: nonzero destroys the virtual ios part, above zero frees.
MacOstream* BasicOstreamDtor(MacOstream* self, int16_t deleteFlags) {
    if (self == nullptr) return self;
    self->mVtbl = Word(gOstreamVt2);
    MacOstreamIos* ios = self->mIos.get();
    ios->mVtbl = Word(gOstreamVt2) + 0xc;
    ios = self->mIos.get();
    ios->mVbaseOffset = static_cast<int32_t>(reinterpret_cast<intptr_t>(&self->mIosPart) - reinterpret_cast<intptr_t>(ios));
    if (deleteFlags != 0) {
        self->mIosPart.mVtbl = Word(gOstreamVt1);
        OstreamIosBaseDtor(&self->mIosPart, 0);
    }
    if (deleteFlags > 0) OstreamDelete(self);
    return self;
}

// 0x10072828, 188 bytes. withVirtualBase (a short) nonzero: this is the most derived class and builds the ios part at +8.
MacOstream* BasicOstreamCtor(MacOstream* self, int16_t withVirtualBase, uint32_t streambuf) {
    if (withVirtualBase != 0) {
        self->mIos = &self->mIosPart;
        self->mIosPart.mVtbl = Word(gIosBaseVt);
        self->mIosPart.mVtbl = Word(gOstreamVt1);
    }
    self->mVtbl = Word(gOstreamVt2);
    MacOstreamIos* ios = self->mIos.get();
    ios->mVtbl = Word(gOstreamVt2) + 0xc;
    ios = self->mIos.get();
    ios->mVbaseOffset = static_cast<int32_t>(reinterpret_cast<intptr_t>(&self->mIosPart) - reinterpret_cast<intptr_t>(ios));
    ios = self->mIos.get();
    OstreamIosInit(ios, streambuf);
    ios->mF3c = 0;
    ios->mFill = static_cast<uint8_t>(OstreamWiden(ios, ' '));
    return self;
}
