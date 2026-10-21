// tb_resource_texture.cpp: name proven by __sinit_tb_resource_texture_cpp in the engine library.

#include "tb_resource_texture.h"

// tb_resource_texture.cpp (decompiled from the engine library of SimTheme Park 2.04b3,
// functions at 0x10029078..0x10029330).

// __sinit_tb_resource_texture_cpp constructs this one global.
CTextureSettings gei_TextureSettings;

CTextureSettings::CTextureSettings() {
    // The original stores 0x142 and ORs the other bits in separately.
    mFlags = 0x142;
    mFlags |= 0x18;
    mFlags |= 0x20;
    mFormat = 4;
    mPixelFormat.dwSize = 0x20;
    mPixelFormat.dwFlags = DDPF_RGB | DDPF_ALPHAPIXELS;
    mPixelFormat.dwFourCC = 0;
    mPixelFormat.dwRGBBitCount = 16;
    mPixelFormat.dwRBitMask = 0xf000;
    mPixelFormat.dwGBitMask = 0x0f00;
    mPixelFormat.dwBBitMask = 0x00f0;
    mPixelFormat.dwRGBAlphaBitMask = 0x000f;
    mFloat18 = 0.5f;
    mUVFixup[1] = -1.0f;
    mUVFixup[0] = -1.0f;
    mFloat08 = 1.5f;
    mFloat0C = 0.8f;
    mTextureMemoryLimit = 0;
    mProcTextureParam = 2500;
}

uint32_t CTextureSettings::GetAlphaMask() {
    return 0xf;
}

void CTextureSettings::CalculateUVFixup() {
    mUVFixup[1] = 0.0f;
    mUVFixup[0] = 0.0f;
    mUVFixup[3] = 0.0f;
    mUVFixup[2] = 0.0f;
}

void CTextureSettings::OutputDebugInfo() {
}

int32_t ConvertSurfaceSizeToFlag(int32_t size) {
    switch (size) {
    case 0x100: return 0;
    case 0x80: return 1;
    case 0x40: return 2;
    case 0x20: return 3;
    case 0x10: return 4;
    case 8: return 5;
    case 4: return 6;
    case 2: return 7;
    case 1: return 8;
    default: return -1;
    }
}
