// Test library for the advisor (libtpw_unit_Advisor.dylib; see tests/test_Advisor.py): src/Advisor.cpp (the state
// machine), src/TPAdvisor_model.cpp (the model and its slots), src/advisor_leaves.cpp (the small callees),
// ScaleLMatrixNoTrans (src/rsse_matrix.cpp, with the real SetMatrixIdentity) and SND_DuckVolume (src/TPSound.cpp), and
// the response table (src/advisor_lines.cpp).
//
// Real: those, and CLight (src/Light.cpp). Fakes, each recording its name and every argument (tests/calllog.h): the
// ride set loader, instancing, set-up and destruction, the animation changes and the animation tick, the mesh draw
// switches, the engine's map and system, the sound wrappers, the effect manager, the particle emitters, the
// interface's key display, the clock, rand, _stricmp, the trace, and malloc and free (only for blocks the test labels
// and, for malloc, while recording). For the advisor's speech (tests/test_AdvisorSpeak.py): FindFilePath, TPFile_Open,
// get_theme_path and the lip-sync file's LbFile_* calls, and _USwapBlock32, which does nothing here as it does nothing
// on a little-endian host (the case's file words are put in memory the way the Mac's swap leaves them).
//
// The test builds the advisor's world from numbers it writes into memory on both sides (the mesh header and nodes in
// the arena, as file images; the sets, meshes, group entries and time controls native), runs one function at a time
// through tpw_ad_op and reads the state back. The animation model of the fakes: SetCurRideAnim queues the animation in
// time control 0 (words 0x24 and 0x28, the kind and the place), and the next few AnimateRideSet calls (anim & 3, plus
// one) run it, after which the control says "nothing queued" (kind 12) again.
#include <dlfcn.h>

#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>

#include "Advisor.h"
#include "Clock.h"
#include "Debug.h"
#include "FS_System.h"
#include "LbFile.h"
#include "Light.h"
#include "Main.h"
#include "MapWho.h"
#include "Quickload.h"
#include "TPAdvisor.h"
#include "TPConfig.h"
#include "TPEngine.h"
#include "TPSettings.h"
#include "TPSound.h"
#include "calllog.h"
#include "platform/arena.h"
#include "platform/macdoze.h"
#include "platform/msl.h"
#include "platform/samsutils.h"
#include "rsse.h"
#include "tb_resource_system.h"

using namespace calllog;

// offsetof on the game's classes (a virtual base, constructors) is what the layout export is for.
#pragma clang diagnostic ignored "-Winvalid-offsetof"

// ---- globals of other files that the unit owns here
CVariableTime::CVariableTime() {}
CFixedTime::CFixedTime() {}
CClock gClock;
CSystem::CSystem() {}
CSystem gei_System;
CPlayerSettings gPlayerSettings;
void CPlayerSettings::Init() {}
CPlayerSettings::~CPlayerSettings() {}
CLight* gev_LightChain;
CMapLinkBase::~CMapLinkBase() {}

namespace {
alignas(16) unsigned char gMapStorage[16];
int32_t gAnimLeft;                 // AnimateRideSet calls left until the queued animation is over
uint32_t gRandState = 1;
std::string gFileSeen;
std::string gTheme;                // get_theme_path's text
std::string gFileBytes;            // the lip-sync file as it is on disk (little-endian words)
char gFound[700];                  // FindFilePath's answer (its buffer, as the original's gFilePathBuffer)
alignas(16) char gFileToken[16];   // what TPFile_Open answers: the "file object"
// malloc blocks the case holds and their sizes (an array: a map's nodes would show in the heap traffic)
struct Block {
    const void* mAddress;
    size_t mSize;
};
Block gBlocks[64];
void BlockAdd(const void* address, size_t size) {
    for (Block& block : gBlocks) {
        if (block.mAddress == nullptr) {
            block = Block{address, size};
            return;
        }
    }
}
void BlockRemove(const void* address) {
    for (Block& block : gBlocks) {
        if (block.mAddress == address) block.mAddress = nullptr;
    }
}
AdvisorLine gPristineLines[611];   // the response table as the build has it (a case may patch rows)
bool gPristineTaken = false;
}  // namespace
CMapWho* gei_MapWho = reinterpret_cast<CMapWho*>(gMapStorage);

