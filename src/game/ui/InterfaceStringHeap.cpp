// InterfaceStringHeap.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of InterfaceStringHeap code, named after it.

#include "InterfaceFont.h"
#include "platform/msl.h"
#include "UI_Base.h"
#include "ui_controls.h"
#include "ui_ErrorDisplay.h"
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <new>

// The small leaves the lobby's start reaches (main app, SimTheme Park 2.04b3): the theme texture folders, the authorised
// user text, the interface's built-in debug font and the interface string heap. The inventory files the first two under
// ui_ErrorDisplay.cpp, Gen8x8DebugFont under FS_MasterSave.cpp and Allocate under ui_cityLobbyController.cpp: the ranges
// do not match their names.

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

// Unit v5975: InterfaceTextBox::CalcTextPos 0x101d7dac, InterfaceTextBox::SetText 0x101d7f50, InterfaceStringHeap::GetString 0x101d79ec,
// InterfaceStringHeap::GetAndCopy 0x101d7b60, InterfaceTimer::OperatorDelete 0x101d822c (the class's operator delete).

// The heap's tables (src/lobby_start_leaves.cpp; main app globals 0x10395c34..0x10395c48).
extern uint8_t* gStringHeapTableB;   // 0x10395c34
extern uint8_t* gStringHeapTableA;   // 0x10395c38
extern int32_t gStringHeapRowsB;     // 0x10395c3c
extern int32_t gStringHeapWidthB;    // 0x10395c40
extern int32_t gStringHeapRowsA;     // 0x10395c44
extern int32_t gStringHeapWidthA;    // 0x10395c48

char16_t* InterfaceStringHeap::GetString(int32_t length) {
    char16_t* found = nullptr;
    uint8_t* rowA = gStringHeapTableA;
    if (rowA == nullptr) return nullptr;
    uint8_t* rowB = gStringHeapTableB;
    if (rowB == nullptr) return nullptr;
    if (length < gStringHeapWidthB) {
        for (int32_t i = 0; i < gStringHeapRowsB && found == nullptr; ++i) {
            if (*reinterpret_cast<int32_t*>(rowB) == -1) found = reinterpret_cast<char16_t*>(rowB);
            rowB += static_cast<int32_t>(static_cast<uint32_t>(gStringHeapWidthB) << 1);
        }
    }
    if (length < gStringHeapWidthA && found == nullptr) {
        for (int32_t i = 0; i < gStringHeapRowsA; ++i) {   // no early exit: the last free row wins
            if (*reinterpret_cast<int32_t*>(rowA) == -1) found = reinterpret_cast<char16_t*>(rowA);
            rowA += static_cast<int32_t>(static_cast<uint32_t>(gStringHeapWidthA) << 1);
        }
    }
    if (found != nullptr) *found = 0;
    return found;
}

// unit w54. 0x101d7b10 (20 bytes): gives a string of the heap back by marking it free: its first two characters become 0xffff (the Mac
// stores the word -1). Nothing for a NULL text.
void InterfaceStringHeap::ReleaseString(char16_t* text) {
    if (text == nullptr) return;
    text[0] = 0xffff;
    text[1] = 0xffff;
}

char16_t* InterfaceStringHeap::GetAndCopy(const char16_t* text) {
    int32_t length = static_cast<int32_t>(wcslen(text));
    char16_t* string = GetString(length);
    if (string != nullptr) wcscpy(string, text);
    return string;
}
