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

int32_t InterfaceFont::GetHeight() { return mHeight; }

void InterfaceFont::Release() {
    if (mBitmap != nullptr) {
        delete[] mBitmap;
        mBitmap = nullptr;
    }
}
