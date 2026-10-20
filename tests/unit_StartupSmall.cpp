// Test entry points for the small main-app start-up functions (library unit_StartupSmall:
// CFixedTime.cpp, CVariableTime.cpp, CommandLine.cpp, TPSound.cpp, TPMovie.cpp, UI_Base.cpp,
// InterfaceFont.cpp, ui_main.cpp, ExitErr.cpp). One operation per call; the results come
// back as a list of numbers in the order tests/test_StartupSmall.py reads the original's.
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

#include "CFixedTime.h"
#include "CVariableTime.h"
#include "Debug.h"
#include "ExitErr.h"
#include "FS_System.h"
#include "Game.h"
#include "InterfaceFontWrapper.h"
#include "TPMovie.h"
#include "TPSound.h"
#include "TbResource.h"
#include "Text.h"
#include "UI_Base.h"
#include "calllog.h"
#include "platform/macdoze.h"
#include "platform/samsutils.h"
#include "rsse.h"
#include "ui_main.h"

using namespace calllog;

namespace {
const char* gCommandText = "";   // what the fake _strdup hands the command line
char gFile;                      // stands for an open file (label 7)
char gFont;                      // stands for an ltms font (label 8)
char gConverters[2];             // stand for the two text converters (labels 10 and 11)
}  // namespace

// ---- Fakes ----------------------------------------------------------------------------------
char* NS_MacDoze::_strdup(const char* text) {
    ev("_strdup", {}, {text});
    const size_t size = std::strlen(gCommandText) + 1;
    char* copy = static_cast<char*>(std::malloc(size));
    std::memcpy(copy, gCommandText, size);
    return copy;
}

void CTRACE(uint32_t level, const char* format, ...) {
    char text[160];
    va_list arguments;
    va_start(arguments, format);
    std::vsnprintf(text, sizeof text, format, arguments);
    va_end(arguments);
    ev("CTRACE", {level}, {text});
}

bfl::FileSystem::CFile* TPFile_Open(const char* name, const char* mode) {
    ev("TPFile_Open", {}, {name, mode});
    return ret("TPFile_Open", 0) ? reinterpret_cast<bfl::FileSystem::CFile*>(&gFile) : nullptr;
}

uint32_t TPFile_Close(bfl::FileSystem::CFile* file) {
    ev("TPFile_Close", {lab(file)});
    return 0;
}

TbError TbIMBToUnicode::ConvertString(char16_t* out, const unsigned char* source, uint16_t size) {
    ev("ConvertString", {lab(this), lab(out), size}, {reinterpret_cast<const char*>(source)});
    return TbError(0);
}

// UIMain_ConvertString (src/ui_main.cpp, unit w44) calls the Unicode to multibyte converter; this unit never calls it.
TbError TbIUnicodeToMB::ConvertString(unsigned char*, const char16_t*, uint16_t) { return TbError(0); }

// The text object's other members are not this unit's: blank ones, so it can exist.
TbTextResourceManager::TbTextResourceManager() {}
TbTextResourceManager::~TbTextResourceManager() {}
TbFontRenderParams::TbFontRenderParams() {}
TbFontRenderParams::~TbFontRenderParams() {}
CText gText;