extern "C" {
int32_t tpw_ad_clock_now;          // CClock::ReadFixed's answer
int32_t tpw_ad_clock_paused;       // CClock::GetPauseFlag's answer
}

// ---- fakes: the clock, the engine, the map
int32_t CClock::ReadFixed() {
    ev("ReadFixed", {lab(this)});
    return tpw_ad_clock_now;
}
int32_t CClock::GetPauseFlag() {
    ev("GetPauseFlag", {lab(this)});
    return tpw_ad_clock_paused;
}
void CSystem::UpdateLights() { ev("UpdateLights", {lab(this)}); }
int32_t CMapWho::Link(CMapLinkBase* link, int32_t list) {
    ev("Link", {lab(this), lab(link), list});
    return 1;
}
int32_t CMapWho::Unlink(CMapLinkBase* link, int32_t list) {
    ev("Unlink", {lab(this), lab(link), list});
    return 1;
}

// ---- fakes: the ride set loader and its callees
int32_t SetupDirectoryForMeshLoad(char* file, char* folder, char* sharedTextureFolder) {
    ev("SetupDirectory", {}, {file, folder, sharedTextureFolder});
    return 1;
}
// Writes the set the case queued ("LoadSet") into *output, unless the case makes the load fail ("LoadRideAnimSet": 0).
int32_t LoadRideAnimSet(char* name, uint32_t flags, tag_sRideAnimSet** output, char* loadedName, uint32_t timeCount) {
    ev("LoadRideAnimSet", {flags, lab(output), lab(loadedName), timeCount}, {name});
    const int32_t result = static_cast<int32_t>(ret("LoadRideAnimSet", 1));
    if (result != 0) *output = reinterpret_cast<tag_sRideAnimSet*>(static_cast<intptr_t>(ret("LoadSet")));
    return result;
}
int32_t InstanceRideAnimSet(tag_sRideAnimSet* set, sHeapInfo* heap, uint32_t flags, tag_sRideAnimSet** output,
                            uint32_t timeCount) {
    ev("InstanceRideAnimSet", {lab(set), lab(heap), flags, lab(output), timeCount});
    if (set == nullptr) return 0;
    const int32_t result = static_cast<int32_t>(ret("InstanceRideAnimSet", 1));
    if (result != 0) *output = reinterpret_cast<tag_sRideAnimSet*>(static_cast<intptr_t>(ret("InstanceSet")));
    return result;
}
int32_t InitRideAnimSet(tag_sRideAnimSet* set, int32_t init, int32_t mode, int32_t keepNodes) {
    ev("InitRideAnimSet", {lab(set), init, mode, keepNodes});
    return 1;
}
void DestroyRideAnimSetInstance(tag_sRideAnimSet* set, sHeapInfo* heap, uint32_t flags) {
    ev("DestroyRideAnimSetInstance", {lab(set), lab(heap), flags});
}
void DestroyRideAnimSet(tag_sRideAnimSet* set, int32_t mode) { ev("DestroyRideAnimSet", {lab(set), mode}); }

// ---- fakes: the mesh draw switches
int32_t IsMeshDrawEnabled(tag_sMesh* mesh) {
    ev("IsMeshDrawEnabled", {lab(mesh)});
    return static_cast<int32_t>(ret("IsMeshDrawEnabled", 1));
}
void DisableMeshDraw(tag_sMesh* mesh) { ev("DisableMeshDraw", {lab(mesh)}); }
void DisableMeshDraw(tag_sMesh* mesh, HFieldXZ from, HFieldXZ to) {
    ev("DisableMeshDrawRange", {lab(mesh), (static_cast<uint32_t>(from.mF00) << 16) | from.mF02,
                                (static_cast<uint32_t>(to.mF00) << 16) | to.mF02});
}

