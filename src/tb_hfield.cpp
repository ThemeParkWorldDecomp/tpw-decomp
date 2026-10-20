// tb_hfield.cpp range of the engine library (SimTheme Park 2.04b3): the view volume and
// the height field. PARTIAL: only what engine start-up and shutdown run; the rest of the
// inventory's range (48 functions) is still to do.
#include "tb_hfield.h"

#include <new>

#include "Wavelet.h"
#include "tb_error.h"
#include "tb_proctexture.h"

// ---- CGamut ---------------------------------------------------------------------------
CGamut* gei_Gamut = nullptr;

int32_t Gamut_Init() {
    if (gei_Gamut != nullptr) return 0;
    CGamut* gamut = new (std::nothrow) CGamut;
    gei_Gamut = gamut;
    if (gamut == nullptr) {
        gei_ErrorHandler.Push(3, kErrOutOfMemory);
        return 0;
    }
    gamut->SetYClipper(0.0f, 0.0f);
    return 1;
}

int32_t Gamut_Close() {
    if (gei_Gamut == nullptr) return 0;
    delete gei_Gamut;
    gei_Gamut = nullptr;
    return 1;
}

CGamut::CGamut() {
    mF6C = 0;
    mFC8 = 0;
    mFC4 = 0;
    mF1D0 = 0;
    mF1CC = 0;
    mYClipHigh = 0.0f;
    mYClipLow = 0.0f;
    for (float& value : mF3C) value = 0.0f;
    for (int32_t& value : mF54) value = 0;
    mFC0 = 0;
}

void CGamut::SetYClipper(float low, float high) {
    mYClipLow = low;
    mYClipHigh = high;
}

// ---- CHField --------------------------------------------------------------------------
CHField* gei_HField = nullptr;

int32_t HField_Init() {
    if (gei_HField != nullptr) return 0;
    gei_HField = new (std::nothrow) CHField;
    if (gei_HField == nullptr) {
        gei_ErrorHandler.Push(3, kErrOutOfMemory);
        return 0;
    }
    return 1;
}

int32_t HField_Close() {
    if (gei_HField != nullptr) {
        gei_HField->Close();
        delete gei_HField;
        gei_HField = nullptr;
    }
    return 1;
}

CHField::CHField() {
    mF24 = 0;
    mF20 = 0;
    mF12C = 0;
    mF128 = 0;
    mF00 = 0;
    mF04 = 0;
    mProcTextures = nullptr;
    mF238 = 0;
    mF234 = 0;
    mF240 = 0x7f;
    mF23C = 0x7f;
    mF2958 = 0;
    mF12968 = 0;
    mF12970 = nullptr;
}

CHField::~CHField() { Close(); }

int32_t CHField::Close() {
    if (mProcTextures != nullptr) {
        mProcTextures->Close();
        delete mProcTextures;
        mProcTextures = nullptr;
    }
    if (mF12970 != nullptr) gFree(mF12970);
    mF00 = 0;
    mF04 = 0;
    return CloseGamut();
}

int32_t CHField::CloseGamut() { return 1; }
