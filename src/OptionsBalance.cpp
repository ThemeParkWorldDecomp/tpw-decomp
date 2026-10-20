// OptionsBalance.cpp (SimTheme Park 2.04b3).
// Static initializer 0x100215a0; emitted members 0x10021628, 0x10021678, 0x100216c0.
#include "OptionsBalance.h"

// Typed description rows at 0x102990a8, ending with type 12.
const BalanceDescription gOptionsBalanceDescriptions[32] = {
    {0, "", 0, 0, 0.0f, 0.0f, 0, 0u},
    {6, "TEXTUREQUALITY", 0, 4, 0.0f, 0.0f, 0, 0u},
    {6, "TEXTUREFILTERING", 0, 4, 0.0f, 0.0f, 0, 0u},
    {6, "PROCEDURALTEXTURING", 0, 4, 0.0f, 0.0f, 0, 0u},
    {6, "PROCEDURALTEXTURECACHE", 0, 2, 0.0f, 0.0f, 0, 0u},
    {6, "ANIMATINGTEXTURES", 0, 2, 0.0f, 0.0f, 0, 0u},
    {6, "SKYQUALITY", 0, 5, 0.0f, 0.0f, 0, 0u},
    {6, "SKYDETAIL", 0, 2, 0.0f, 0.0f, 0, 0u},
    {6, "SKYSHADOW", 0, 2, 0.0f, 0.0f, 0, 0u},
    {6, "MESHSHADOW", 0, 2, 0.0f, 0.0f, 0, 0u},
    {6, "SPRITESHADOW", 0, 4, 0.0f, 0.0f, 0, 0u},
    {6, "TRIPLEBUFFER", 0, 2, 0.0f, 0.0f, 0, 0u},
    {6, "FOG", 0, 2, 0.0f, 0.0f, 0, 0u},
    {6, "MIPMAP", 0, 2, 0.0f, 0.0f, 0, 0u},
    {6, "RENDER32", 0, 2, 0.0f, 0.0f, 0, 0u},
    {6, "TEXTURE32", 0, 2, 0.0f, 0.0f, 0, 0u},
    {6, "BUMPMAPPING", 0, 2, 0.0f, 0.0f, 0, 0u},
    {6, "FIRSTPERSONVIEWDISTANCE", 0, 4, 0.0f, 0.0f, 0, 0u},
    {6, "LIGHTING", 0, 2, 0.0f, 0.0f, 0, 0u},
    {6, "FORCELOWALPHAREF", 0, 2, 0.0f, 0.0f, 0, 0u},
    {1, "GraphicalOptions", 0, 0, 0.0f, 0.0f, 0, 0u},
    {0, "", 0, 0, 0.0f, 0.0f, 0, 0u},
    {6, "COASTERSMOOTHNESS", 1, 9, 0.0f, 0.0f, 0, 0u},
    {6, "COASTERTRACKDETAIL", 0, 2, 0.0f, 0.0f, 0, 0u},
    {6, "PARTICLEDENSITY", 500, 2001, 0.0f, 0.0f, 0, 0u},
    {6, "TOTALPARTICLES", 1000, 3201, 0.0f, 0.0f, 0, 0u},
    {6, "NUMKIDS", 0, 5, 0.0f, 0.0f, 0, 0u},
    {6, "WEATHER", 0, 2, 0.0f, 0.0f, 0, 0u},
    {6, "WATER", 0, 2, 0.0f, 0.0f, 0, 0u},
    {6, "LOBBYOBJECTS", 0, 101, 0.0f, 0.0f, 0, 0u},
    {1, "GameOptions", 0, 0, 0.0f, 0.0f, 0, 0u},
    {12, "", 0, 0, 0.0f, 0.0f, 0, 0u},
};

const BalanceDescription* COptionsBalance::GetBalanceDescription(int32_t row) {
    return &gOptionsBalanceDescriptions[row];
}
BalanceValue* COptionsBalance::GetBalanceValuesStart() { return mValues; }
COptionsBalance::~COptionsBalance() { UnInit(); }

COptionsBalance gOptionsBalance; // __sinit_OptionsBalance_cpp at 0x100215a0, object at 0x102b4da8
