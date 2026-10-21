// TpBody.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of TpBody code, named after it.

#include "AudioPlaceHolder.h"
#include "AudioPlaceHolderList.h"
#include "CSprocketsModule.h"
#include "platform/samsutils.h"
#include "snd_commands.h"
#include "TbAudioBody.h"
#include "TbAudioSystem.h"
#include "TbMapStreamer.h"
#include "TbSoundBank.h"
#include "TbSoundCodecs.h"
#include "TbSoundInfo.h"
#include "TbSoundManager.h"
#include "TbSoundPipe.h"
#include <cmath>
#include <cstdint>
#include <cstring>

// TbSysCommand.cpp: the Execute bodies of the sound library's system commands (unit w79h, "sound symbols shared", Code at 0x10000000,
// TOC r2 = 0x1003abf0; the first 41 are in CSprocketsModule.cpp at 0x10027fd4 to 0x1002a0c4, the rest follow the module's static
// initialiser at 0x1002de28 to 0x1002e348), with the five payload accessors of the command base classes. The classes are declared in
// include/TbAudioSystem.h, where the game builds them; TbAudioBody::SystemCommand (unit w79f) hands each one the body.
//
// Calling convention: every Execute returns its TbError through a hidden r3 buffer (the listing's `this`, the command in r4, the body
// in r5); a body that ends with a call to the sound manager or a virtual of the body answers that call's TbError through the same
// buffer. The manager is reached through the body (GetSoundManager 0x10028110), the placeholder list too (GetPlaceHolderList
// 0x1002814c); neither is tested for NULL here (TbAudioBody::SystemCommand tests both before it runs the command), except by
// GetSampleLength for the list.
//
// Findings (kept as the original does them):
//  - GetMaxNumberOfChannels, SetDroppable, SetDistanceMapping, Process, Tick and the answers-0 commands answer 0 whatever the
//    manager or the body answered.
//  - Process runs the placeholders' tick (ProcessPlaceHolders) BEFORE it sends the Process command to the manager.
//  - The volume commands send (float)(uint)v / 100.0f (the listing's bias trick: a double minus 2^52 rounded to a single by fsubs, then
//    an fdivs by the single 100.0f at 0x100332d0), so the game's 0..100 is an unsigned word (101 is allowed, 0xffffffff is not
//    clamped); the control index is a register the decompiler drops (0 master, 1 music, 2 speech and user 1, 3 user 2 and sfx).
//  - SetHandleVolume's holder is changed even when the sample handle is NULL (answers 0 then).
//  - SetNumberOfChannels answers -1 when the count is unchanged (compared with the list's count BEFORE the list clamps it).

namespace TbSysCommand {

// ---- the audio object's own commands (TpBody's members take the body) -------------------------------------------------------

// 0x1002de28 (64): the library's per-tick work
TbError Tick::Execute(const TbAudioBody* body) const {
    reinterpret_cast<TpBody*>(const_cast<TbAudioBody*>(body))->Tick();
    return TbError(0);
}

// 0x1002deac (80): answers TpBody::Save's TbError
TbError Save::Execute(const TbAudioBody* body) const {
    void* file = GetHandle();
    return reinterpret_cast<const TpBody*>(body)->Save(file);
}

// 0x1002df40
void* Serialise::GetHandle() const {
    return mFile;
}

// 0x1002df84 (96): the byte at +8 goes along unchanged
TbError Load::Execute(const TbAudioBody* body) const {
    void* file = GetHandle();
    return reinterpret_cast<const TpBody*>(body)->Load(file, mFlag);
}

// 0x1002e028 (152): the placeholder's virtual +0x24 (its answer is dropped); -1 for an unknown handle
TbError PauseHandle::Execute(const TbAudioBody* body) const {
    void* handle = GetTbHandleSysValue();
    CAudioPlaceHolder* holder = body->GetPlaceHolderList()->FindPlaceHolder(handle);
    if (holder == nullptr) return TbError(-1);
    holder->Pause();
    return TbError(0);
}

// 0x1002e10c (152): the placeholder's virtual +0x28 (its answer is dropped); -1 for an unknown handle
TbError RestartHandle::Execute(const TbAudioBody* body) const {
    void* handle = GetTbHandleSysValue();
    CAudioPlaceHolder* holder = body->GetPlaceHolderList()->FindPlaceHolder(handle);
    if (holder == nullptr) return TbError(-1);
    holder->Restart();
    return TbError(0);
}

// 0x1002e1f0 (36)
TbError SetPresetReverb::Execute(const TbAudioBody*) const {
    return TbError(0);
}

// 0x1002e264 (36)
TbError RegisterObject::Execute(const TbAudioBody*) const {
    return TbError(0);
}

// 0x1002e2d8 (36)
TbError DeleteObject::Execute(const TbAudioBody*) const {
    return TbError(0);
}

// 0x1002e348 (36)
TbError DeleteAllObjects::Execute(const TbAudioBody*) const {
    return TbError(0);
}

}  // namespace TbSysCommand

