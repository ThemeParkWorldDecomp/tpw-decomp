// PARTIAL variable tag interface, name of the common base inferred.
#pragma once
#include "CStaticBuffer.h"
struct TbVarFormat {
    const unsigned char* mFormat; // Mac +0
    int16_t mWidth;               // Mac +4
    uint16_t mPrecision;          // Mac +6
    uint32_t mFlags;              // Mac +8
};
class TbVTag {
public:
    virtual int32_t Format(CStaticBuffer&, TbVarFormat&) = 0; // Mac vptr +4, slot 8
    uint32_t mMagic; // Mac +0, 0x5641524d
    uint32_t mId;    // Mac +8
};

// unit w43: the Unicode string tag EntryMsg builds inline (Mac 0x10 bytes: magic, vptr, id, text).
class TbUniStringVTag : public TbVTag {
public:
    TbUniStringVTag(uint32_t id, const char16_t* text) : mText(text) { mMagic = 0x5641524d; mId = id; }
    int32_t Format(CStaticBuffer&, TbVarFormat&) override;   // ltms 0x100092f0, src/ltms/TbVTag.cpp
    const char16_t* mText;   // Mac 0x0c
};

// unit w125: the integer tag InterfaceCashAndGTickTextObject::Change builds inline (Mac 0x14 bytes, vtable import __vt__13TbIntegerVTag): magic, vptr,
// the tag's id (0xd, 0x13 or 0x17 there), the value (+0xc), and a 0 word after it (its vtable has the same Format slot as the other tags).
class TbIntegerVTag : public TbVTag {
public:
    TbIntegerVTag(uint32_t id, int32_t value) : mValue(value) { mMagic = 0x5641524d; mId = id; }
    // ltms 0x1000923c (104 bytes, untranslated: unit w125 only builds the tag; the launcher gets an interim answer in launcher/pending_manual.cpp):
    // swprintf(buffer of 0x11 characters, "%d" (the string at 0x1000fc8e), value), appended to `out`; answers 0.
    int32_t Format(CStaticBuffer& out, TbVarFormat& params) override;
    int32_t mValue;   // Mac 0x0c
};

// Unit v51035: the double tag (Mac 0x14 bytes: magic, vptr, id, then two words at 0x0c and 0x10 that Format hands to swprintf).
class TbDoubleVTag : public TbVTag {
public:
    TbDoubleVTag(uint32_t id, uint32_t first, uint32_t second) : mFirst(first), mSecond(second) { mMagic = 0x5641524d; mId = id; }
    // ltms 0x10009408: swprintf(buffer of 0x11 characters, the string at 0x1000fc8e + 6, mFirst, mSecond), appended with
    // vtable slot 0x18 (AppendUnicode(buffer, length)); answers 0.
    int32_t Format(CStaticBuffer& out, TbVarFormat& params) override;
    uint32_t mFirst;    // Mac 0x0c
    uint32_t mSecond;   // Mac 0x10
};
