// InterfaceFont and InterfaceFontWrapper (main app of SimTheme Park 2.04b3,
// 0x10168ae0..0x10168ff8; the inventory files the range under FS_MasterSave.cpp).
// PARTIAL: Gen8x8DebugFont and the width functions (0x1016906c..0x101692d4) are still to do.
#include "InterfaceFontWrapper.h"

InterfaceFontWrapper::InterfaceFontWrapper(TbIFont* font) : mFont(font) {}

InterfaceFontWrapper::~InterfaceFontWrapper() {}

InterfaceFont::InterfaceFont() {
    mBitmap = nullptr;
    mF00 = 0;
    mF02 = 2;
    mF04 = 2;
}

InterfaceFont::~InterfaceFont() {
    if (mBitmap != nullptr) delete[] mBitmap;
}

// GetHeight, GetWidth, GetCharWidth and LastCharIn are virtual on the Mac (the vptr at +0x10) and InterfaceFontWrapper
// overrides them; on the host they are not, so the four bodies pass a wrapper (every font CText holds) on to its own
// version, the call the Mac makes. They are not virtual here because many unit tests fake them on addresses that are
// not objects.
int32_t InterfaceFont::GetHeight() {
    if (auto* const wrapper = dynamic_cast<InterfaceFontWrapper*>(this)) return wrapper->GetHeight();
    return mHeight;
}

void InterfaceFont::Release() {
    if (mBitmap != nullptr) {
        delete[] mBitmap;
        mBitmap = nullptr;
    }
}