// ---- fakes: the animation changes and the tick (the model in the file comment)
namespace {
void QueueAnimation(tag_sRideAnimSet* set, uint32_t kind, uint32_t anim) {
    if (set == nullptr || set->mTimeControls == nullptr || set->mTimeCount == 0) return;
    sAnimTimeControl* control = static_cast<sAnimTimeControl*>(set->mTimeControls);
    control->mF24 = kind;
    control->mF28 = anim;
    gAnimLeft = kind == 0xc ? 0 : static_cast<int32_t>(anim & 3) + 1;
}
}  // namespace
void SetCurRideAnim(tag_sRideAnimSet* set, uint32_t a, uint32_t anim, uint32_t loop, float speed, uint32_t b) {
    ev("SetCurRideAnim", {lab(set), a, anim, loop, fbits(speed), b});
    ret("SetCurRideAnim");
    QueueAnimation(set, a, anim);
}
uint32_t SetCurRideAnimDuration(tag_sRideAnimSet* set, uint32_t a, uint32_t anim, uint32_t loop, float speed, uint32_t b) {
    ev("SetCurRideAnim", {lab(set), a, anim, loop, fbits(speed), b});
    const uint32_t result = static_cast<uint32_t>(ret("SetCurRideAnim"));
    QueueAnimation(set, a, anim);
    return result;
}
void AnimateRideSet(tag_sRideAnimSet* set, int32_t a, uint32_t flags) {
    ev("AnimateRideSet", {lab(set), a, flags});
    if (gAnimLeft > 0 && --gAnimLeft == 0 && set != nullptr && set->mTimeControls != nullptr && set->mTimeCount != 0)
        static_cast<sAnimTimeControl*>(set->mTimeControls)->mF24 = 0xc;
}

// ---- fakes: the sound, the effects, the particles, the interface
int32_t SOUND_SendEvent(int32_t entity, int32_t bank, int32_t event, int32_t a, int32_t b, int32_t c) {
    ev("SOUND_SendEvent", {entity, bank, event, a, b, c});
    return static_cast<int32_t>(ret("SOUND_SendEvent", 1));
}
void SOUND_PauseSound(int32_t handle) { ev("SOUND_PauseSound", {handle}); }
void SOUND_UnpauseSound(int32_t handle) { ev("SOUND_UnpauseSound", {handle}); }
int32_t SOUND_MoveSoundEffect(int32_t handle, int32_t a, int32_t b, int32_t c) {
    ev("SOUND_MoveSoundEffect", {handle, a, b, c});
    return static_cast<int32_t>(ret("SOUND_MoveSoundEffect", handle));
}
void EM_Init() { ev("EM_Init"); }
int32_t EM_SetEmitersOn(int32_t effect, int32_t count, int32_t emitter) {
    ev("EM_SetEmitersOn", {effect, count, emitter});
    return static_cast<int32_t>(ret("EM_SetEmitersOn", 1));
}
void EM_SetEffectTranslation(int32_t effect, int32_t index, float x, float y, float z) {
    ev("EM_SetEffectTranslation", {effect, index, fbits(x), fbits(y), fbits(z)});
}
void EM_SetEffectOrbit(int32_t effect, int32_t index, float a, float b, float c) {
    ev("EM_SetEffectOrbit", {effect, index, fbits(a), fbits(b), fbits(c)});
}
void EM_Tick() { ev("EM_Tick"); }
uint32_t PAR_InitEmitter(int32_t type, int32_t a, int32_t b, int32_t c) {
    ev("PAR_InitEmitter", {type, a, b, c});
    return 7;
}
void UpdateIslandLobbyKeyDisplay() { ev("UpdateIslandLobbyKeyDisplay"); }

// ---- fakes: the sound handles the advisor stops, fades, mutes and asks the length of
void SOUND_SetVolumeOnSound(int32_t handle, int32_t volume) { ev("SOUND_SetVolumeOnSound", {handle, volume}); }
void SOUND_StopSoundEffect(int32_t handle) { ev("SOUND_StopSoundEffect", {handle}); }
void SOUND_FadeSoundEffect(int32_t handle) { ev("SOUND_FadeSoundEffect", {handle}); }
int32_t SOUND_GetCurrentSampleLengthInMilliseconds(int32_t handle) {
    ev("SOUND_GetCurrentSampleLengthInMilliseconds", {handle});
    return static_cast<int32_t>(ret("SOUND_GetCurrentSampleLengthInMilliseconds", 0));
}

