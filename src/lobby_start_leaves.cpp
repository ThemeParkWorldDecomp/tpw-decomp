// The small leaves the lobby's start reaches (main app, SimTheme Park 2.04b3): the theme texture folders, the authorised
// user text, the interface's built-in debug font and the interface string heap. The inventory files the first two under
// ui_ErrorDisplay.cpp, Gen8x8DebugFont under FS_MasterSave.cpp and Allocate under ui_cityLobbyController.cpp: the ranges
// do not match their names.
#include <cstdio>
#include <cstring>
#include <new>

#include "InterfaceFont.h"
#include "UI_Base.h"
#include "ui_ErrorDisplay.h"

// ---- the theme texture folders

char gThemeTextureFolder[16];    // 0x103963cc
char gSharedTextureFolder[16];   // 0x103963bc

// 0x101ef430
void SetThemeTextureDirectories(int32_t small) {
    if (small != 0) {
        std::snprintf(gThemeTextureFolder, sizeof(gThemeTextureFolder), "stexture");
        std::snprintf(gSharedTextureFolder, sizeof(gSharedTextureFolder), "ssharete");
    } else {
        std::snprintf(gThemeTextureFolder, sizeof(gThemeTextureFolder), "textures");
        std::snprintf(gSharedTextureFolder, sizeof(gSharedTextureFolder), "sharetex");
    }
}

// ---- the authorised user

// The text the check compares and decodes (data of the main app: two strings and a table in it). Nothing writes them.
char gSecurityKey[] = "QPALZMWOSKXNThe Quick Brown Fox Jumped over the Lazy dog ... blah blah blah.";   // 0x1029d311
char gSecurityText[] = "he Quick Brown Fox Jumped over the Lazy dog ... blah blah blah.";                // 0x1024f351
char gSecurityMask[] = "PQPOEDIJKMCIOOSPLADTOSZAFKLASDJPOYCGFKHGYGHEJLFPZVLKFLIPQWDLFOLFMIE";             // 0x102b1324
char gAuthorizationStamp[] = "No Authorization Stamp";                                                    // 0x102b1368
char gAuthorizedUser[80];                                                                                 // 0x103963dc

// 0x101ef4e0: "No Authorization Stamp", unless the key's text (from its 13th character) differs from the one in the string
// table, which no shipped copy does. Then the answer is a text worked out from the key: the difference between the key's
// characters (from the 12th) and the mask's, up to the first place where they agree. (The loop keeps no terminator of its
// own and stores nothing for the place where the two agree: the buffer's zeros end the text.)
const char* SEC_AuthorizedUser() {
    uint32_t i = 0;
    if (std::strcmp(gSecurityKey + 0xd, gSecurityText) != 0) {
        for (;;) {
            const int32_t difference = static_cast<int32_t>(static_cast<char>(gSecurityKey[i + 0xc])) -
                                       static_cast<int32_t>(static_cast<char>(gSecurityMask[i]));
            if (difference == 0) break;
            gAuthorizedUser[i] = static_cast<char>(difference);
            ++i;
        }
        return gAuthorizedUser;
    }
    return gAuthorizationStamp;
}

// ---- the debug font

// 0x1016906c: the built-in 8x8 font: `a` entries of 8 bytes, each starting with the byte `b` and then the byte `e` and
// ending in a zero word; `c` and `d` become the font's two halfwords at +2 and +6. Nothing but those two (and the
// array) is set when the array cannot be made. (The loop counts in a short against the count as an unsigned halfword.)
void InterfaceFont::Gen8x8DebugFont(int16_t a, int16_t b, int16_t c, int16_t d, int16_t e) {
    mBitmap = new (std::nothrow) uint8_t[static_cast<uint32_t>(static_cast<int32_t>(a)) << 3];
    if (mBitmap != nullptr) {
        mF00 = static_cast<uint16_t>(a);
        for (int16_t i = 0; static_cast<int32_t>(i) < static_cast<int32_t>(mF00); ++i) {
            uint8_t* entry = mBitmap + static_cast<int32_t>(i) * 8;
            entry[0] = static_cast<uint8_t>(b);
            entry[1] = static_cast<uint8_t>(e);
            std::memset(entry + 4, 0, 4);
        }
        mHeight = d;
    }
    mF02 = static_cast<uint16_t>(c);
}

// ---- the interface string heap

// The two tables InterfaceStringHeap::Allocate makes (main app globals 0x10395c34..0x10395c48; the original names are not
// known): table A has `rowsA` rows of `widthA` 16-bit entries, table B `rowsB` rows of `widthB`.
uint8_t* gStringHeapTableB;   // 0x10395c34
uint8_t* gStringHeapTableA;   // 0x10395c38
int32_t gStringHeapRowsB;     // 0x10395c3c  the last argument
int32_t gStringHeapWidthB;    // 0x10395c40  the third
int32_t gStringHeapRowsA;     // 0x10395c44  the second
int32_t gStringHeapWidthA;    // 0x10395c48  the first

// 0x101d7808: makes the two tables and puts -1 in the first word of rows of each. Asks for all four sizes to be given and
// 4 < widthB < widthA; answers 1 when both tables were made, else 0. The sizes are stored first, so a failure leaves them
// (and a table that was made, or NULL) behind. Both loops run `rowsA` times (the second should run `rowsB` times: a bug of
// the original that writes past table B when rowsA is larger than rowsB).
int32_t InterfaceStringHeap::AllocateResult(int32_t a, int32_t b, int32_t c, int32_t d) {
    if (a == 0 || b == 0 || c == 0 || d == 0 || c <= 4 || a <= c) return 0;
    gStringHeapWidthA = a;
    gStringHeapWidthB = c;
    gStringHeapRowsA = b;
    gStringHeapRowsB = d;
    gStringHeapTableA = new (std::nothrow) uint8_t[static_cast<uint32_t>(gStringHeapRowsA * gStringHeapWidthA * 2)];
    gStringHeapTableB = new (std::nothrow) uint8_t[static_cast<uint32_t>(gStringHeapRowsB * gStringHeapWidthB * 2)];
    if (gStringHeapTableA == nullptr || gStringHeapTableB == nullptr) return 0;
    uint8_t* row = gStringHeapTableA;
    for (int32_t i = 0; i < gStringHeapRowsA; ++i) {
        std::memset(row, 0xff, 4);
        row += static_cast<int32_t>(static_cast<uint32_t>(gStringHeapWidthA) << 1);
    }
    row = gStringHeapTableB;
    for (int32_t i = 0; i < gStringHeapRowsA; ++i) {
        std::memset(row, 0xff, 4);
        row += static_cast<int32_t>(static_cast<uint32_t>(gStringHeapWidthB) << 1);
    }
    return 1;
}

// 0x101d7808, the callers' form (see include/UI_Base.h).
void InterfaceStringHeap::Allocate(int32_t a, int32_t b, int32_t c, int32_t d) { AllocateResult(a, b, c, d); }

// 0x101d7978 (60 bytes): gives the two tables back, A then B, with delete[]. The pointers are not cleared and the sizes are left as
// they were (a second call frees the same blocks again).
void InterfaceStringHeap::DeAllocate() {
    delete[] gStringHeapTableA;
    delete[] gStringHeapTableB;
}

// unit w54. 0x101d7b10 (20 bytes): gives a string of the heap back by marking it free: its first two characters become 0xffff (the Mac
// stores the word -1). Nothing for a NULL text.
void InterfaceStringHeap::ReleaseString(char16_t* text) {
    if (text == nullptr) return;
    text[0] = 0xffff;
    text[1] = 0xffff;
}
