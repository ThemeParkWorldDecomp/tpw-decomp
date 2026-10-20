// TpBody::Load / Save (the save-game block of the listener and the placeholder list), the Sound Sprockets command constructors
// and UReferenceCounted::RemoveReference. Unit v51014.
#include <cstdint>
#include <cstring>
#include "AudioPlaceHolder.h"
#include "AudioPlaceHolderList.h"
#include "TbAudioBody.h"
#include "TbAudioSystem.h"
#include "TbMapStreamer.h"
#include "TbSoundBank.h"
#include "TbSoundInfo.h"
#include "TbSoundManager.h"
#include "snd_commands.h"
#include "CSprocketsModule.h"

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

namespace SamsUtilities {

namespace {
void Put32(uint8_t* base, size_t offset, uint32_t value) { std::memcpy(base + offset, &value, 4); }
void PutDouble(uint8_t* base, size_t offset, double value) { std::memcpy(base + offset, &value, 8); }
}  // namespace

// The Mac object: the UCommand base (+0, its own constructor), the vtable word at +4 and the fields. The host keeps the words in the
// base's storage at the Mac offsets (the vtable word is the Mac address of the table).
UCommandSpeakerType::UCommandSpeakerType(SpeakerType type, int32_t flags) {   // 0x1002d7cc
    UCommand_Construct(this);
    Put32(mStorage, 4, 0x1003dce0);
    Put32(mStorage, 8, static_cast<uint32_t>(type));
    Put32(mStorage, 0xc, static_cast<uint32_t>(flags));
}

UCommandVolume::UCommandVolume(double volume) {   // 0x1002cdcc
    UCommand_Construct(this);
    Put32(mStorage, 4, 0x1003dd7c);
    PutDouble(mStorage, 8, volume);
    PutDouble(mStorage, 0x10, volume);
}

UCommandListenerScale::UCommandListenerScale(double x, double y, double z) {   // 0x1002b788
    UCommand_Construct(this);
    Put32(mStorage, 4, 0x1003de58);
    PutDouble(mStorage, 8, x);
    PutDouble(mStorage, 0x10, y);
    PutDouble(mStorage, 0x18, z);
}

UCommandCPULoad::UCommandCPULoad(double load) {   // 0x1002b700
    UCommand_Construct(this);
    Put32(mStorage, 4, 0x1003de80);
    PutDouble(mStorage, 8, load);
}

UCommandSoundType::UCommandSoundType(SoundType type) {   // 0x1002b158
    UCommand_Construct(this);
    Put32(mStorage, 4, 0x1003dfb4);
    Put32(mStorage, 8, static_cast<uint32_t>(type));
}

// The key function of the class (the Mac's destructor lives elsewhere): empty body here, the subclasses own the real ones.
UReferenceCounted::~UReferenceCounted() {}

// 0x1002af28 (120): one reference less; at zero the object disposes itself (virtual +0xc) and is deleted (deleting destructor, +8,
// flag 1); answers 0 then, else the new count.
uint32_t UReferenceCounted::RemoveReference() {
    mCount = mCount - 1;
    if (mCount == 0) {
        Dispose();
        delete this;
        return 0;
    }
    return mCount;
}

}  // namespace SamsUtilities
