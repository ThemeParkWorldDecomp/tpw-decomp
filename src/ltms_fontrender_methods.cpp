// ltms TbFontRender.cpp, the four 4-bit font render method objects (unit w45b; ltms symbols shared 0x1000a0c0..0x1000e068 for the
// constructors and destructors, 0x1000d020 for SetWriteZero and 0x10002298 for the static initialiser): the objects the table
// gFontRenderMethods holds and the table itself. The two one-colour classes' pixel writers are in
// src/ltms_fontrender_onecolour.cpp and src/ltms_fontrender_onecolour_noblend.cpp; the two textured classes' writers
// (0x1000a1dc..0x1000cf98) are untranslated.
#include "TbFontRender.h"

// ---- constructors and destructors. Each constructor runs Tb4BitFontRenderMethod's (inline) and stores its class's vtable
// (Mac 0x100115b4 textured, 0x10011614 textured without blending, 0x10011674 one colour without blending, 0x100116d4 one
// colour); each destructor stores it again, runs ~Tb4BitFontRenderMethod (0x10000cb4) and, when the hidden flag is positive,
// deletes the object (the compiler's deleting form).

// 0x1000dff0
TbOneColour4BitFontRenderMethod::TbOneColour4BitFontRenderMethod() {}
// 0x1000e068
TbOneColour4BitFontRenderMethod::~TbOneColour4BitFontRenderMethod() {}
// 0x1000d050
TbOneColourNoBlend4BitFontRenderMethod::TbOneColourNoBlend4BitFontRenderMethod() {}
// 0x1000d0d0
TbOneColourNoBlend4BitFontRenderMethod::~TbOneColourNoBlend4BitFontRenderMethod() {}
// 0x1000a0c0
TbTextured4BitFontRenderMethod::TbTextured4BitFontRenderMethod() {}
// 0x1000a138
TbTextured4BitFontRenderMethod::~TbTextured4BitFontRenderMethod() {}
// 0x1000b9c8
TbTexturedNoBlend4BitFontRenderMethod::TbTexturedNoBlend4BitFontRenderMethod() {}
// 0x1000ba48
TbTexturedNoBlend4BitFontRenderMethod::~TbTexturedNoBlend4BitFontRenderMethod() {}

// ---- SetWriteZero

// The byte at 0x10011768. Nothing in the ltms library reads it (the symbols name it `writeZero`).
bool gWriteZero = false;

// 0x1000d020
void SetWriteZero(bool writeZero) { gWriteZero = writeZero; }

// ---- the objects and the table

// __sinit_TbFontRender_cpp (0x10002298) constructs the four objects, in this order, and registers each destructor to run at exit
// (__register_global_object); here the compiler does both for the global definitions, which follow the order of the original.
TbOneColour4BitFontRenderMethod gOneColour4BitFontRenderMethod;                 // 0x10011744
TbOneColourNoBlend4BitFontRenderMethod gOneColourNoBlend4BitFontRenderMethod;   // 0x10011734
TbTextured4BitFontRenderMethod gTextured4BitFontRenderMethod;                   // 0x10011724
TbTexturedNoBlend4BitFontRenderMethod gTexturedNoBlend4BitFontRenderMethod;     // 0x10011714

// The table is initialised data of the Mac (0x1001103c, relocated pointers; the static initialiser does not touch it): 2 blends x
// 5 types x 3 styles, all null but four cells at type 4. [blend][type][style], blend 0 = after UseAlphaWriteMode, style 1 = one
// colour, style 2 = textured.
TbFontRenderMethod* gFontRenderMethods[2][5][3] = {
    {{}, {}, {}, {}, {nullptr, &gOneColourNoBlend4BitFontRenderMethod, &gTexturedNoBlend4BitFontRenderMethod}},
    {{}, {}, {}, {}, {nullptr, &gOneColour4BitFontRenderMethod, &gTextured4BitFontRenderMethod}},
};
