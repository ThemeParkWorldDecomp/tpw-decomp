// Test library for unit v51125 (libtpw_unit_std_exception_what.dylib, tests/test_std_exception_what.py), main app:
//   src/game/MslSupport.cpp, src/game/online/FS_Online.cpp, src/game/park/TPHField.cpp, src/game/ui/ui_FeatureStatus.cpp  small leaves: the exception what()s, three destructors with a delete flag, byte swaps, the ushort set tree's
//                               destroy, three online list destructors, std::list<std::string>::clear, FeatureStatusPanel::DeInitViewWindow
//   src/engine/EngineUtil.cpp, src/engine/M3D2Load.cpp, src/game/park/TPHField.cpp           sAlignedBox::Byteswap (tested where it is)
//
// The callees outside the unit are recording fakes (the same event names and arguments as the PowerPC stubs of the module); the globals of
// other files the unit writes are defined here. Objects are raw arena blocks written by the test with Mac offsets or the host's.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "std_exception_what.h"
#include "m3d2.h"
#include "ui_main.h"
#include "Wavelet.h"

using namespace calllog;

#pragma clang diagnostic ignored "-Winvalid-offsetof"

// ---- globals of other files (the vtable word stand-ins of the length_error destructor)
namespace IME {
char gMslLengthErrorVtbl[16];   // 0x10278b74
char gMslExcVtblB[16];          // 0x10278bb0
char gMslExcVtblC[16];          // 0x10278bc0
}  // namespace IME

TbFileOpenFn gFileOpen;
TbFileReadFn gFileRead;
TbFileSeekFn gFileSeek;
TbFileTellFn gFileTell;
TbFileNameFn gFileName;
TbIsFirstPersonFn gIsFirstPerson;
TbUseSmallTexturesFn gUseSmallTextures;
TbReadClockFn gReadClock;
TbUpdateMouseFn gUpdateMouse;
TbMallocFn gMalloc;
TbFreeFn gFree;
TbFileCloseFn gFileClose;  // 0x101735e0: src/engine/EngineUtil.cpp, src/engine/M3D2Load.cpp, src/game/park/TPHField.cpp's loaders close their file through it (never run here)
void (*gCityRecordDeleter)(CFSOnlineCity* city);   // FS_OnlineList.h: the hook the weak DestroyCityRecord calls (never run here)

namespace {
void* g_objs[256];   // label -> address (set by the test), what the RemoveHead fakes answer
}

// ---- fakes: the two RemoveHead instances (0x10177e2c city, 0x10177b98 park): the count drops by one, the next queued object comes back
template<> CFSOnlineCity* TbTList<CFSOnlineCity*, CFSOnlineCity*>::RemoveHead() {
    ev("RemoveHead", {lab(this)});
    mCount -= 1;
    return static_cast<CFSOnlineCity*>(g_objs[ret("RemoveHead") & 255]);
}
template<> CFSOnlinePark* TbTList<CFSOnlinePark*, CFSOnlinePark*>::RemoveHead() {
    ev("RemoveHead", {lab(this)});
    mCount -= 1;
    return static_cast<CFSOnlinePark*>(g_objs[ret("RemoveHead") & 255]);
}

namespace IME {
// 0x100025e0 std::_RefCountedPtr<char, std::_Array<char>>::~_RefCountedPtr (this = the exception + 4 on the Mac)
MslRefCountedChars* MslRefCountedCharsDtor25e0(MslRefCountedChars* self, int16_t deleteFlag) {
    ev("RcDtor", {lab(reinterpret_cast<char*>(self) - offsetof(MslExceptionObject, mRc)), deleteFlag});
    return self;
}
}  // namespace IME

