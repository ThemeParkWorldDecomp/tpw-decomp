// Test library for the coasters' ticks (unit w137; libtpw_unit_CoasterTicks.dylib, tests/test_CoasterTicks.py), main app:
//   src/game/rides/CoasterEdit.cpp, src/game/rides/TPCoaster.cpp   CoasterTickPreRender, CoasterTickPostRender, CoasterUserInputTick
//
// Every callee is a fake that records its arguments (floats as bits, objects as labels) and the live state the order of the calls
// shows (the ticks: gCoasterCurrent and the instance's flags). The scripted answers (the cost, the rounded cost, the brace angle) come
// from the case's queues. The module builds the world from a description of numbers (tpw_ct_ticks and tpw_ct_input) and the dump of
// numbers it answers is compared with the PowerPC side's.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include "ActionRecorder.h"
#include "TPCamera.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "calllog.h"
#include "m3d2.h"
#include "rsse.h"

using namespace calllog;

// ---- the strings of gTPGlobals are never read
template<> void TbDynamicStringTemplate<char>::Format(const char*, ...) {}
template<> void TbDynamicStringTemplate<char16_t>::Format(const char16_t*, ...) {}
template<> void TbStaticStringTemplate<char>::Format(const char*, ...) {}

// ---- globals of other files, defined here
TPGlobals::TPGlobals() {}
TPGlobals::~TPGlobals() {}
TPGlobals gTPGlobals;
CActionRecorder gActionRecorder;
tag_sCoasterInstance* gCoasterInstances;
float gCameraF5BF4;
float gPointerScreenXY[2];
s3DPoint gPointerWorldPosn;
tag_sRideAnimSet* gPointedRide;

namespace {

// Labels of the objects of a case.
enum Label : int32_t { kInstance = 11 /* 4 */, kEdit = 21, kBrace = 22, kRecorder = 99, kRide = 31 /* 3 */, kTarget = 41 /* 4 */ };

int64_t Bits(float value) { return fbits(value); }
float FromBits(int64_t bits) {
    const uint32_t word = static_cast<uint32_t>(bits);
    float value;
    std::memcpy(&value, &word, sizeof value);
    return value;
}

// Where the train is within its instance's block (the stride is 0x80 on both sides).
int64_t TrainIndex(const tag_sCoasterInstance* instance, const sTrainInstance* train) {
    return train - instance->mTrains;
}

// The state the order of the ticks' calls shows: the instance (label), gCoasterCurrent (label) and the instance's flags.
void TickEvent(const char* name, tag_sCoasterInstance* instance, int64_t extra = -1) {
    ev(name, {lab(instance), lab(gCoasterCurrent), instance->mFlags, extra});
}

}  // namespace

// ---- the fakes
void LinkPreviewBrace(tag_sCoasterInstance* instance) { TickEvent("LinkPreviewBrace", instance); }
void UnLinkPreviewBrace(tag_sCoasterInstance* instance) { TickEvent("UnLinkPreviewBrace", instance); }
void CoasterUVAnimTick(tag_sCoasterInstance* instance) { TickEvent("CoasterUVAnimTick", instance); }
void TickTrainSounds(tag_sCoasterInstance* instance, sTrainInstance* train) { TickEvent("TickTrainSounds", instance, TrainIndex(instance, train)); }
void HandleTrainLoading(tag_sCoasterInstance* instance, sTrainInstance* train) { TickEvent("HandleTrainLoading", instance, TrainIndex(instance, train)); }
void CoasterCarRideTrackTick(tag_sCoasterInstance* instance) { TickEvent("CoasterCarRideTrackTick", instance); }
void GenerateTrack(tag_sCoasterInstance* instance) { TickEvent("GenerateTrack", instance); }
void CActionRecorder::RecordAction(int32_t action, uint16_t value) {
    ev("RecordAction", {lab(this), action, value, lab(gCoasterCurrent), gCoasterCurrent ? static_cast<int64_t>(gCoasterCurrent->mFlags) : int64_t{-1}});
}
void FlagGenerateTrack(tag_sCoasterInstance* instance, int32_t flag) { ev("FlagGenerateTrack", {lab(instance), flag}); }
int32_t RoundUp(float value) {
    ev("RoundUp", {Bits(value)});
    return static_cast<int32_t>(ret("RoundUp", 0));
}
void HandleCoasterDataChange(uint32_t flags, tag_sTrackBrace* brace, tag_sCoasterInstance* instance, int32_t a, int32_t b) {
    ev("HandleCoasterDataChange", {flags, lab(brace), lab(instance), a, b});
}
void HandleMultiBracePreviewChange(tag_sCoasterInstance* instance, tag_sRideAnimSet* previous, tag_sRideAnimSet* pointed, int32_t changed,
                                   uint32_t cellX, uint32_t cellZ) {
    ev("HandleMultiBracePreviewChange", {lab(instance), lab(previous), lab(pointed), changed, cellX, cellZ});
}
float CalculateBraceCost(tag_sCoasterInstance* instance, int32_t a, tag_sTrackBrace* brace, float* b, float* c, float* d, float* e) {
    ev("CalculateBraceCost", {lab(instance), a, lab(brace), lab(b), lab(c), lab(d), lab(e)});
    return FromBits(ret("Cost", 0));
}
void LLCC_SystemAddToSessionCost(int32_t cost) { ev("LLCC_SystemAddToSessionCost", {cost}); }
float GetVectorRotationAroundYAxisFromZ(s3DPoint& point) {
    ev("GetVectorRotationAroundYAxisFromZ", {lab(&point), Bits(point.x), Bits(point.y), Bits(point.z)});
    return FromBits(ret("Rot", 0));
}

