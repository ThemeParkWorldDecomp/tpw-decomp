// TbAudioBody_create.cpp: CreateAudioObject (0x1002a18c), the sound library's entry point, decompiled from SimTheme Park 2.04b3
// ("sound symbols shared", Code at 0x10000000, TOC r2 = 0x1003abf0; file CSprocketsModule.cpp).
//
// Why it is alone in its file: the launcher's libtpw.a is a static archive and launcher/pending_manual.cpp defines an interim
// CreateAudioObject (it answers -1, so SOUND_Init fails cleanly) that is always linked. This archive member is therefore never
// pulled in while the interim exists, and no duplicate symbol arises. When the interim is deleted, the real one links by itself.
// The test unit (cmake/units/AudioBody.cmake) lists this file next to src/TbAudioBody.cpp.
#include "TbAudioBody.h"

// 0x1002a18c (196): the sound library's entry point. A new body is stored in *audio before it is started; a failed start deletes
// it (Release, virtual slot +8) and clears *audio; the answer is 0 or -1. (The Mac does not test the allocation for NULL before
// the virtual call.)
TbError CreateAudioObject(const char* name, TbAudioSystem** audio, const CAudioConfig& config) {
    TbAudioBody* body = new TbAudioBody();
    *audio = body;
    if ((*audio)->Init(name, config) < 0) {
        (*audio)->Release();
        *audio = nullptr;
        return TbError(-1);
    }
    return TbError(0);
}