// ---- fakes: the lip-sync file
// The path hook answers "Found:<name>" in a buffer of its own, or NULL (the case's queue "FindFilePath", default found);
// the length of the name is recorded too because the text is cut at 159 characters.
char* FindFilePath(const char* name, uint32_t flags, int32_t* result) {
    ev("FindFilePath", {flags, lab(result), static_cast<int64_t>(std::strlen(name))}, {name});
    if (ret("FindFilePath", 1) == 0) return nullptr;
    std::snprintf(gFound, sizeof gFound, "Found:%s", name);
    return gFound;
}
// Answers the file object (fixed label 6) or NULL (the queue "TPFile_Open", default opened).
bfl::FileSystem::CFile* TPFile_Open(const char* name, const char* mode) {
    ev("TPFile_Open", {name == nullptr ? -1 : static_cast<int64_t>(std::strlen(name))}, {name, mode});
    if (ret("TPFile_Open", 1) == 0) return nullptr;
    return reinterpret_cast<bfl::FileSystem::CFile*>(gFileToken);
}
char* get_theme_path() {
    ev("get_theme_path");
    return const_cast<char*>(gTheme.c_str());
}
int32_t LbFile_GetSize(void* file) {
    ev("LbFile_GetSize", {lab(file)});
    return static_cast<int32_t>(ret("LbFile_GetSize", 0));
}
// Copies as much of the case's file as was asked for (and exists); the rest of the buffer is left as it was.
TbError LbFile_Read(void* file, void* buffer, uint32_t length, uint32_t* transferred) {
    ev("LbFile_Read", {lab(file), lab(buffer), length, lab(transferred)});
    const size_t count = length < gFileBytes.size() ? length : gFileBytes.size();
    if (buffer != nullptr && count != 0) std::memcpy(buffer, gFileBytes.data(), count);
    return TbError(0);
}
TbError LbFile_Close(void* file) {
    ev("LbFile_Close", {lab(file)});
    return TbError(0);
}
// The original swaps `count` words of the data it was given the BYTE count of; the host's does nothing, and so does this.
void SamsUtilities::_USwapBlock32(uint32_t* words, int32_t count) { ev("_USwapBlock32", {lab(words), count}); }

// ---- fakes: the runtime
// The trace records its level and, for a format with %d, the int it was given.
void CTRACE(uint32_t level, const char* format, ...) {
    int64_t value = 0;
    if (format != nullptr && std::strstr(format, "%d") != nullptr) {
        va_list args;
        va_start(args, format);
        value = va_arg(args, int);
        va_end(args);
    }
    ev("CTRACE", {static_cast<int64_t>(level), value}, {format});
}
int32_t NS_MacDoze::_stricmp(const char* a, const char* b) {
    ev("_stricmp", {}, {a, b});
    for (;; ++a, ++b) {
        const int ca = (*a >= 'A' && *a <= 'Z') ? *a + 32 : static_cast<unsigned char>(*a);
        const int cb = (*b >= 'A' && *b <= 'Z') ? *b + 32 : static_cast<unsigned char>(*b);
        if (ca != cb) return ca < cb ? -1 : 1;
        if (ca == 0) return 0;
    }
}
// MSL's generator (include/platform/msl.h), or the next value the case queued.
int32_t msl_rand() {
    int32_t value;
    const int64_t queued = ret("Rand", -1);
    if (queued >= 0) {
        value = static_cast<int32_t>(queued);
    } else {
        gRandState = gRandState * 0x41c64e6du + 0x3039u;
        value = static_cast<int32_t>((gRandState >> 16) & 0x7fff);
    }
    ev("rand", {value});
    return value;
}
// free: records the blocks the test has labelled (the lip-sync data), then frees them.
extern "C" void free(void* block) {
    static void (*real)(void*) = reinterpret_cast<void (*)(void*)>(dlsym(RTLD_NEXT, "free"));
    if (block != nullptr && on() && lab(block) > 0) {
        ev("free", {lab(block)});
        forget(block);
        BlockRemove(block);
    }
    real(block);
}
// malloc: while recording, a block gets the next label, is filled with 0xa5 (as the PowerPC side's heap is, 16 bytes more) and its
// size is kept for the dump; a size over a megabyte fails (the PowerPC side's does too).
extern "C" void* malloc(size_t size) {
    static void* (*real)(size_t) = reinterpret_cast<void* (*)(size_t)>(dlsym(RTLD_NEXT, "malloc"));
    static bool inside = false;     // operator new (tests/calllog.cpp) calls malloc for the map's nodes below
    if (!on() || inside) return real(size);
    inside = true;
    ev("malloc", {static_cast<int64_t>(size)});
    void* block = nullptr;
    if (size <= (1u << 20)) {
        block = real(size + 16);       // 16 spare bytes, also 0xa5: a read just past the block sees the same on both sides
        std::memset(block, 0xa5, size + 16);
        newlab(block);
        BlockAdd(block, size);
    }
    inside = false;
    return block;
}