namespace {

struct Reader {
    const int64_t* p;
    int64_t next() { return *p++; }
    float nextf() { return FromBits(*p++); }
};

struct Out {
    int64_t* p;
    int32_t n = 0;
    void put(int64_t v) { p[n++] = v; }
    void putf(float v) { p[n++] = Bits(v); }
};

// The index (1 to 4) of the instance a pointer is, or 0 for null, or -1.
int64_t Index(tag_sCoasterInstance* const* all, int32_t count, const void* pointer) {
    if (pointer == nullptr) return 0;
    for (int32_t i = 0; i < count; ++i)
        if (pointer == all[i]) return static_cast<int64_t>(i) + 1;
    return -1;
}

uint32_t gTypeWords[4];
sTrainInstance gTrainBlocks[4][3];

}  // namespace

extern "C" {

// The sine table (4,096 words of floats), once per library.
void tpw_ct_table(const uint32_t* words) { std::memcpy(gTPGlobals.mSinTable, words, sizeof gTPGlobals.mSinTable); }

// One of the two ticks. The description: op (0 pre, 1 post), the number of instances n (0 to 4), the edit instance (0 none, 1 to n one of
// them, 5 another one), the start of gCoasterCurrent (0 none, 1 to n), gCoasterCurrentData (a word that is none of the blocks), the
// word 0x102b7310, the actionless recorder; then per instance: flags, state, the type's word, the id, the number of trains and the first
// words of three trains.
int32_t tpw_ct_ticks(const int64_t* description, int64_t* out) {
    Reader in{description};
    const int64_t op = in.next();
    const int32_t n = static_cast<int32_t>(in.next());
    const int64_t editChoice = in.next();
    const int64_t currentChoice = in.next();
    const int64_t dataWord = in.next();
    const int64_t word7310 = in.next();
    static tag_sCoasterInstance other;
    static tag_sCoasterInstance* all[4];
    static tag_sCoasterInstance store[4];
    std::memset(&other, 0, sizeof other);
    std::memset(store, 0, sizeof store);
    for (int32_t i = 0; i < n; ++i) {
        tag_sCoasterInstance& c = store[i];
        c.mFlags = static_cast<uint32_t>(in.next());
        c.mState = static_cast<uint32_t>(in.next());
        gTypeWords[i] = static_cast<uint32_t>(in.next());
        c.mType = &gTypeWords[i];
        c.mObjectInstanceID = static_cast<uint32_t>(in.next());
        c.mTrainCount = static_cast<int32_t>(in.next());
        c.mTrains = gTrainBlocks[i];
        std::memset(gTrainBlocks[i], 0, sizeof gTrainBlocks[i]);
        for (int32_t t = 0; t < 3; ++t) {
            const uint32_t word = static_cast<uint32_t>(in.next());
            gTrainBlocks[i][t].mFlags = word;
        }
        c.mF40 = 1;
        all[i] = &c;
        tpw_cl_label(kInstance + i, &c);
    }
    for (int32_t i = 0; i + 1 < n; ++i) store[i].mNext = &store[i + 1];
    gCoasterInstances = n > 0 ? &store[0] : nullptr;
    tpw_cl_label(kEdit, &other);
    tpw_cl_label(kRecorder, &gActionRecorder);
    gCoasterEditInstance = editChoice == 0 ? nullptr : editChoice == 5 ? &other : all[editChoice - 1];
    gCoasterCurrent = currentChoice == 0 ? nullptr : all[currentChoice - 1];
    gCoasterCurrentData = reinterpret_cast<void*>(static_cast<uintptr_t>(dataWord));
    gCoasterWord7310 = static_cast<uint32_t>(word7310);
    if (op == 0) {
        CoasterTickPreRender();
    } else {
        CoasterTickPostRender();
    }
    Out o{out};
    for (int32_t i = 0; i < n; ++i) o.put(store[i].mFlags);
    o.put(Index(all, n, gCoasterCurrent));
    int64_t data = -1;
    for (int32_t i = 0; i < n; ++i)
        if (gCoasterCurrentData == &all[i]->mBlock3C) data = static_cast<int64_t>(i) + 1;
    o.put(gCoasterCurrentData == reinterpret_cast<void*>(static_cast<uintptr_t>(dataWord)) ? -2 : data);
    o.put(gCoasterWord7310);
    o.put(Index(all, n, gCoasterEditInstance));
    return o.n;
}

// The input tick. The description is read in the order of the module's `input_words`.
int32_t tpw_ct_input(const int64_t* description, int64_t* out) {
    Reader in{description};
    static tag_sCoasterInstance edit;
    static tag_sTrackBrace brace;
    static sTrackBraceData braceData;
    static uint32_t rideDummies[3];
    static float targets[4][3];
    std::memset(&edit, 0, sizeof edit);
    std::memset(&brace, 0, sizeof brace);
    const int64_t editMode = in.next();
    gCoasterInputFlags = static_cast<uint32_t>(in.next());
    gCoasterDragAnchor[0] = in.nextf();
    gCoasterDragAnchor[1] = in.nextf();
    gCoasterDragScaleX = in.nextf();
    gCoasterDragScaleY = in.nextf();
    gCoasterDragLastDX = in.nextf();
    gCoasterDragLastDY = in.nextf();
    gPointerScreenXY[0] = in.nextf();
    gPointerScreenXY[1] = in.nextf();
    gCameraF5BF4 = in.nextf();
    const int64_t braceMode = in.next();
    brace.mAngle = in.nextf();
    braceData.mF00 = 0;
    braceData.mFlags = static_cast<uint32_t>(in.next());
    brace.mData = &braceData;
    gCoasterVerticalBase1 = in.nextf();
    gCoasterVerticalMin1 = in.nextf();
    gCoasterVerticalMax1 = in.nextf();
    gCoasterVerticalBase2 = in.nextf();
    gCoasterVerticalMin2 = in.nextf();
    gCoasterVerticalMax2 = in.nextf();
    gCoasterHorizontalBase1 = in.nextf();
    gCoasterHorizontalMin1 = in.nextf();
    gCoasterHorizontalMax1 = in.nextf();
    gCoasterHorizontalBase2 = in.nextf();
    gCoasterHorizontalMin2 = in.nextf();
    gCoasterHorizontalMax2 = in.nextf();
    float* saved[4] = {gCoasterSavedVertical1, gCoasterSavedVertical2, gCoasterSavedHorizontal1, gCoasterSavedHorizontal2};
    for (float* s : saved)
        for (int32_t k = 0; k < 3; ++k) s[k] = in.nextf();
    float** pointers[4] = {&gCoasterVerticalTarget1, &gCoasterVerticalTarget2, &gCoasterHorizontalTarget1, &gCoasterHorizontalTarget2};
    bool present[4];
    for (int32_t t = 0; t < 4; ++t) {
        present[t] = in.next() != 0;
        for (int32_t k = 0; k < 3; ++k) targets[t][k] = in.nextf();
        *pointers[t] = present[t] ? targets[t] : nullptr;
        if (present[t]) tpw_cl_label(kTarget + t, targets[t]);
    }
    gCoasterCostBefore = in.nextf();
    gCoasterGridCellX = static_cast<int32_t>(in.next());
    gCoasterGridCellZ = static_cast<int32_t>(in.next());
    const int64_t previous = in.next();
    const int64_t pointed = in.next();
    gPointerWorldPosn.x = in.nextf();
    gPointerWorldPosn.y = in.nextf();
    gPointerWorldPosn.z = in.nextf();
    gActionRecorder.mRecording = static_cast<int32_t>(in.next());
    gCoasterWord16E0 = static_cast<uint32_t>(in.next());
    gTPGlobals.mSinTableMask = static_cast<uint32_t>(in.next());
    gTPGlobals.mSinTableScale = in.nextf();
    gTPGlobals.mSinTableSize = 0x1000;
    for (int32_t i = 0; i < 3; ++i) tpw_cl_label(kRide + i, &rideDummies[i]);
    gCoasterPreviousPointedRide = previous == 0 ? nullptr : reinterpret_cast<tag_sRideAnimSet*>(&rideDummies[previous - 1]);
    gPointedRide = pointed == 0 ? nullptr : reinterpret_cast<tag_sRideAnimSet*>(&rideDummies[pointed - 1]);
    gCoasterDragBrace = braceMode == 0 ? nullptr : &brace;
    if (braceMode != 0) tpw_cl_label(kBrace, &brace);
    edit.mF40 = editMode == 2 ? 0x55 : 0;
    tpw_cl_label(kEdit, &edit);
    gCoasterEditInstance = editMode == 0 ? nullptr : &edit;
    CoasterUserInputTick();
    Out o{out};
    o.put(gCoasterInputFlags);
    o.putf(gCoasterDragAnchor[0]);
    o.putf(gCoasterDragAnchor[1]);
    o.putf(gCoasterDragLastDX);
    o.putf(gCoasterDragLastDY);
    for (float* s : saved)
        for (int32_t k = 0; k < 3; ++k) o.putf(s[k]);
    for (int32_t t = 0; t < 4; ++t) {
        o.put(*pointers[t] == nullptr ? 0 : 1);
        for (int32_t k = 0; k < 3; ++k) o.putf(targets[t][k]);
    }
    o.put(gCoasterGridCellX);
    o.put(gCoasterGridCellZ);
    o.put(lab(gCoasterPreviousPointedRide));
    o.put(lab(gPointedRide));
    return o.n;
}

}  // extern "C"
