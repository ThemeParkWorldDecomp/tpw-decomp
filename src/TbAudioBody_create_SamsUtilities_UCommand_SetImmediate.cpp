#include "TbAudioBody.h"

#include <cstring>

#include "CMpegBase.h"
#include "CSprocketsModule.h"
#include "TbGraphicFile.h"

// Unit v51107: the small Sound Sprockets pieces of CSprocketsModule.cpp (sound 0x1002879c .. 0x1002c4d8). The Mac objects are raw
// words; a vtable word is the TOC (r2 = 0x1003abf0) plus an offset, stored native-endian.
namespace {
constexpr uint32_t kToc = 0x1003abf0;

uint32_t LoadWord(const void* base, uint32_t offset) {
    uint32_t value;
    std::memcpy(&value, static_cast<const uint8_t*>(base) + offset, 4);
    return value;
}

void StoreWord(void* base, uint32_t offset, uint32_t value) {
    std::memcpy(static_cast<uint8_t*>(base) + offset, &value, 4);
}
}  // namespace

// 0x1002a63c (16)
void UHasFlags_ClearFlag(void* flags, uint32_t flag) { StoreWord(flags, 0, LoadWord(flags, 0) & ~flag); }

// 0x1002a690 (16)
void UHasFlags_SetFlag(void* flags, uint32_t flag) { StoreWord(flags, 0, LoadWord(flags, 0) | flag); }

// 0x1002a858 (12)
void UHasFlags_Construct(void* flags) { StoreWord(flags, 0, 0); }

// 0x1002c4d8 (8)
void UHasFlags_ConstructWith(void* flags, uint32_t value) { StoreWord(flags, 0, value); }

namespace SamsUtilities {

// 0x1002a5c4 (56): the low byte decides.
void UCommand::SetImmediate(uint8_t immediate) {
    if (immediate != 0)
        UHasFlags_SetFlag(this, 1);
    else
        UHasFlags_ClearFlag(this, 1);
}

// 0x1002a7e8 (56)
void UCommand_Construct(UCommand* command) {
    UHasFlags_Construct(command);
    StoreWord(command, 4, kToc + 0x33f8);
}

// 0x1002c468 (56): the same, the flags passed through to UHasFlags(unsigned long).
void UCommand_ConstructFlags(UCommand* command, uint32_t flags) {
    UHasFlags_ConstructWith(command, flags);
    StoreWord(command, 4, kToc + 0x33f8);
}

// 0x1002a774 (56)
UCommandPlay::UCommandPlay() {
    UCommand_Construct(this);
    StoreWord(this, 4, kToc + 0x3420);
}

// 0x1002c1c4 (56)
UCommandStop::UCommandStop() {
    UCommand_Construct(this);
    StoreWord(this, 4, kToc + 0x3218);
}

// 0x1002c3f4 (56): the argument goes to UCommand(unsigned long) as the flags.
UCommandPause::UCommandPause(uint32_t on) {
    UCommand_ConstructFlags(this, on);
    StoreWord(this, 4, kToc + 0x31f0);
}

// 0x1002b838 (72)
UCommandSoundDistance::UCommandSoundDistance(double distance) {
    UCommand_Construct(this);
    StoreWord(this, 4, kToc + 0x3240);
    std::memcpy(static_cast<uint8_t*>(static_cast<void*>(this)) + 8, &distance, 8);
}

// 0x1002a6e0 (8)
template <>
UReferenceCounter<USoundDescription>::operator USoundDescription*() const {
    return mObject;
}

// 0x1002adfc (20)
uint32_t UReferenceCounted::AddReference() { return ++mCount; }

// 0x1002afec (64): keeps the object and counts a reference unless it is NULL. (The Mac answers the counter itself.)
void USoundDescriptionCounter_Make(UReferenceCounter<USoundDescription>* self, USoundDescription* description) {
    self->mObject = description;
    if (self->mObject != nullptr) reinterpret_cast<UReferenceCounted*>(self->mObject)->AddReference();
}

// 0x1002879c's neighbour: the status test, ok when the word is 0.
int32_t UStatus::IfOk(TbLegacyError ok, TbLegacyError failed) const {
    if (mValue == 0) return ok.mValue;
    return failed.mValue;
}

}  // namespace SamsUtilities

// 0x1002a94c (8), 0x1002a99c (36), 0x1002b1fc (8)
void CSprocketsChannelMac::SetPipe(void* pipe) { mPipe = pipe; }

SamsUtilities::USoundDescription* CSprocketsChannelMac::GetDescription() { return mDescription; }

uint8_t CSprocketsChannelMac::Is3D() { return m3D; }

// 0x1002b2d0 (56)
CSprocketsSpeaker::CSprocketsSpeaker() {
    TbSpeaker::ModuleConstruct(this);
    StoreWord(this, 0, kToc + 0x3394);
}

namespace TbSpeaker {
// 0x1002b338 (12)
void ModuleConstruct(void* module) { StoreWord(module, 0, kToc + 0x336c); }

// 0x1002b6c4 (12)
void ConfigConstruct(void* config) { StoreWord(config, 0, kToc + 0x32a4); }

// 0x1002b65c (56)
void StereoConstruct(void* stereo) {
    ConfigConstruct(stereo);
    StoreWord(stereo, 0, kToc + 0x1cdc);
}
}  // namespace TbSpeaker

// Lead 2026-10-20 (v51107): the three CSprocketsModule functions below are weak, and so are the class's vtable and typeinfo,
// which the destructor (the key function) emits here: platform/sound_module.cpp holds the host versions the launcher links
// (as v51053 did for SetSpeedOfSound and SetListenerVelocity), the unit test library takes these.
asm(".weak_definition __ZTV16CSprocketsModule\n.weak_definition __ZTI16CSprocketsModule\n.weak_definition __ZTS16CSprocketsModule");

// 0x1002b374 (100): the Mac stores its vtable, runs the base destructor with the flag 0 and deletes when the delete flag is
// above 0 (the C++ destructor leaves that to the caller). The NULL test is the caller's.
__attribute__((weak)) CSprocketsModule::~CSprocketsModule() {}

// 0x1002bd90 (36), 0x1002beb8 (36): both answer TbError(0).
__attribute__((weak)) TbError CSprocketsModule::SetOptions(uint32_t) { return TbError(0); }

__attribute__((weak)) TbError CSprocketsModule::Set3DDSPFunctionality(uint32_t, uint32_t) { return TbError(0); }

namespace SamsUtilities {
// 0x1002bbe4 (64)
template <>
void* UManager<UAllocator>::operator->() {
    if (mObject == nullptr) Allocate();
    return mObject;
}
}  // namespace SamsUtilities

// 0x1002879c (20)
void TbRect_Construct(TbRect* rect, int32_t left, int32_t top, int32_t right, int32_t bottom) {
    rect->left = left;
    rect->top = top;
    rect->right = right;
    rect->bottom = bottom;
}