// ---- the driver
namespace {
std::string gLayout;
void* gArenaBlocks[64];
int gArenaCount = 0;

template <class T>
void Add(const char* name, T value) {
    gLayout += name;
    gLayout += '=';
    gLayout += std::to_string(static_cast<long long>(value));
    gLayout += ';';
}
#define OFF(struct_, name, member) Add(name, offsetof(struct_, member))
float FromBits(int64_t bits) {
    const uint32_t word = static_cast<uint32_t>(bits);
    float value;
    std::memcpy(&value, &word, 4);
    return value;
}
}  // namespace

extern "C" {

// The arena blocks the case made go back at the next begin (the C heap ones are the code's to free, or leak).
void tpw_ad_begin(uint32_t seed) {
    for (int i = 0; i < gArenaCount; ++i) tpw::ArenaFree(gArenaBlocks[i]);
    gArenaCount = 0;
    for (sAdvisorStore& slot : gAdvisorStore) {
        std::memset(static_cast<void*>(&slot), 0, sizeof slot);
        new (&slot) sAdvisorStore;
    }
    std::memset(static_cast<void*>(&gAdvisorState), 0, sizeof gAdvisorState);
    gAdvisorStaticInUse = 0;
    gAdvisorFile = nullptr;
    gTheme.clear();
    gFileBytes.clear();
    std::memset(gBlocks, 0, sizeof gBlocks);
    if (!gPristineTaken) {
        std::memcpy(gPristineLines, gAdvisorLines, sizeof gAdvisorLines);
        gPristineTaken = true;
    }
    std::memcpy(gAdvisorLines, gPristineLines, sizeof gAdvisorLines);
    std::memset(gAdvisorStaticBuffer, 0, sizeof gAdvisorStaticBuffer);
    std::memset(gSoundBanks, 0, sizeof gSoundBanks);
    gAdvisorThemeSoundBank = 0;
    gPlayerSettings.mCurrent.mAdvisorOn = true;
    std::memset(&gSoundSettings, 0, sizeof gSoundSettings);
    tpw_ad_clock_now = 0;
    tpw_ad_clock_paused = 0;
    gAnimLeft = 0;
    gRandState = seed;
}

// Zeroed blocks, from the arena (file images: Ptr32 fields lead to them) or the C heap.
void* tpw_ad_alloc(int32_t size, int32_t arena) {
    void* block;
    if (arena != 0) {
        block = tpw::ArenaAllocZeroed(static_cast<size_t>(size));
        gArenaBlocks[gArenaCount++] = block;
    } else {
        block = std::calloc(1, static_cast<size_t>(size));
    }
    return block;
}
int64_t tpw_ad_arena_base() { return reinterpret_cast<intptr_t>(tpw::ArenaBase()); }
void* tpw_ad_state() { return &gAdvisorState; }
void* tpw_ad_slot(int32_t s) { return &gAdvisorStore[s]; }
void* tpw_ad_linkbase(int32_t s) { return static_cast<CMapLinkBase*>(&gAdvisorStore[s].mLight); }
void* tpw_ad_buffer() { return gAdvisorStaticBuffer; }
void* tpw_ad_in_use() { return &gAdvisorStaticInUse; }
void* tpw_ad_system() { return &gei_System; }
void* tpw_ad_map() { return gei_MapWho; }
void* tpw_ad_clock() { return &gClock; }
void* tpw_ad_banks() { return gSoundBanks; }
void* tpw_ad_theme_bank() { return &gAdvisorThemeSoundBank; }
void* tpw_ad_advisor_on() { return &gPlayerSettings.mCurrent.mAdvisorOn; }
void* tpw_ad_sound_settings() { return &gSoundSettings; }
void* tpw_ad_canned() { return const_cast<uint32_t*>(&gCannedSequences[0][0]); }
void* tpw_ad_lines() { return gAdvisorLines; }
void* tpw_ad_file_global() { return &gAdvisorFile; }
void* tpw_ad_file_token() { return gFileToken; }
// The case's world for the speech: the theme path text, the lip-sync file's bytes, and a patched row of the table.
void tpw_ad_set_theme(const char* text) { gTheme = text; }
void tpw_ad_set_file(const uint8_t* bytes, int32_t count) { gFileBytes.assign(reinterpret_cast<const char*>(bytes), static_cast<size_t>(count)); }
void tpw_ad_set_row(int32_t index, const int32_t* words) { std::memcpy(&gAdvisorLines[index], words, sizeof(AdvisorLine)); }
int64_t tpw_ad_block_size(const void* block) {
    for (const Block& entry : gBlocks) {
        if (entry.mAddress == block) return static_cast<int64_t>(entry.mSize);
    }
    return -1;
}

// Where every field the test reads or writes lies in the host's structures.
const char* tpw_ad_layout() {
    if (!gLayout.empty()) return gLayout.c_str();
#define S(name) OFF(SAdvisorState, "state." #name, name)
    S(mFlags); S(mBaseTime); S(mEndTime); S(mSpeaking); S(mMouthOpen); S(mWhich); S(mSoundSet); S(mSound);
    S(mSoundHandle); S(mHat); S(mHand); S(mDressAfterAnim); S(mDressNow); S(mSoundTime); S(mHeadNode); S(mMouthNodes);
    S(mAnimLengths); S(mSequence); S(mSequencePos); S(mAnimIndex); S(mMouthCount); S(mMouthTime); S(mKeyTime);
    S(mClockPaused); S(mPauseStart); S(mPausedTotal); S(mMouthWanted); S(mMouthShown); S(mMouthChange); S(mSyncSize);
    S(mSyncPos); S(mSyncActive); S(mSyncData); S(mAccessoriesOn); S(mAccessoriesOff); S(mPausedSound);
#undef S
    Add("state.size", sizeof(SAdvisorState));
    OFF(sAdvisorStore, "slot.mSet", mSet);
    OFF(sAdvisorStore, "slot.mInstance", mInstance);
    OFF(sAdvisorStore, "slot.mMatrix", mMatrix);
    Add("slot.size", sizeof(sAdvisorStore));
#define L(name) Add("light." #name, offsetof(sAdvisorStore, mLight) + offsetof(CLight, name))
    L(mColor); L(mPosition); L(mF20); L(mNormal); L(mData); L(mRadius); L(mRadiusSquared); L(mInverseRadiusSquared);
    L(mFlags); L(mColorWord); L(mNext);
#undef L
    OFF(tag_sRideAnimSet, "set.mFlags", mFlags);
    OFF(tag_sRideAnimSet, "set.mHeads", mHeads);
    OFF(tag_sRideAnimSet, "set.mTimeCount", mTimeCount);
    OFF(tag_sRideAnimSet, "set.mTimeControls", mTimeControls);
    OFF(tag_sRideAnimSet, "set.mGroups", mGroups);
    OFF(tag_sRideAnimSet, "set.mFC0", mFC0);
    OFF(tag_sRideAnimSet, "set.mFC4", mFC4);
    OFF(tag_sRideAnimSet, "set.mMatrix", mMatrix);
    Add("set.size", sizeof(tag_sRideAnimSet));
    OFF(sRideAnimGroup, "group.mFlags", mFlags);
    OFF(sRideAnimGroup, "group.mCount", mCount);
    OFF(sRideAnimGroup, "group.mEntries", mEntries);
    Add("group.size", sizeof(sRideAnimGroup));
    OFF(sRideAnimEntry, "entry.mIndex", mIndex);
    OFF(sRideAnimEntry, "entry.mAnimation", mAnimation);
    Add("entry.size", sizeof(sRideAnimEntry));
    OFF(tag_sMesh, "mesh.mF00", mF00);
    OFF(tag_sMesh, "mesh.mHeader", mHeader);
    Add("mesh.size", sizeof(tag_sMesh));
    OFF(sAnimTimeControl, "control.mF24", mF24);
    OFF(sAnimTimeControl, "control.mF28", mF28);
    Add("control.size", sizeof(sAnimTimeControl));
    OFF(SSoundSettings, "sound.mChanged", mChanged);
    OFF(SSoundSettings, "sound.mF2C", mDuck);   // (the key keeps the old member name)
    Add("pointer.size", sizeof(void*));
    return gLayout.c_str();
}

// Puts a slot's storage together as the static initialiser does (over whatever the storage held).
void tpw_ad_construct(void* storage) { new (storage) sAdvisorStore; }

// The functions under test, by number (tests/test_Advisor.py has the same list). Arguments are integers and addresses.
int64_t tpw_ad_op(int32_t op, const int64_t* a) {
    auto set = [&](int i) { return reinterpret_cast<tag_sRideAnimSet*>(static_cast<intptr_t>(a[i])); };
    auto text = [&](int i) { return reinterpret_cast<char*>(static_cast<intptr_t>(a[i])); };
    switch (op) {
    case 1: ADV_Init(); return 0;
    case 2: ADV_Close(); return 0;
    case 3: ADV_Pause(); return 0;
    case 4: ADV_UnPause(); return 0;
    case 5: ADV_Tick(); return 0;
    case 6: ADV_Tick2(); return 0;
    case 7: TickAdvisor(static_cast<uint32_t>(a[0])); return 0;
    case 8: DressAdvisor(); return 0;
    case 9: SetAdvisorHatHand(static_cast<int32_t>(a[0])); return 0;
    case 10: return GenerateAnimSequence(static_cast<int32_t>(a[0]));
    case 11: return SetCannedSequence(static_cast<int32_t>(a[0]), static_cast<int32_t>(a[1]));
    case 12: LoadAdvisor(text(0), text(1), text(2), static_cast<uint32_t>(a[3])); return 0;
    case 13: DestroyAdvisor(static_cast<uint32_t>(a[0])); return 0;
    case 14: DisableAdvisorDisplay(static_cast<uint32_t>(a[0])); return 0;
    case 15: return StartAdvisorAnimation(static_cast<uint32_t>(a[0]), static_cast<uint32_t>(a[1]));
    case 16: EnableAdvisorAccessory(static_cast<uint32_t>(a[0]), static_cast<uint32_t>(a[1])); return 0;
    case 17: DisableAdvisorAccessory(static_cast<uint32_t>(a[0]), static_cast<uint32_t>(a[1])); return 0;
    case 18: return lab(GetAdvisorRideAnim(static_cast<uint32_t>(a[0])));
    case 19: SND_DuckVolume(static_cast<int32_t>(a[0])); return 0;
    case 20: return FindDataNodeIndexByID(set(0), static_cast<uint32_t>(a[1]), static_cast<uint32_t>(a[2]));
    case 21: return FindDataNodeIndexByIDAdvisorTempHack(set(0), static_cast<uint32_t>(a[1]), static_cast<uint32_t>(a[2]));
    case 22: SetMeshInUISpace(*reinterpret_cast<tag_sMesh**>(static_cast<intptr_t>(a[0]))); return 0;
    case 23: {
        uint32_t kind = 0xdeadbeefu;
        uint32_t index = 0xdeadbeefu;
        GetDeferredAnim(set(0), (a[2] & 1) != 0 ? &kind : nullptr, (a[2] & 2) != 0 ? &index : nullptr,
                        static_cast<uint32_t>(a[1]));
        return (static_cast<int64_t>(index) << 32) | kind;
    }
    case 24: return GetAnimLength(set(0), static_cast<uint32_t>(a[1]), static_cast<uint32_t>(a[2]));
    case 26: return ADV_Call(AdvisorResponse{static_cast<int32_t>(a[0])});
    case 27: ADV_Slap(); return 0;
    case 28: ADV_Kill(static_cast<int32_t>(a[0])); return 0;
    case 29: return ADV_GetTag(AdvisorResponse{static_cast<int32_t>(a[0])});
    case 30: return ADV_Stat();
    case 25: {
        s3DPoint scale = {FromBits(a[1]), FromBits(a[2]), FromBits(a[3])};
        ScaleLMatrixNoTrans(reinterpret_cast<sMatrix*>(static_cast<intptr_t>(a[0])), &scale);
        return 0;
    }
    }
    return -99;
}

}  // extern "C"