// ---- the movie player's QuickTime and Toolbox calls (labels: the window 12, the movie
// 13, the graphics device 14)
namespace {
char gWindow[32], gMovieObject[8], gDevice[8];
const FSSpec* gSpec;   // the one FSMakeFSSpec filled
Movie FakeMovie() { return reinterpret_cast<Movie>(gMovieObject); }
}  // namespace
int32_t gScreenWidth, gScreenHeight;
void SamsUtilities::UDebug(const char* format, ...) {
    va_list arguments;
    va_start(arguments, format);
    ev("UDebug", {va_arg(arguments, int)}, {format});
    va_end(arguments);
}
void SamsUtilities::UString::c2pcpy(unsigned char* dest, const char* source) {
    const size_t length = std::strlen(source);
    dest[0] = static_cast<unsigned char>(length);
    std::memcpy(dest + 1, source, length);
}
SamsUtilities::URectangle::~URectangle() {}
SamsUtilities::URectangle::URectangle(int32_t left, int32_t top, int32_t right, int32_t bottom) : mRect{left, top, right, bottom} {
    ev("URectangle", {left, top, right, bottom});
    if (mRect.right < mRect.left) std::swap(mRect.left, mRect.right);
    if (mRect.bottom < mRect.top) std::swap(mRect.top, mRect.bottom);
}
void SamsUtilities::URectangleMac::ToLocal(GrafPort* port) {   // the window is at (10, 20)
    ev("ToLocal", {lab(port), mRect.left, mRect.top, mRect.right, mRect.bottom});
    mRect.left -= 10;
    mRect.right -= 10;
    mRect.top -= 20;
    mRect.bottom -= 20;
}
SamsUtilities::URectangleMac::operator Rect*() {
    static Rect rect;
    rect = Rect{static_cast<int16_t>(mRect.top), static_cast<int16_t>(mRect.left), static_cast<int16_t>(mRect.bottom),
                static_cast<int16_t>(mRect.right)};
    return &rect;
}
OSErr FSMakeFSSpec(int16_t vRefNum, int32_t dirID, const unsigned char* fileName, FSSpec* spec) {
    char name[256];
    std::memcpy(name, fileName + 1, fileName[0]);
    name[fileName[0]] = '\0';
    ev("FSMakeFSSpec", {vRefNum, dirID}, {name});
    gSpec = spec;
    return static_cast<OSErr>(ret("FSMakeFSSpec", 0));
}
OSErr ResolveAliasFile(FSSpec* spec, bool resolveAliasChains, bool* targetIsFolder, bool* wasAliased) {
    ev("ResolveAliasFile", {spec == gSpec, resolveAliasChains, targetIsFolder != nullptr, wasAliased != nullptr});
    return static_cast<OSErr>(ret("ResolveAliasFile", 0));
}
OSErr OpenMovieFile(const FSSpec* spec, int16_t* refNum, int8_t permission) {
    ev("OpenMovieFile", {spec == gSpec, permission});
    *refNum = 77;
    return static_cast<OSErr>(ret("OpenMovieFile", 0));
}
OSErr NewMovieFromFile(Movie* movie, int16_t refNum, int16_t* resId, unsigned char* resName, int16_t flags, bool* changed) {
    ev("NewMovieFromFile", {movie == &gMovie, refNum, *resId, resName == nullptr, flags, changed == nullptr});
    const OSErr error = static_cast<OSErr>(ret("NewMovieFromFile", 0));
    if (error == 0) *movie = FakeMovie();
    return error;
}
void GetMovieBox(Movie movie, Rect* box) {
    ev("GetMovieBox", {lab(movie)});
    *box = Rect{1, 2, 3, 4};
}
void SetPort(GrafPort* port) { ev("SetPort", {lab(port)}); }
void ShowWindow(GrafPort* window) { ev("ShowWindow", {lab(window)}); }
void ValidRect(const Rect* rect) { ev("ValidRect", {rect->top, rect->left, rect->bottom, rect->right}); }
GDevice** GetGWorldDevice(GrafPort* port) {
    ev("GetGWorldDevice", {lab(port)});
    return reinterpret_cast<GDevice**>(gDevice);
}
void SetMovieGWorld(Movie movie, GrafPort* port, GDevice** device) { ev("SetMovieGWorld", {lab(movie), lab(port), lab(device)}); }
void SetMovieBox(Movie movie, const Rect* box) { ev("SetMovieBox", {lab(movie), box->top, box->left, box->bottom, box->right}); }
void SetMovieVolume(Movie movie, int16_t volume) { ev("SetMovieVolume", {lab(movie), volume}); }
void MoviesTask(Movie movie, int32_t maxMilliseconds) { ev("MoviesTask", {lab(movie), maxMilliseconds}); }
TimeValue GetMovieTime(Movie movie, void* currentTime) {
    ev("GetMovieTime", {lab(movie), currentTime == nullptr});
    return static_cast<TimeValue>(ret("GetMovieTime", 5));
}
Fixed GetMoviePreferredRate(Movie movie) {
    ev("GetMoviePreferredRate", {lab(movie)});
    return static_cast<Fixed>(ret("GetMoviePreferredRate", 0x10000));
}
OSErr PrerollMovie(Movie movie, TimeValue time, Fixed rate) {
    ev("PrerollMovie", {lab(movie), time, rate});
    return 0;
}
void SetMovieRate(Movie movie, Fixed rate) { ev("SetMovieRate", {lab(movie), rate}); }
bool IsMovieDone(Movie movie) {
    ev("IsMovieDone", {lab(movie)});
    return ret("IsMovieDone", 0) != 0;
}
void StopMovie(Movie movie) { ev("StopMovie", {lab(movie)}); }
void DisposeMovie(Movie movie) { ev("DisposeMovie", {lab(movie)}); }
OSErr EnterMovies() {
    ev("EnterMovies");
    return 0;
}
void ExitMovies() { ev("ExitMovies"); }

