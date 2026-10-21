// Bank.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of Bank code, named after it.

#include "CSprocketsModule.h"
#include "snd_handles.h"
#include "TbDynamicStringTemplate.h"
#include "TbGraphicFile.h"
#include "TbSoundBank.h"
#include "TbSoundCodecs.h"
#include "TbSoundCore.h"
#include "TbSoundPipeChannel.h"
#include "TbSoundSampleStore.h"
#include "TbSoundSystemModule.h"
#include <cstddef>
#include <cstring>
#include <new>

// The sound library's sample banks (file TbMMFileBank.cpp of the Mac's "sound symbols shared" library, with the factory and
// the bank handle from ChannelState.cpp). See include/TbSoundBank.h for the layouts, the vtables and the file format.
//
// Every allocation goes through the nothrow forms of operator new, new[] and delete[] (the library's own allocator pair on
// the Mac: __nw__FUl, __nwa__FUl and __dla__FPv), so that a failed allocation is a NULL result as it is there.

// ---- Bank::CBank -------------------------------------------------------------------------------------------------------

// 0x100143e8 (76 bytes)
Bank::CBank::CBank(TbBank* bank) : mBank(bank) {}

// 0x10014464 (124 bytes): releases the bank (no test for NULL, as in the original).
Bank::CBank::~CBank() {
    mBank->Release();
}

// 0x10014508 (64 bytes): the bank's file handle, or NULL without a bank.
Bank::CBank::operator bfl::FileSystem::CFile*() const {
    return mBank != nullptr ? mBank->GetHandle() : nullptr;
}

// Unit v51075: small functions of the sound library ("sound symbols shared"): the speaker module's conversions, the bank handle's
// forwards, the static cleanup objects and their __sinit_*_cpp, and a few tiny helpers.

// ---- Bank::CBank ------------------------------------------------------------------------------------------------------------
namespace Bank {
bool CBank::IsType(uint32_t type) const {
    if (mBank == nullptr) return false;
    return mBank->IsType(type);
}
int32_t CBank::GetNumberOfSamples() const {   // 0x100146a8 (unit v5L21)
    if (mBank == nullptr) return 0;
    return mBank->GetNumberOfSamples();
}
uint32_t CBank::GetSampleLength(uint32_t index) const {
    if (mBank == nullptr) return 0;
    return mBank->GetSampleLength(index);
}
const char* CBank::GetSampleName(uint32_t index) const {
    if (mBank == nullptr) return nullptr;
    return mBank->GetSampleName(index);
}
}  // namespace Bank

// 0x10014884 (112 bytes): 1 when the bank gave the sample's header, 0 without a bank or when it failed.
int32_t Bank::CBank::RetrieveSample(TbSampleHeader** out, uint32_t index) const {
    if (mBank == nullptr) return 0;
    const int32_t result = mBank->RetrieveSample(out, index);
    return (result >> 31) + 1;
}

// 0x1001458c (8 bytes): the bank, as the reference counted object the codecs keep (the word at +8, not tested).
TbRefRelease* Bank::CBank::GetBankAsRefRelease() const {
    return mBank;
}

// 0x100145cc (64 bytes): the bank's IsStreaming (slot +0x34), false without a bank.
bool Bank::CBank::IsStreaming() const {
    return mBank != nullptr ? mBank->IsStreaming() : false;
}

// 0x10014720 (64 bytes): the bank's GetSamplePosition (slot +0x28), 0 without a bank.
uint32_t Bank::CBank::GetSamplePosition(uint32_t index) const {
    return mBank != nullptr ? mBank->GetSamplePosition(index) : 0;
}
