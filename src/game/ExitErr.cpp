// ExitErr.cpp: name proven by __sinit_ExitErr_cpp in the main application.

#include "ActionRecorder.h"
#include "bfl_FileSystem.h"
#include "Debug.h"
#include "ExitErr.h"
#include "Game.h"
#include "InGame.h"
#include "keytable_actions.h"
#include "LbFile.h"
#include "MainWindowCallback.h"
#include "platform/ltms.h"
#include "platform/msl.h"
#include "Ptr32.h"
#include "Serialise.h"
#include "tb_error.h"
#include "TbITable.h"
#include "TbTList.h"
#include "Text.h"
#include "ui_main.h"
#include "World.h"
#include "WorldSavvy.h"
#include "WorldThings.h"
#include <cstdint>
#include <cstring>
#include <new>

// ExitErr.cpp (main app of SimTheme Park 2.04b3). PARTIAL: only CExitError::SetError
// (0x1014b62c) so far; ReportAnyError, HandleEngineErrors and ConvertEngineErrors
// (0x1014b6c8..0x1014bd04) are still to do.

CExitError gExitError;

// The reason the game is giving up: a type, a value and up to 0x50 characters of text (a
// longer text is cut without a terminator, as strncpy leaves it).
void CExitError::SetError(ErrorType type, uint32_t value, const char* text) {
    mType = type;
    mValue = value;
    if (text != nullptr) std::strncpy(mText, text, 0x50);
    mError = 1;
}

// ExitErr.cpp (main app of SimTheme Park 2.04b3): showing the error SetError kept.
// CExitError::ReportAnyError (0x1014b6c8), HandleEngineErrors (0x1014bc38) and
// ConvertEngineErrors (0x1014bd04). SetError is src/ExitErr.cpp.

namespace {

// The UI string id each type asks the string table (gText.mResourcePointers[3]) for: the
// original reaches it through a 43-way jump table at 0x102a4da8 whose blocks all have the
// same shape (FormatString(buffer, 0x100, id)). 0: no string, for type 12 (which never gets
// here) and type 13 (the message is only the text SetError kept).
const uint8_t kStringId[43] = {
    5, 3, 11, 12, 18, 20, 21, 22, 19, 23,   // types 0..9
    24, 25, 0, 0, 42, 28, 29, 30, 31, 32,   // 10..19 (12: the engine errors, 13: none)
    33, 34, 35, 36, 37, 38, 39, 40, 41, 42, // 20..29
    43, 44, 45, 46, 47, 48, 25, 1, 26, 49,  // 30..39
    50, 51, 26,                             // 40..42
};

const uint32_t kBufferSize = 0x100;   // characters of the message buffer (a 512-byte stack buffer)
const char16_t kTitle[] = u"Theme Park World";   // 0x1024878a
const char16_t kFormat[] = u"%ls (%hs)";          // 0x1024878a + 0x22
const uint32_t kBoxFlags = 0x10010;               // MB_ICONERROR | MB_SETFOREGROUND

}  // namespace

void CExitError::ReportAnyError() {
    if (mError == 0 || gText.mInitialised == 0) return;
    // The original's buffer is uninitialised stack: a type of 13 or above 0x2a with a text shows
    // whatever was there before the text. Zeroed here, so the box shows only the text.
    char16_t buffer[kBufferSize] = {};
    if (static_cast<uint32_t>(mType) <= 0x2a) {
        if (mType == kEngineErrors) {
            // Asks the engine what went wrong; that reports (a type it never answers 12 for) and
            // this returns without a box of its own.
            HandleEngineErrors();
            return;
        }
        if (mType != kEmpty) {
            static_cast<TbITable*>(gText.mResourcePointers[3])
                ->FormatString(buffer, kBufferSize, kStringId[mType]);
        }
    }
    if (mText[0] != 0) {
        // "<message> (<text>)", formatted into the message buffer itself in the original (the
        // first argument is the destination). Formatted aside here and copied back, terminator
        // included, so what follows it in the buffer stays as it was.
        char16_t formatted[kBufferSize];
        swprintf(formatted, kBufferSize, kFormat, buffer, mText);
        std::memcpy(buffer, formatted, (wcslen(formatted) + 1) * sizeof(char16_t));
    }
    NS_MacDoze::MessageBoxW(nullptr, buffer, kTitle, kBoxFlags);
}

void CExitError::HandleEngineErrors() {
    // The engine's error stack is emptied; the code of the LAST level-3 record popped (the
    // first one pushed, since Pop takes the newest first) is the one that counts.
    int32_t code = 0;
    while (SError* error = gei_ErrorHandler.Pop()) {
        if (error->mLevel == 3) code = error->mCode;
    }
    mType = static_cast<ErrorType>(ConvertEngineErrors(code));
    mValue = 0;
    mError = 1;
    ReportAnyError();
}

// A binary search over the codes, as the original's nested compares (signed) lay it out. The
// engine's codes are 0xb0754000 + n (the n-th error the engine knows).
int32_t CExitError::ConvertEngineErrors(int32_t code) const {
    const int32_t base = static_cast<int32_t>(0xb0754000);
    if (code == base + 0xe) return 0x1c;
    if (code < base + 0xe) {
        if (code == base + 6) return 0x14;
        if (code > base + 6) {
            if (code == base + 0xa) return 0x18;
            if (code > base + 0xa) {
                if (code == base + 0xc) return 0x1a;
                if (code > base + 0xc) return 0x1b;
                return 0x19;
            }
            if (code == base + 8) return 0x16;
            if (code > base + 8) return 0x17;
            return 0x15;
        }
        if (code == base + 2) return 0x10;
        if (code > base + 2) {
            if (code == base + 4) return 0x12;
            if (code > base + 4) return 0x13;
            return 0x11;
        }
        if (code == base) return 0xe;
        if (code > base) return 0xf;
    } else {
        if (code == base + 0x16) return 0x24;
        if (code < base + 0x16) {
            if (code == base + 0x12) return 0x20;
            if (code > base + 0x12) {
                if (code == base + 0x14) return 0x22;
                if (code > base + 0x14) return 0x23;
                return 0x21;
            }
            if (code == base + 0x10) return 0x1e;
            if (code > base + 0x10) return 0x1f;
            return 0x1d;
        }
        if (code == base + 0x100) return 0x27;
        if (code < base + 0x100) {
            if (code == base + 0x18) return 0x26;
            if (code < base + 0x18) return 0x25;
        } else {
            if (code == base + 0x102) return 0x29;
            if (code < base + 0x102) return 0x28;
        }
    }
    return 0x2a;
}

// Unit v51072: CActionRecorder::ProcessAction (count, text), the 8 byte stand-ins of the recorder and the visitor panel, the destructors
// of the wide string, CMemFile and the thing id list, the file initialisers of InGame.cpp and ExitErr.cpp and a few key table actions.

// 0x1014bf38
void __sinit_ExitErr_cpp() {
    gExitError.mError = 0;
    gExitError.mText[0] = 0;
}
