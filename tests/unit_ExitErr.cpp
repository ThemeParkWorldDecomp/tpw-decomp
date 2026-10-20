// Test library for the exit error report (libtpw_unit_ExitErr.dylib; see tests/test_ExitErr.py): src/ExitErr_report.cpp
// (CExitError::ReportAnyError, HandleEngineErrors, ConvertEngineErrors) with src/ExitErr.cpp's gExitError. Real under them:
// the 16-bit C library (swprintf and wcslen, platform/msl.cpp). Fakes that record through calllog: the UI string table's
// FormatString (it writes the case's text), the engine's error stack (CErrorHandler::Pop answers the case's records in
// order) and NS_MacDoze::MessageBoxW (it records the whole message buffer, so that nothing the box would show is missed).
#include <cstdint>
#include <cstring>

#include "ExitErr.h"
#include "InterfaceFont.h"
#include "InterfaceFontWrapper.h"
#include "TbFontRenderParams.h"
#include "TbITable.h"
#include "TbTextResourceManager.h"
#include "Text.h"
#include "calllog.h"
#include "platform/ltms.h"
#include "tb_error.h"

using namespace calllog;

// ---- globals of other files (the text system is only read: mInitialised and the string table pointer)
CText gText;
CErrorHandler gei_ErrorHandler;
InterfaceFont::InterfaceFont() {}
InterfaceFont::~InterfaceFont() {}
InterfaceFontWrapper::InterfaceFontWrapper(TbIFont* font) : mFont(font) {}
InterfaceFontWrapper::~InterfaceFontWrapper() {}
TbFontRenderParams::~TbFontRenderParams() {}
TbFontRenderParams::TbFontRenderParams() {}
TbTextResourceManager::TbTextResourceManager() {}
TbTextResourceManager::~TbTextResourceManager() {}

namespace {

// The string table object: the code only calls FormatString on it, which the fake answers, so a block of bytes is enough.
alignas(16) unsigned char gTableBlock[64];
const int32_t kLabelTable = 5;

// The text FormatString writes (the case's), and the engine's records in the order Pop answers them.
char16_t gFormatText[0x200];
SError gRecords[80];
int32_t gRecordCount;
int32_t gNextRecord;

// FNV-1a over 16-bit characters: stands for a whole buffer in one recorded integer.
int64_t Hash(const char16_t* text, uint32_t count) {
    uint32_t hash = 2166136261u;
    for (uint32_t i = 0; i < count; ++i) {
        hash = (hash ^ (text[i] & 0xff)) * 16777619u;
        hash = (hash ^ (text[i] >> 8)) * 16777619u;
    }
    return hash;
}

}  // namespace

// ---- the UI string table: FormatString writes the case's text (cut to the size), whatever the id
int32_t TbITable::FormatString(char16_t* out, uint32_t size, uint32_t id, ...) const {
    ev("FormatString", {lab(this), size, id});
    if (size == 0) return 0;
    uint32_t count = 0;
    while (gFormatText[count] != 0 && count < size - 1) {
        out[count] = gFormatText[count];
        ++count;
    }
    out[count] = 0;
    return static_cast<int32_t>(count);
}

// ---- the engine's error stack: Pop answers the case's records, then NULL
SError* CErrorHandler::Pop() {
    ev("Pop", {this == &gei_ErrorHandler});
    return gNextRecord < gRecordCount ? &gRecords[gNextRecord++] : nullptr;
}

// ---- the message box: the whole 256-character buffer and the title (17 characters with the terminator) go in the log
int32_t NS_MacDoze::MessageBoxW(GrafPort* owner, const char16_t* text, const char16_t* title, uint32_t flags) {
    uint32_t length = 0;
    while (text[length] != 0 && length < 0x100) ++length;
    ev("MessageBoxW", {lab(owner), flags, length, Hash(text, 0x100), Hash(title, 17)}, {narrow(text).c_str(), narrow(title).c_str()});
    return 1;
}

extern "C" {

// One call. op: 0 ReportAnyError, 1 HandleEngineErrors, 2 ConvertEngineErrors (answer = the code's type). The object gExitError
// starts as (error, type, value, the 0x50 text bytes); the dump after is outInts[3] and outText[0x50]. `formatText` is what
// FormatString writes; `records` are (level, code) pairs, `count` of them, in the order Pop answers them.
int32_t tpw_ee_run(int32_t op, uint32_t initialised, int32_t error, int32_t type, uint32_t value, const uint8_t* text,
                   const char16_t* formatText, const int32_t* records, int32_t count, int32_t code, int32_t* outInts,
                   uint8_t* outText) {
    gText.mInitialised = initialised;
    gText.mResourcePointers[3] = gTableBlock;
    tpw_cl_label(kLabelTable, gTableBlock);
    std::memset(gFormatText, 0, sizeof gFormatText);
    for (int i = 0; i < 0x1ff && formatText[i] != 0; ++i) gFormatText[i] = formatText[i];
    gRecordCount = count;
    gNextRecord = 0;
    for (int i = 0; i < count; ++i) {
        gRecords[i].mLevel = records[2 * i];
        gRecords[i].mCode = records[2 * i + 1];
    }
    gExitError.mError = error;
    gExitError.mType = static_cast<CExitError::ErrorType>(type);
    gExitError.mValue = value;
    std::memcpy(gExitError.mText, text, sizeof gExitError.mText);
    int32_t answer = 0;
    switch (op) {
    case 0: gExitError.ReportAnyError(); break;
    case 1: gExitError.HandleEngineErrors(); break;
    case 2: answer = gExitError.ConvertEngineErrors(code); break;
    }
    outInts[0] = gExitError.mError;
    outInts[1] = gExitError.mType;
    outInts[2] = static_cast<int32_t>(gExitError.mValue);
    std::memcpy(outText, gExitError.mText, sizeof gExitError.mText);
    return answer;
}

}  // extern "C"
