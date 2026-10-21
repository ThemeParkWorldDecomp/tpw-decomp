// TbSoundSampleWAVFile.cpp: name proven by __sinit_TbSoundSampleWAVFile_cpp in the sound library.

#include "CSprocketsModule.h"
#include "helpers/TbSoundSampleWAVHelpers.h"
#include "snd_handles.h"
#include "TbDynamicStringTemplate.h"
#include "TbGraphicFile.h"
#include "TbSoundBank.h"
#include "TbSoundCodecs.h"
#include "TbSoundCore.h"
#include "TbSoundPipeChannel.h"
#include "TbSoundSampleStore.h"
#include "TbSoundSystemModule.h"
#include <cstring>
#include <new>

// The sound library's four small codecs (unit w80, program "sound symbols shared"): _TbSoundSampleWAV and
// _TbSoundSampleWAVFile (TbSoundSampleWAV.cpp, LbMpeg.cpp), _TbSoundCodecLoop (TbSoundCodecLoop.cpp) and TbTimerCodec
// (TimerCodec.cpp), with CMemoryCopy::Copy. See include/TbSoundCodecs.h for the layouts and the slot order.

namespace {

struct WAVFilePool {
    using Object = _TbSoundSampleWAVFile;
    static Object*& first() { return Object::mFirst_TbSoundSampleWAVFile; }
    static int32_t& current() { return Object::mCurrent_TbSoundSampleWAVFile; }
    static int32_t& low() { return Object::mLow_TbSoundSampleWAVFile; }
    static int32_t& high() { return Object::mHigh_TbSoundSampleWAVFile; }
    static int32_t& total() { return Object::mTotal_TbSoundSampleWAVFile; }
    static int32_t& level() { return Object::mLevel_TbSoundSampleWAVFile; }
    static Object*& link(Object* object) { return object->mWAVFilePoolLink; }
};

}  // namespace

void _TbSoundSampleWAVFile::ClearCache() { PoolClearCache<WAVFilePool>(); }

// ---------------------------------------------------------------- _TbSoundSampleWAVFile

void* _TbSoundSampleWAVFile::operator new(std::size_t size) { return PoolNew<WAVFilePool>(size); }
void _TbSoundSampleWAVFile::operator delete(void* object, std::size_t size) { PoolDelete<WAVFilePool>(object, size); }
void _TbSoundSampleWAVFile::TopUp() { PoolTopUp<WAVFilePool>(); }
void _TbSoundSampleWAVFile::SetLevel(int32_t level) { mLevel_TbSoundSampleWAVFile = level; }
// The file, then the start (the arguments of the Mac constructor are file, header, start, owner).
_TbSoundSampleWAVFile::_TbSoundSampleWAVFile(bfl::FileSystem::CFile* file, TbSampleHeader* header, uint32_t start,
                                             TbRefRelease* owner)
    : _TbSoundSampleWAV(header, owner) {
    mFile = file;
    mStart = start;
}

// Like the memory codec, but the position is the file's: the bytes left are mDataSize - (file position - mStart), read with
// the file's Read (slot +0x10, after GetPos, slot +0x20), and the end of the data is tested by asking GetPos again (a signed
// compare, unlike the memory codec's). The timer gets the bytes just read, without the memory codec's + 0x1200.
int32_t _TbSoundSampleWAVFile::GetDataChunk(void* buffer, int32_t size, int32_t& done, bool stopped) {
    if (Timed() != 0) {
        done = 0;
        return IsTimeComplete();
    }
    int32_t position = mFile->GetPos();
    int32_t left = static_cast<int32_t>(mDataSize - static_cast<uint32_t>(position - static_cast<int32_t>(mStart)));
    done = LB_MIN<int32_t>(size, left);
    mFile->Read(buffer, static_cast<uint32_t>(done));
    int32_t after = static_cast<int32_t>(static_cast<uint32_t>(mFile->GetPos()) - mStart);
    if (after >= static_cast<int32_t>(mDataSize)) {
        if (!SetTime(stopped, static_cast<uint32_t>(done))) return 1;
    }
    return 0;
}

void _TbSoundSampleWAVFile::Restart() {
    mFile->Seek(mStart, 0);
}
_TbSoundSampleWAVFile::~_TbSoundSampleWAVFile() {}

// Unit v51075: small functions of the sound library ("sound symbols shared"): the speaker module's conversions, the bank handle's
// forwards, the static cleanup objects and their __sinit_*_cpp, and a few tiny helpers.

extern "C" void __register_global_object(void* object, void* destructor, void* node);   // runtime glue 0x1002eefc here

uint32_t gCS_TbSoundSampleWAVFileObject;   // 0x1003f2f0
uint32_t gCS_TbSoundSampleWAVFileNode;     // 0x1003f2f4

void __sinit_TbSoundSampleWAVFile_cpp() {
    __register_global_object(&gCS_TbSoundSampleWAVFileObject, reinterpret_cast<void*>(&_CS__TbSoundSampleWAVFile::Destruct),
                             &gCS_TbSoundSampleWAVFileNode);
}

// ---- math ------------------------------------------------------------------------------------------------------------------
namespace tbstd {
float powf(float a, float b) { return static_cast<float>(MslPow(a, b)); }   // frsp of the runtime's pow
float fabs(float x) { return fabsf(x); }
float fabsf(float x) { return static_cast<float>(fabs(static_cast<double>(x))); }
double fabs(double x) { return __builtin_fabs(x); }
}  // namespace tbstd
