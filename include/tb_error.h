// tb_error.h: the engine's error stack (engine library, original tb_error.cpp; the static
// initialiser at 0x10008a7c constructs gei_ErrorHandler).
//
// PARTIAL: SError, CErrorHandler::Push, Pop and GetAltTabStatus so far.
#pragma once
#include <cstdint>

// One pushed error. Inline constructor (out-of-line copy at 0x10008af0, after tb_error's
// static initialiser).
struct SError {
    SError() : mLevel(0), mCode(0) {}

    int32_t mLevel;   // 0x00  2 for the errors seen so far
    int32_t mCode;    // 0x04  HRESULT-style code, e.g. 0xb0754000 (out of memory)
};

class CErrorHandler {
public:
    CErrorHandler() : mCount(0) {}

    // Records an error unless 64 are already pending (no message, no assert).
    void Push(int32_t level, int32_t code);
    // The most recent error (removed from the stack), or NULL when there is none.
    SError* Pop();
    // Drops every DDERR_SURFACELOST from the stack (the game lost its surfaces, as after
    // an alt-tab); if there was one, raises CSystem::kAltTab and returns 1.
    int32_t GetAltTabStatus();

    SError mStack[64];   // 0x000
    int32_t mCount;      // 0x200
};
static_assert(sizeof(CErrorHandler) == 0x204, "CErrorHandler layout");

extern CErrorHandler gei_ErrorHandler;   // engine data 0x10052b44

// Error codes pushed by the code translated so far.
enum : int32_t {
    kErrOutOfMemory = static_cast<int32_t>(0xb0754000),   // an engine malloc callback failed
    kErrFileOpen = static_cast<int32_t>(0xb0754002),      // CCapture: fopen failed
    kErrInitTwice = static_cast<int32_t>(0xb0754015),     // CEZSprite::Init when already initialised
};
enum : uint32_t {
    kErrSurfaceLost = 0x887601c2,   // DDERR_SURFACELOST
};