namespace stdexc_callee {
void OperatorDelete(void* block) { ev("Delete", {lab(block)}); }
void UStringDtor(SamsUtilities::UString* string, int16_t flag) { ev("UStringDtor", {lab(string), flag}); }
void StringDtor(void* string, int16_t flag) {   // the string is the node's at +8 on the Mac, +16 here
    ev("StringDtor", {lab(static_cast<char*>(string) - offsetof(IME::MslStrNode, mStr)), flag});
}
void DestroyCityInfos(ccCityInfo* infos) { ev("DestroyInfos", {lab(infos), 0x102674f0}); }
void DestroyParkInfos(ccParkInfo* infos) { ev("DestroyInfos", {lab(infos), 0x10267508}); }
void DestroyCityRecord(CFSOnlineCity* city) { ev("VDtor", {lab(city), 1}); }
void DestroyParkRecord(CFSOnlinePark* park) { ev("VDtor", {lab(park), 1}); }
void DestroyCityList(TbPtrList<CFSOnlineCity>* list) { ev("VDtor", {lab(list), 1}); }
void DestroyParkList(TbPtrList<CFSOnlinePark>* list) { ev("VDtor", {lab(list), 1}); }
void UshortDestroy(UshortSetTree* tree, uint16_t* value) {
    ev("UsDestroy", {lab(tree), lab(reinterpret_cast<char*>(value) - offsetof(UshortSetTree::node, mValue))});
}
void UshortNodeDeallocate(UshortSetTree* tree, UshortSetTree::node* node, unsigned long count) {
    ev("UsDealloc", {lab(tree), lab(node), static_cast<int64_t>(count)});
}
}  // namespace stdexc_callee

// 0x101703e4 (the weak default of DestroyCityInfos names it; the fake never runs it)
ccCityInfo::~ccCityInfo() {}
ccParkInfo::~ccParkInfo() {}   // 0x1016ff48

// 0x1000221c (the weak default of StringDtor names it; the fake never runs it)
namespace msl_char_string {
MslCharString* destroy(MslCharString* self, int16_t) { return self; }
}

// src/engine/EngineUtil.cpp, src/engine/M3D2Load.cpp, src/game/park/TPHField.cpp's other functions need the platform's block swap; never run here
namespace SamsUtilities {
// (the platform's are no-ops on a little-endian host; the original swaps, so the fakes swap, which is what the Mac's call does)
void _USwapBlock16(uint16_t* words, int32_t count) {
    for (int32_t i = 0; i < count; ++i) words[i] = __builtin_bswap16(words[i]);
}
void _USwapBlock32(uint32_t* words, int32_t count) {
    for (int32_t i = 0; i < count; ++i) words[i] = __builtin_bswap32(words[i]);
}
void UDebug(const char*, ...) {}
}

void sMatrix::Byteswap() {}   // 0x1004a57c

// 0x101815d4
void UIMain_DestroyRidePreview() { ev("DestroyRidePreview", {}); }

extern "C" {

// The host address of a global by the module's name for it.
uint64_t tpw_stdexc_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"vtblA", IME::gMslLengthErrorVtbl},
        {"vtblB", IME::gMslExcVtblB},
        {"vtblC", IME::gMslExcVtblC},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_stdexc_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

void tpw_stdexc_label(int32_t label, uint64_t address) { g_objs[label & 255] = reinterpret_cast<void*>(static_cast<uintptr_t>(address)); }

// The offsets and sizes the module relies on (HOST_LAYOUT).
int32_t tpw_stdexc_layout(int64_t* out) {
    using List = CFSOnlineSystemParkList_InWorldByParkID;
    using Received = ccReceivedList<ccParkInfo>;
    using TbList = TbTList<CFSOnlineCity*, CFSOnlineCity*>;
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(IME::MslExceptionObject, mRc));
    out[n++] = static_cast<int64_t>(sizeof(IME::MslExceptionObject));
    out[n++] = static_cast<int64_t>(offsetof(UshortSetTree::node, mRight));
    out[n++] = static_cast<int64_t>(offsetof(UshortSetTree::node, mParent));
    out[n++] = static_cast<int64_t>(offsetof(UshortSetTree::node, mValue));
    out[n++] = static_cast<int64_t>(sizeof(UshortSetTree::node));
    out[n++] = static_cast<int64_t>(offsetof(List, mF24));
    out[n++] = static_cast<int64_t>(offsetof(List, mF28));
    out[n++] = static_cast<int64_t>(offsetof(List, mList));
    out[n++] = static_cast<int64_t>(offsetof(List, mReceived));
    out[n++] = static_cast<int64_t>(sizeof(List));
    out[n++] = static_cast<int64_t>(offsetof(Received, mInfos));
    out[n++] = static_cast<int64_t>(offsetof(Received, mRefCount));
    out[n++] = static_cast<int64_t>(sizeof(Received));
    out[n++] = static_cast<int64_t>(offsetof(TbList, mCount));
    out[n++] = static_cast<int64_t>(sizeof(TbList));
    out[n++] = static_cast<int64_t>(offsetof(IME::MslStrList, mHead) + offsetof(IME::MslStrLink, mPrev));
    out[n++] = static_cast<int64_t>(offsetof(IME::MslStrList, mHead) + offsetof(IME::MslStrLink, mNext));
    out[n++] = static_cast<int64_t>(offsetof(IME::MslStrNode, mNext));
    out[n++] = static_cast<int64_t>(offsetof(IME::MslStrNode, mStr));
    out[n++] = static_cast<int64_t>(sizeof(IME::MslStrNode));
    out[n++] = static_cast<int64_t>(sizeof(TbTimeStamp));
    return n;
}