// Unit v51113: the small sound-library functions at the edges of CSprocketsModule.cpp (sound 0x10023310 to 0x1002e8fc) and the
// float square roots / cleanup objects of the codec files.

using namespace SamsUtilities;

TbError TpBody::Init(const char*) { return TbError(0); }                      // 0x1002e3bc
// ---- TpBody (0x1002e3bc to 0x1002e8fc)
void TpBody::Tick() { reinterpret_cast<CSprocketsModule*>(this)->Tick(); }   // 0x1002e404
TbError TpBody::Close() { return TbError(0); }                                // 0x1002e44c
TbError TpBody::SetFades(uint32_t) const { return TbError(0); }               // 0x1002e494
// 0x1002e4e0 (100)
TbError TpBody::DeregisterCatagoryBanks(uint32_t category) const {
    if (category == 0) return TbError(-1);
    return TbMapStreamer::Instance()->RemoveCatagory(category);
}

// TpBody::Load / Save (the save-game block of the listener and the placeholder list), the Sound Sprockets command constructors
// and UReferenceCounted::RemoveReference. Unit v51014.

// 0x1002e57c (264): the three coordinates of the listener, each swapped and written, then the list (-1 without a list).
// `this` is the audio body itself (position at +0x34, list at +0x28).
TbError TpBody::Save(void* file) const {
    const TbAudioBody* body = reinterpret_cast<const TbAudioBody*>(this);
    bfl::FileSystem::CFile* out = static_cast<bfl::FileSystem::CFile*>(file);
    float x, y, z;
    body->mPosition.Get(x, y, z);
    x = SamsUtilities::USwap32(x);
    out->Write(&x, 4);
    y = SamsUtilities::USwap32(y);
    out->Write(&y, 4);
    z = SamsUtilities::USwap32(z);
    out->Write(&z, 4);
    if (static_cast<CAudioPlaceHolderList*>(body->mPlaceHolders) != nullptr) {
        body->mPlaceHolders->Save(out);
        return TbError(0);
    }
    return TbError(-1);
}

// 0x1002e6ec (328): reads (and drops) the three coordinates; with a list the list loads, else the block's size word is read and
// the file skips the rest (size - 4 from the current position).
TbError TpBody::Load(void* file, uint8_t flag) const {
    const TbAudioBody* body = reinterpret_cast<const TbAudioBody*>(this);
    bfl::FileSystem::CFile* in = static_cast<bfl::FileSystem::CFile*>(file);
    float a, b, c;
    in->Read(&a, 4);
    in->Read(&b, 4);
    in->Read(&c, 4);
    a = SamsUtilities::USwap32(a);
    b = SamsUtilities::USwap32(b);
    c = SamsUtilities::USwap32(c);
    if (static_cast<CAudioPlaceHolderList*>(body->mPlaceHolders) != nullptr) {
        body->mPlaceHolders->Load(in, flag != 0);
        return TbError(0);
    }
    uint32_t size;
    in->Read(&size, 4);
    size = SamsUtilities::USwap32(size);
    in->Seek(size - 4, 1);
    return TbError(0);
}

TbError TpBody::SetCatagoryReverb(uint32_t) const { return TbError(0); }      // 0x1002e8a4

// 0x1002e8fc (104)
TpBody* TpBody::Destruct(TpBody* self, int16_t deleteFlag) {
    if (self != nullptr) {
        reinterpret_cast<TbAudioBody*>(self)->TbAudioBody::~TbAudioBody();
        if (deleteFlag > 0) USoundChannel_Free(self);
    }
    return self;
}