namespace {
int64_t* gOut;
int32_t gCount;
void put(int64_t value) { gOut[gCount++] = value; }
int64_t double_bits(const uint8_t* where) {
    int64_t raw;
    std::memcpy(&raw, where, 8);
    return raw;
}
uint32_t word_at(const uint8_t* where) {
    uint32_t value;
    std::memcpy(&value, where, 4);
    return value;
}
// A bitmap for a font, made the way the game makes them, with a label.
uint8_t* FontBitmap() {
    auto* bitmap = new uint8_t[16];
    newlab(bitmap);
    return bitmap;
}
}  // namespace

extern "C" {

void* tpw_ss_address(int32_t which) {
    switch (which) {
    case 7: return &gFile;
    case 8: return &gFont;
    case 9: return gUIMainConvertBuffer;
    case 10: return &gConverters[0];
    case 12: return gWindow;
    case 13: return gMovieObject;
    case 14: return gDevice;
    default: return &gConverters[1];
    }
}

// One operation; returns how many numbers it wrote to `out`.
int32_t tpw_ss_op(int32_t op, const int32_t* a, const char* text, int64_t* out, char* textOut) {
    gOut = out;
    gCount = 0;
    textOut[0] = '\0';
    alignas(16) uint8_t block[0x220];
    std::memset(block, 0xa5, sizeof block);
    switch (op) {
    case 1: {
        new (block) CFixedTime;
        put(static_cast<int32_t>(word_at(block)));
        put(double_bits(block + 4));
        put(word_at(block + 0xc));
        break;
    }
    case 2: {
        new (block) CVariableTime;
        put(static_cast<int32_t>(word_at(block)));
        put(double_bits(block + 4));
        put(word_at(block + 0xc));
        put(word_at(block + 0x10));
        put(word_at(block + 0x14));
        put(double_bits(block + 0x18));
        put(word_at(block + 0x20));
        break;
    }
    case 3: {
        gCommandText = text;
        auto* line = new CCommandLine;
        put(line->mArgc);
        put(line->GetArgc());
        put(line->GetArgv() == line->mArgv);
        for (int32_t i = 0; i < line->mArgc && i < 0x80; ++i) {
            std::strcat(textOut, line->mArgv[i]);
            std::strcat(textOut, "|");
        }
        delete line;
        break;
    }
    case 4: {
        gSoundSettings.mChanged = 0x11111111;
        gSoundSettings.mSfxOn = 0x22222222;
        gSoundSettings.mMusicOn = 0x33333333;
        gSoundSettings.mSpeechOn = 0x44444444;
        if (a[0] == 0) SOUND_SetMusicStatus(static_cast<uint32_t>(a[1]));
        else if (a[0] == 1) SOUND_SetSfxStatus(static_cast<uint32_t>(a[1]));
        else SOUND_SetSpeechStatus(static_cast<uint32_t>(a[1]));
        put(gSoundSettings.mChanged);
        put(gSoundSettings.mSfxOn);
        put(gSoundSettings.mMusicOn);
        put(gSoundSettings.mSpeechOn);
        break;
    }
    case 5:
        MOVIE_SetVolume(static_cast<uint32_t>(a[0]));
        put(gMovieVolume);
        break;
    case 6:
        UI_SetRealScreenRez(static_cast<int16_t>(a[0]), static_cast<int16_t>(a[1]));
        put(gUIRealScreenWidth);
        put(gUIRealScreenHeight);
        break;
    case 7: {
        char name[160];
        std::strncpy(name, text, 159);
        name[159] = '\0';
        put(CheckFileExists(name));
        break;
    }
    case 8: {   // a[0] the height to give it; a[1], a[2]: a bitmap before Release, before the destructor
        auto* font = new (block) InterfaceFont;
        put(font->mF00);
        put(font->mF02);
        put(font->mF04);
        put(font->mHeight);
        put(font->mBitmap != nullptr);
        put(font->mID);
        font->mHeight = static_cast<int16_t>(a[0]);
        put(font->GetHeight());
        if (a[1]) font->mBitmap = FontBitmap();
        font->Release();
        put(font->mBitmap != nullptr);
        if (a[2]) font->mBitmap = FontBitmap();
        font->~InterfaceFont();
        break;
    }
    case 9: {   // a[0]: with a font; a[1]: a bitmap when it is destroyed
        auto* font = new (block) InterfaceFontWrapper(a[0] ? reinterpret_cast<TbIFont*>(&gFont) : nullptr);
        put(font->mF00);
        put(font->mF02);
        put(font->mF04);
        put(font->mBitmap != nullptr);
        put(lab(font->mFont));
        if (a[1]) font->mBitmap = FontBitmap();
        font->~InterfaceFontWrapper();
        break;
    }
    case 10: {   // a[0]: a text at all; a[1], a[2]: the two converters; a[3]: the non-const overload
        gText.mResourcePointers[0] = a[1] ? &gConverters[0] : nullptr;
        gText.mResourcePointers[1] = a[2] ? &gConverters[1] : nullptr;
        unsigned char source[160];
        std::strncpy(reinterpret_cast<char*>(source), text, 159);
        source[159] = '\0';
        const char16_t* result = a[3] ? UIMain_ConvertMulti(a[0] ? source : nullptr)
                                      : UIMain_ConvertMulti(a[0] ? static_cast<const unsigned char*>(source) : nullptr);
        put(lab(result));
        break;
    }
    case 11: {   // a[0] type, a[1] value, a[2]: with a text
        auto* error = reinterpret_cast<CExitError*>(block);
        error->SetError(static_cast<CExitError::ErrorType>(a[0]), static_cast<uint32_t>(a[1]), a[2] ? text : nullptr);
        put(error->mError);
        put(error->mType);
        put(error->mValue);
        for (int32_t i = 0; i < 0x54; ++i) put(block[0xc + i]);
        break;
    }
    case 12: {   // the movie player: `text` is a script (I Init, P Play, T Tick, S Stop, C Close); a: x, y,
                 // the screen's width and height, the volume, whether a movie is playing at the start
        gScreenWidth = a[2];
        gScreenHeight = a[3];
        gMovieVolume = static_cast<uint32_t>(a[4]);
        gMovie = a[5] ? FakeMovie() : nullptr;
        for (const char* step = text; *step != '\0'; ++step) {
            char path[32];
            std::strcpy(path, "Data:Movies:bf.mov");
            switch (*step) {
            case 'I': MOVIE_Init(); put(0); break;
            case 'P': put(MOVIE_Play(path, reinterpret_cast<GrafPort*>(gWindow), a[0], a[1], 0)); break;
            case 'T': put(MOVIE_Tick()); break;
            case 'S': MOVIE_Stop(); put(0); break;
            case 'C': MOVIE_Close(); put(0); break;
            }
            put(lab(gMovie));
        }
        break;
    }
    }
    return gCount;
}

}  // extern "C"