// ---- the functions: every wrapper takes two words and answers one (an address or a value)
#define PTR(T, a) reinterpret_cast<T*>(static_cast<uintptr_t>(a))
#define ADDR(p) static_cast<uint64_t>(reinterpret_cast<uintptr_t>(p))

uint64_t tpw_stdexc_exc_what(uint64_t a, uint64_t) { return ADDR(stdexc::ExceptionWhat(PTR(const void, a))); }
uint64_t tpw_stdexc_logic_what(uint64_t a, uint64_t) { return ADDR(stdexc::LogicErrorWhat(PTR(IME::MslExceptionObject, a))); }
uint64_t tpw_stdexc_length_dtor(uint64_t a, uint64_t f) { return ADDR(IME::MslLengthErrorDtor(PTR(IME::MslExceptionObject, a), static_cast<int16_t>(f))); }
uint64_t tpw_stdexc_uresid_dtor(uint64_t a, uint64_t f) {
    return ADDR(stdexc::UResourceIdDestruct(PTR(SamsUtilities::UResourceId, a), static_cast<int16_t>(f)));
}
uint64_t tpw_stdexc_cmap_dtor(uint64_t a, uint64_t f) { return ADDR(stdexc::CMapLinkBaseDestruct(PTR(CMapLinkBase, a), static_cast<int16_t>(f))); }
uint64_t tpw_stdexc_box_swap(uint64_t a, uint64_t) {
    PTR(sAlignedBox, a)->Byteswap();
    return 0;
}
uint64_t tpw_stdexc_swap32(uint64_t a, uint64_t) { return USwap32UCharPtr_1005e5b4(static_cast<uint32_t>(a)); }
uint64_t tpw_stdexc_swap16(uint64_t a, uint64_t) {
    USwapInplace16UShort_1005e610(*PTR(uint16_t, a));
    return 0;
}
uint64_t tpw_stdexc_swapinplace32(uint64_t a, uint64_t) {
    USwapInplace32ULong_1005e658(*PTR(uint32_t, a));
    return 0;
}
uint64_t tpw_stdexc_ts_ctor(uint64_t a, uint64_t) { return ADDR(stdexc::TbTimeStampCtor(PTR(TbTimeStamp, a))); }
uint64_t tpw_stdexc_tree_destroy(uint64_t a, uint64_t n) {
    PTR(UshortSetTree, a)->destroy(PTR(UshortSetTree::node, n));
    return 0;
}
uint64_t tpw_stdexc_city_dtor(uint64_t a, uint64_t f) {
    using CityList = CFSOnlineSystemList<TbPtrList<CFSOnlineCity>, ccCityList>;
    return ADDR(stdexc::CityListDestruct(PTR(CityList, a), static_cast<int16_t>(f)));
}
uint64_t tpw_stdexc_park1_dtor(uint64_t a, uint64_t f) {
    return ADDR(stdexc::ParkListByParkIDDestruct(PTR(CFSOnlineSystemParkList_InWorldByParkID, a), static_cast<int16_t>(f)));
}
uint64_t tpw_stdexc_park2_dtor(uint64_t a, uint64_t f) {
    return ADDR(stdexc::ParkListTop10Destruct(PTR(CFSOnlineSystemParkList_InCityTop10, a), static_cast<int16_t>(f)));
}
uint64_t tpw_stdexc_list_clear(uint64_t a, uint64_t) {
    IME::MslStrList_clear(PTR(IME::MslStrList, a));
    return 0;
}
uint64_t tpw_stdexc_deinit(uint64_t a, uint64_t) {
    PTR(FeatureStatusPanel, a)->DeInitViewWindow();
    return 0;
}

}  // extern "C"
