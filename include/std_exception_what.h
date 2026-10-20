// Unit v51125 (src/std_exception_what.cpp): small leaves of the main app, translated on their own (Mac addresses in comments).
// The destructors take the hidden signed 16 bit delete flag (r4) and answer `self`; the stores of vtable words the compiler makes are not
// written where the host has no Mac vtable (a note on each). The callees outside the unit are in stdexc_callee (weak defaults in
// src/std_exception_what_callees.cpp, recording fakes in the test library).
#pragma once
#include <cstdint>

#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "MapWho.h"
#include "TbTimeDiff.h"
#include "ui_ime.h"
#include "platform/samsutils.h"

namespace stdexc {
const char* ExceptionWhat(const void* self);                              // 0x100025a8 std::exception::what: "exception"
const char* LogicErrorWhat(const IME::MslExceptionObject* self);          // 0x10002570 std::logic_error::what: the message word
SamsUtilities::UResourceId* UResourceIdDestruct(SamsUtilities::UResourceId* self, int16_t deleteFlag);   // 0x10001ad0
CMapLinkBase* CMapLinkBaseDestruct(CMapLinkBase* self, int16_t deleteFlag);                              // 0x10007b04 (the vtable store is skipped)
TbTimeStamp* TbTimeStampCtor(TbTimeStamp* self);                          // 0x1013dca8: both words zero
// 0x101701c8, 0x10170834, 0x10170ca8: the destructors with the base's work inline (the vtable stores are skipped).
CFSOnlineSystemList<TbPtrList<CFSOnlineCity>, ccCityList>* CityListDestruct(CFSOnlineSystemList<TbPtrList<CFSOnlineCity>, ccCityList>* self, int16_t deleteFlag);
CFSOnlineSystemParkList_InWorldByParkID* ParkListByParkIDDestruct(CFSOnlineSystemParkList_InWorldByParkID* self, int16_t deleteFlag);
CFSOnlineSystemParkList_InCityTop10* ParkListTop10Destruct(CFSOnlineSystemParkList_InCityTop10* self, int16_t deleteFlag);
}  // namespace stdexc

namespace IME {
MslExceptionObject* MslLengthErrorDtor(MslExceptionObject* self, int16_t deleteFlag);   // 0x100024ac std::length_error::~length_error
// MslStrList_clear (0x1017fc04) is declared in ui_ime.h.
}

// The two RemoveHead instances the list destructors call (0x10177e2c city, 0x10177b98 park) are callees outside the unit: explicit
// specializations (weak in src/std_exception_what.cpp, recording fakes in the test library) so the call is not inlined.
template<> CFSOnlineCity* TbTList<CFSOnlineCity*, CFSOnlineCity*>::RemoveHead();
template<> CFSOnlinePark* TbTList<CFSOnlinePark*, CFSOnlinePark*>::RemoveHead();

// The byte swaps of TPEngine.h (same declarations).
void USwapInplace32ULong_1005e658(uint32_t& word);   // 0x1005e658
void USwapInplace16UShort_1005e610(uint16_t& half);  // 0x1005e610
uint32_t USwap32UCharPtr_1005e5b4(uint32_t word);    // 0x1005e5b4

// Callees outside the unit.
namespace stdexc_callee {
void OperatorDelete(void* block);                                   // 0x10238218
void UStringDtor(SamsUtilities::UString* string, int16_t flag);     // 0x10238290
void StringDtor(void* string, int16_t flag);                        // 0x1000221c
void DestroyCityInfos(ccCityInfo* infos);                           // __destroy_new_array 0x10238ad0 with the TVector 0x102674f0
void DestroyParkInfos(ccParkInfo* infos);                           // the same with the TVector 0x10267508
void DestroyCityRecord(CFSOnlineCity* city);                        // virtual destructor (slot +8) through __ptr_glue, flag 1
void DestroyParkRecord(CFSOnlinePark* park);
void DestroyCityList(TbPtrList<CFSOnlineCity>* list);
void DestroyParkList(TbPtrList<CFSOnlinePark>* list);
void UshortDestroy(UshortSetTree* tree, uint16_t* value);           // 0x1016e988 allocator<unsigned short>::destroy (this = the tree)
void UshortNodeDeallocate(UshortSetTree* tree, UshortSetTree::node* node, unsigned long count);   // 0x1016e8b0 (this = tree + 4)
}  // namespace stdexc_callee
