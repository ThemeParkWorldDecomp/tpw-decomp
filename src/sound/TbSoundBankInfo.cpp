// TbSoundBankInfo.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of TbSoundBankInfo code, named after it.

#include "AudioPlaceHolder.h"
#include "TbDynamicStringTemplate.h"
#include "TbSoundBank.h"
#include "TbSoundCodecs.h"
#include "TbSoundSampleCallback.h"
#include <cstddef>
#include <cstring>
#include <new>

// The sample callback pipes of the sound library (unit w79i, program "sound symbols shared"): TbSoundUserCallback (UserCallback.cpp),
// _TbSoundSampleCallback and CAutoDetach (TbSoundSampleCallback.cpp), _TbSoundSampleCompletedCallback (TbSoundSamplePipe.cpp, its
// constructor and GetDataChunk in LbMpeg.cpp) and the three cleanup objects. See include/TbSoundSampleCallback.h for the layouts
// and the vtables.

// ---------------------------------------------------------------- the static cleanup objects

_CS_TbSoundUserCallback::~_CS_TbSoundUserCallback() { TbSoundUserCallback::ClearCache(); }

// The sound library's sample banks (file TbMMFileBank.cpp of the Mac's "sound symbols shared" library, with the factory and
// the bank handle from ChannelState.cpp). See include/TbSoundBank.h for the layouts, the vtables and the file format.
//
// Every allocation goes through the nothrow forms of operator new, new[] and delete[] (the library's own allocator pair on
// the Mac: __nw__FUl, __nwa__FUl and __dla__FPv), so that a failed allocation is a NULL result as it is there.

// 0x10013fb4 (140 bytes): a TbMMFileBank (0x20 bytes) over the info's file, holding a reference on the component's TbRefRelease
// base (the component plus 4 on the Mac, NULL for no component). Answers success whatever the allocation did.
TbError CBankFactory::ConstructBank(TbBank*& out, const TbSoundBankInfo& info, CAudioComponent* component) {
    out = nullptr;
    info.GetSize();
    TbMMFileBank* bank = new (std::nothrow) TbMMFileBank(component, info.GetFile());
    out = bank;
    return TbError(0);
}

// ---- TbMMFileBank ------------------------------------------------------------------------------------------------------

// 0x100140a0 (96 bytes): the base with its reference on the owner, the vtable, a copy of the file handle.
TbMMFileBank::TbMMFileBank(TbRefRelease* owner, bfl::FileSystem::CFile* file) : TbBank(owner), mFile(*file) {}

// 0x10014150 (8 bytes)
bfl::FileSystem::CFile* TbSoundBankInfo::GetFile() const {
    return mFile;
}

// 0x10014188 (8 bytes)
uint32_t TbSoundBankInfo::GetSize() const {
    return mSize;
}

// 0x100141c0 (316 bytes): the path goes through a UString and a UMemoryWatcher (nothing else is done with them), the bank
// is made by the virtual ConstructBank and registered with the info's path and size; when that fails the bank is released.
// The result wraps the bank in a Bank::CBank (0xc bytes), or is NULL.
Bank::CBank* CBankFactory::GenerateBank(const TbSoundBankInfo& info, CAudioComponent* component) {
    SamsUtilities::UMemoryWatcher watcher((SamsUtilities::UString(info.GetPath())));
    TbBank* bank = nullptr;
    const TbError made = ConstructBank(bank, info, component);
    if (static_cast<int32_t>(made) >= 0 && bank != nullptr) {
        const uint32_t size = info.GetSize();
        const TbError registered = bank->RegisterBank(info.GetPath(), size);
        if (static_cast<int32_t>(registered) >= 0) {
            return new (std::nothrow) Bank::CBank(bank);
        }
        bank->Release();
    }
    return nullptr;
}

// ---- SamsUtilities -----------------------------------------------------------------------------------------------------

// 0x10014354 (4 bytes): a bare return.
SamsUtilities::UMemoryWatcher::UMemoryWatcher(const UString& name) {}
// 0x100143b0 (8 bytes)
const char* TbSoundBankInfo::GetPath() const {
    return mPath;
}
