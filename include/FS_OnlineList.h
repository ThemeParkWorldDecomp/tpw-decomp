// FS_OnlineList.h (main app): the online system's lists and the park record they hold (inventory files FS_Online.cpp and
// FS_OnlineList.cpp; unit w68 translates the constructors, the destructors and the request members follow in w69).
//
// The online library hands every answer back as a "cc" list: a count, an array of "cc" infos made with new[] and a reference count.
// CFSOnlineSystemList<LIST, RECEIVED> (0x34 bytes) holds the typed list it fills (LIST, a TbPtrList of the game's records) and the
// last answer it stored (RECEIVED, reference counted, released by Reset). The system builds nine of them:
//   CFSOnlineSystemCityList, five park lists (CFSOnlineSystemParkList and its four subclasses), the theme, server and ride lists.
// Layouts are the Mac's (the host's pointers are 64 bits wide, so tests compare members, not bytes).
#pragma once
#include <cstdint>

#include "Ptr32.h"
#include "TbDynamicStringTemplate.h"
#include "TbTList.h"

class City;                  // the online library's city connection (an import)
class CFSOnlineCity;         // record classes the typed lists hold (untranslated, held by pointer only)
class CFSOnlineTheme;
class CFSOnlineServer;
class CFSOnlineRide;
class CFSOnlineSystemParkList;
class CFSOnlinePark;

// What the library answers: a count, the infos (new[], so the array has the runtime's cookie), a reference count. The Mac's ccXList
// classes are the library's; only these members are read (InitCity's release). The info types are the game's own records with
// destructors in main (their sizes are not known except ccParkInfo's, the stride 0x66 CFSOnlineSystemParkList::StoreReceivedList walks).
template<class INFO> struct ccReceivedList {
    int32_t mCount;          // 0x00
    INFO* mInfos;            // 0x04  made with new[]; destroyed last to first when the last reference goes
    int32_t mF08;            // 0x08
    int32_t mRefCount;       // 0x0c
};
struct ccParkInfo {
    ~ccParkInfo();           // 0x1016ff48
    // Unit v5917: the destructor with the hidden delete flag (r4): frees the five blocks at 4, 0xc, 8, 0x44 and 0x4c (32-bit words),
    // clears the words, and for a flag above 0 (signed 16 bit) deletes the object. NULL is allowed.
    static ccParkInfo* Destruct(ccParkInfo* self, int16_t deleteFlag);
    uint8_t mF00[0x66];      // 0x66 bytes (the stride of StoreReceivedList)
};
struct ccCityInfo {
    ~ccCityInfo();           // 0x101703e4: frees the blocks at 0x04, 0x08 and 0x1c
    // Unit v5921: the file's 32-bit layout, the words the destructor reads.
    uint32_t mF00;                       // 0x00
    Ptr32<void> mBlock04;                // 0x04  malloc'd
    Ptr32<void> mBlock08;                // 0x08  malloc'd
    uint8_t mF0C[0x1c - 0x0c];           // 0x0c
    Ptr32<void> mBlock1C;                // 0x1c  malloc'd
};
struct ccThemeInfo {
    ~ccThemeInfo();          // 0x101732e4
    // Unit v5961: the destructor with the hidden delete flag (r4, signed 16 bit): frees the blocks at 4, 8 and 0xc, clears the words,
    // deletes the object for a flag above 0. NULL is allowed.
    static ccThemeInfo* Destruct(ccThemeInfo* self, int16_t deleteFlag);
    // Unit v5670: the fields CFSOnlineTheme's constructor reads (the file's 32-bit layout, 0x10 bytes).
    uint32_t mF00;                       // 0x00
    Ptr32<const char16_t> mStr04;        // 0x04
    Ptr32<const char16_t> mStr08;        // 0x08
    Ptr32<const char16_t> mStr0C;        // 0x0c
};
#pragma pack(push, 1)
struct ccServerInfo {
    ~ccServerInfo();         // 0x101731dc
    // Unit v5049: the fields CFSOnlineServer's constructor reads (the file's 32-bit layout, 0x26 bytes).
    uint32_t mF00;                       // 0x00
    uint32_t mF04;                       // 0x04
    Ptr32<const char16_t> mStr08;        // 0x08
    Ptr32<const char16_t> mStr0C;        // 0x0c
    uint32_t mF10;                       // 0x10
    uint16_t mF14;                       // 0x14
    Ptr32<const char16_t> mStr16;        // 0x16
    Ptr32<const char16_t> mStr1A;        // 0x1a
    uint32_t mF1E;                       // 0x1e
    Ptr32<const char16_t> mStr22;        // 0x22
};
#pragma pack(pop)
#pragma pack(push, 1)
struct ccRideInfo {
    ~ccRideInfo();           // 0x10173110
    // Unit v5961: the same with the blocks at 4, 8 and 0x24.
    static ccRideInfo* Destruct(ccRideInfo* self, int16_t deleteFlag);
    // Unit v5141: the fields CFSOnlineRide's constructor reads (the file's 32-bit layout, 0x34 bytes).
    uint32_t mF00;                       // 0x00
    Ptr32<const char16_t> mStr04;        // 0x04
    Ptr32<const char16_t> mStr08;        // 0x08
    uint32_t mF0C;                       // 0x0c
    uint32_t mF10;                       // 0x10
    uint32_t mF14;                       // 0x14
    uint32_t mF18;                       // 0x18
    uint32_t mF1C;                       // 0x1c
    uint32_t mF20;                       // 0x20
    Ptr32<const char16_t> mStr24;        // 0x24
    uint32_t mF28;                       // 0x28
    uint32_t mF2C;                       // 0x2c
    uint32_t mF30;                       // 0x30
};
#pragma pack(pop)
typedef ccReceivedList<ccParkInfo> ccParkList;
typedef ccReceivedList<ccCityInfo> ccCityList;
typedef ccReceivedList<ccThemeInfo> ccThemeList;
typedef ccReceivedList<ccServerInfo> ccServerList;
typedef ccReceivedList<ccRideInfo> ccRideList;

// What a list request calls back with: the request's id, the caller's context and the list that was filled (the Mac's pointer to a
// function; the host's differs).
template<class LIST> struct OnlineListCallback {
    typedef void (*Type)(int32_t id, const void* context, LIST* list);
};

// The common 0x34-byte base (vtable: +8 the destructor, +0xc ChildLocked; the derived classes add RequestListFromServer(City*,
// unsigned long) at +0x10 and StoreReceivedList(const cc...List*) at +0x14, untranslated and not declared yet).
template<class LIST, class RECEIVED> class CFSOnlineSystemList {
public:
    // The constructors the Mac builds (the typed lists' own, or inlined into CFSOnlineSystem's): the words zeroed, the list made.
    CFSOnlineSystemList()
        : mRequestID(0), mF08(0), mF0C(0), mF10(0), mF14(0), mF18(0), mF1C(0), mF20(0), mF24(0), mF28(0), mReceived(nullptr) {
        mList = new LIST;
    }
    virtual ~CFSOnlineSystemList();   // the instances' out-of-line copies: 0x1017004c, 0x101701c8, 0x10179b08, 0x1017a6e4, 0x1017b288

    // Unit v5215: the other virtual slots (Mac vtable +0xc, +0x10, +0x14; the derived lists override them, the bodies here are placeholders).
    virtual int32_t ChildLocked() { return 0; }                                 // +0xc: nonzero while a parent request is in the way
    virtual int32_t RequestListFromServer(City*, uint32_t) { return 0; }       // +0x10: answers the request id
    virtual void StoreReceivedList(const RECEIVED*) {}                          // +0x14
    // 0x10178414 (412, city instance): asks the city server for the list once logged in, or reports the failure (answer 1) to the callback.
    void LoggedInGetList();

    // The block InitCity runs on each of the nine lists (inlined there): the request id back to -1, seven words cleared, and the stored
    // answer released: its count drops by one, and at zero the infos are destroyed (last to first) and freed and the answer itself freed
    // (operator delete only, no destructor); the pointer is cleared whatever the count was.
    void Reset() {
        mRequestID = -1;
        mF08 = 0;
        mF0C = 0;
        mF10 = 0;
        mF14 = 0;
        mF18 = 0;
        mF1C = 0;
        mF20 = 0;
        if (mReceived != nullptr) {
            --mReceived->mRefCount;
            if (mReceived->mRefCount == 0) {
                if (mReceived->mInfos != nullptr) delete[] mReceived->mInfos;
                mReceived->mInfos = nullptr;
                ::operator delete(static_cast<void*>(mReceived));
            }
            mReceived = nullptr;
        }
    }

    // TickCity's completion step, one instantiation per list (0x10173f98 city, 0x101741d4 park, 0x10174410 ride, 0x1017464c server,
    // 0x1017488c theme; untranslated, 412 or 468 bytes each): takes the answer the library stored and hands it to the typed list.
    void CompleteGetList(int32_t error);

    int32_t mRequestID;      // 0x04  the id of the request in flight, -1 when none
    int32_t mF08;            // 0x08
    int32_t mF0C;            // 0x0c  nonzero while a request for the list is out (TickCity clears it before CompleteGetList)
    int32_t mF10;            // 0x10  nonzero once the answer arrived (TickCity completes the request when both are set)
    int32_t mF14;            // 0x14
    int32_t mF18;            // 0x18
    int32_t mF1C;            // 0x1c
    int32_t mF20;            // 0x20
    int32_t mF24;            // 0x24  (not cleared by Reset)
    int32_t mF28;            // 0x28  (not cleared by Reset)
    LIST* mList;             // 0x2c  made by the constructor (0x10 bytes)
    RECEIVED* mReceived;     // 0x30  the stored answer, reference counted
};

// Unit v5215 (src/CFSOnlineSystemList_LoggedInGetList.cpp): the city instances, 0x10178414 and 0x10173f98.
template<> void CFSOnlineSystemList<TbPtrList<CFSOnlineCity>, ccCityList>::LoggedInGetList();
template<> void CFSOnlineSystemList<TbPtrList<CFSOnlineCity>, ccCityList>::CompleteGetList(int32_t error);
// The Mac calls the list's callback (a function pointer word at 0x1c, its context at 0x20: 32-bit words in the host layout) through
// __ptr_glue, and a city record's virtual destructor (vtable +8, flag 1). The host fields cannot hold a function pointer and CFSOnlineCity
// has no vtable, so both go through these hooks (the defaults treat the word as a 32-bit address, and delete the record); tests replace them.
extern void (*gOnlineListCallbackCaller)(int32_t callback, int32_t error, int32_t context, void* list);
extern void (*gCityRecordDeleter)(CFSOnlineCity* city);

class CFSOnlineSystemCityList : public CFSOnlineSystemList<TbPtrList<CFSOnlineCity>, ccCityList> {
public:
    CFSOnlineSystemCityList() {}      // (inlined into CFSOnlineSystem's constructor on the Mac)
    // 0x10178718 (352, untranslated): asks the city server for the list of cities; non-virtual.
    void GetList(OnlineListCallback<TbPtrList<CFSOnlineCity>>::Type callback, const void* context);
    ~CFSOnlineSystemCityList() override;   // 0x10170e20
    // Unit v5307 (src/FS_Online_CFSOnlineSystemParkList_InWorldByPlayerName_Dtor_CFSOnlineSystemParkList_InCity_GetList_CFSOnlineSystemCityList_StoreReceivedList.cpp).
    int32_t ChildLocked() override;                                   // 0x101788d8 (196): 1 unless the child lock (bit 0x10) forbids the lobby
    int32_t RequestListFromServer(City* city, uint32_t unused) override;   // 0x101789d8 (84)
    void StoreReceivedList(const ccCityList* list) override;          // 0x10178a78 (116): one CFSOnlineCity (0x68 bytes) per 0x30-byte info
};

// The park lists: the base adds nothing (0x34 bytes); the subclasses add a word at 0x34 set to -1 (InWorldByPlayerName a string).
class CFSOnlineSystemParkList : public CFSOnlineSystemList<TbPtrList<CFSOnlinePark>, ccParkList> {
public:
    typedef OnlineListCallback<TbPtrList<CFSOnlinePark>>::Type Callback;
    CFSOnlineSystemParkList() {}
    ~CFSOnlineSystemParkList() override;   // 0x1016fd48
    // Unit v51039: 0x10178bb0 (116, stand-in): one CFSOnlinePark (0xc0 bytes) per 0x66-byte info; the parks are not kept (a leak).
    void StoreReceivedList(const ccParkList* list) override;
};
#ifndef TPW_V51039_STRONG
__attribute__((weak)) inline void CFSOnlineSystemParkList::StoreReceivedList(const ccParkList*) {}
#endif
// Callee the host cannot call as the Mac does (operator new(0xc0) then the constructor 0x1017c0d8): a weak definition in
// src/Serialise_basic_std_list_std_basic_string_std_allocator_std_basi_4_callees.cpp, faked by the test library.
namespace parklist_callee {
void NewPark(const ccParkInfo* info, CFSOnlineSystemParkList* list);
}

// Unit v5994 (src/CFSOnlineSystemRideList_RequestListFromServer_CFSOnlineCity_CFSOnlineCity.cpp): the ride instances of the list callees,
// 0x10174410 and 0x1017b6c8 (untranslated, faked by the test library), and the list calls of the city and server records, weak
// definitions in that file that the test library overrides.
template<> void CFSOnlineSystemList<TbPtrList<CFSOnlineRide>, ccRideList>::CompleteGetList(int32_t error);
template<> void CFSOnlineSystemList<TbPtrList<CFSOnlineRide>, ccRideList>::LoggedInGetList();
namespace onlinelist_callee {
void CityAddTail(TbPtrList<CFSOnlineCity>* list, CFSOnlineCity* city);     // 0x1017d93c
void* CityFind(TbPtrList<CFSOnlineCity>* list, CFSOnlineCity* city);       // 0x1017d7d8
void CityRemoveAt(TbPtrList<CFSOnlineCity>* list, void*& position);        // 0x1017d858
void FreeCity(void* city);                                                 // operator delete (0x10238218)
void* ServerFind(TbPtrList<CFSOnlineServer>* list, CFSOnlineServer* server);   // 0x1017d120
void ServerRemoveAt(TbPtrList<CFSOnlineServer>* list, void*& position);        // 0x1017d258
}
void RideListPostLoginCallbackFn(int32_t error, const void* context);   // 0x1017b604 (140): makes the online system when there is none, then asks the ride list
class CFSOnlineSystemParkList_InWorldByParkID : public CFSOnlineSystemParkList {
public:
    CFSOnlineSystemParkList_InWorldByParkID() : mF34(-1) {}
    void GetList(int32_t parkID, Callback callback, const void* context);   // 0x10179794 (352, untranslated)
    ~CFSOnlineSystemParkList_InWorldByParkID() override;   // 0x10170834
    // Unit v5928
    int32_t RequestListFromServer(City* city, uint32_t arg) override;   // 0x10179964 (104)
    int32_t mF34;            // 0x34
};
class CFSOnlineSystemParkList_InWorldByPlayerName : public CFSOnlineSystemParkList {
public:
    void GetList(const char16_t* playerName, Callback callback, const void* context);   // 0x101794e8 (364, untranslated)
    CFSOnlineSystemParkList_InWorldByPlayerName();   // (inlined into CFSOnlineSystem's constructor on the Mac) assigns the empty string after making it
    ~CFSOnlineSystemParkList_InWorldByPlayerName() override;   // 0x101709b0
    TbDynamicStringTemplate<char16_t> mPlayerName;   // 0x34  (0x48 bytes in all)
    // Unit v5762
    int32_t RequestListFromServer(City* city, uint32_t unused) override;   // 0x101796cc (104)
};
class CFSOnlineSystemParkList_InCity : public CFSOnlineSystemParkList {
public:
    CFSOnlineSystemParkList_InCity() : mF34(-1) {}
    // 0x10178f54 (352, untranslated): TickCity asks (1, NULL, NULL) of the second InCity list.
    void GetList(int32_t cityID, Callback callback, const void* context);
    ~CFSOnlineSystemParkList_InCity() override;   // 0x10170b34
    CFSOnlinePark* ReturnParkInfo(int32_t index);   // 0x101791d8 (60, unit v5130, stand-in)
    // Unit v5532
    int32_t RequestListFromServer(City* city, uint32_t unused) override;   // 0x1017911c (104)
    int32_t mF34;            // 0x34
};
class CFSOnlineSystemParkList_InCityTop10 : public CFSOnlineSystemParkList {
public:
    CFSOnlineSystemParkList_InCityTop10() : mF34(-1) {}
    void GetList(int32_t cityID, Callback callback, const void* context);   // 0x1017925c (352, untranslated)
    ~CFSOnlineSystemParkList_InCityTop10() override;   // 0x10170ca8
    // Unit v5928
    int32_t RequestListFromServer(City* city, uint32_t arg) override;   // 0x10179428 (104)
    int32_t mF34;            // 0x34
};
// Unit v5532: a weak default so other units' libraries (which build this vtable but not the src file) link; the src file defines it for real.
#ifndef TPW_V5532_STRONG
__attribute__((weak)) inline int32_t CFSOnlineSystemParkList_InCity::RequestListFromServer(City*, uint32_t) { return 0; }
#endif
// Unit v5928: the same weak default for the by-park-id, top-10 and theme lists.
#ifndef TPW_V5928_STRONG
__attribute__((weak)) inline int32_t CFSOnlineSystemParkList_InWorldByParkID::RequestListFromServer(City*, uint32_t) { return 0; }
__attribute__((weak)) inline int32_t CFSOnlineSystemParkList_InCityTop10::RequestListFromServer(City*, uint32_t) { return 0; }
#endif
// Unit v5762: the same weak default for the player-name list.
#ifndef TPW_V5762_STRONG
__attribute__((weak)) inline int32_t CFSOnlineSystemParkList_InWorldByPlayerName::RequestListFromServer(City*, uint32_t) { return 0; }
#endif

// The theme, server and ride lists keep their own out-of-line constructors (FS_OnlineList.cpp) and add nothing to the base.
class CFSOnlineSystemThemeList : public CFSOnlineSystemList<TbPtrList<CFSOnlineTheme>, ccThemeList> {
public:
    CFSOnlineSystemThemeList();   // 0x10179a28
    void GetList();               // 0x1017a1c0 (348, untranslated)
    ~CFSOnlineSystemThemeList() override;   // 0x10179d28
    char* ReturnOnlineThemeName(int32_t theme);   // 0x1017a588
    // Unit v5043: 0x1017a4c0 (128, untranslated), the id of the theme called `name` (a byte).
    uint8_t ReturnOnlineThemeID(const char* name);
    // Unit v5928
    int32_t RequestListFromServer(City* city, uint32_t arg) override;   // 0x1017a354 (88)
    void StoreReceivedList(const ccThemeList* list) override;           // 0x1017a3fc (116): one CFSOnlineTheme (0x48 bytes) per 0x10-byte info
};
#ifndef TPW_V5928_STRONG
__attribute__((weak)) inline int32_t CFSOnlineSystemThemeList::RequestListFromServer(City*, uint32_t) { return 0; }
__attribute__((weak)) inline void CFSOnlineSystemThemeList::StoreReceivedList(const ccThemeList*) {}
#endif
// Unit v5928: the theme instances (fakes in the test) and the login callback (0x10179e8c, 140).
template<> void CFSOnlineSystemList<TbPtrList<CFSOnlineTheme>, ccThemeList>::LoggedInGetList();     // 0x10179f50 (412)
template<> void CFSOnlineSystemList<TbPtrList<CFSOnlineTheme>, ccThemeList>::CompleteGetList(int32_t error);   // 0x1017488c (468)
void ThemeListPostLoginCallbackFn(int32_t error, const void* context);
// Unit v51110: the base destructors of the server list (the 0x1017a6e4 copy) and of the ride list (0x1017b288); the derived destructors
// (0x1017a908, 0x1017b4a4) are the same code with the base's work inline, so they are empty here and run the base's.
template<> CFSOnlineSystemList<TbPtrList<CFSOnlineServer>, ccServerList>::~CFSOnlineSystemList();
template<> CFSOnlineSystemList<TbPtrList<CFSOnlineRide>, ccRideList>::~CFSOnlineSystemList();
// Unit v51129 (src/ui_Postcard_panel_std_basic_string_assign_2.cpp): the theme list's destructor (0x10179d28) has the base's work inline; the
// callees of that work, weak defaults in the source (the real calls) that the test library replaces with recording fakes.
namespace onlinelist3_callee {
CFSOnlineTheme* ThemeRemoveHead(TbPtrList<CFSOnlineTheme>* list);   // 0x10177904
void DestroyTheme(CFSOnlineTheme* theme);                           // the virtual destructor, slot +8, with the delete flag 1
void DestroyThemeList(TbPtrList<CFSOnlineTheme>* list);
void DestroyThemeInfos(ccThemeInfo* infos);                         // __destroy_new_array (0x10238ad0) with the TVector 0x10267498
}
class CFSOnlineSystemServerList : public CFSOnlineSystemList<TbPtrList<CFSOnlineServer>, ccServerList> {
public:
    CFSOnlineSystemServerList();   // 0x1017a604
    void GetList();                // 0x1017ada8 (348, untranslated)
    ~CFSOnlineSystemServerList() override;   // 0x1017a908
    // Unit v5713 (src/CFSOnlineSystemServerList_RequestListFromServer.cpp)
    int32_t RequestListFromServer(City* city, uint32_t arg) override;   // 0x1017af40 (92): logs, asks the city for the server list; answers its request id
    void StoreReceivedList(const ccServerList* list) override;          // 0x1017afec (116): makes one CFSOnlineServer (0x80 bytes) per info
};

// Unit v5721 (src/ServerListPostLoginCallbackFn.cpp): the server instances (untranslated, fakes in the test).
template<> void CFSOnlineSystemList<TbPtrList<CFSOnlineServer>, ccServerList>::LoggedInGetList();     // 0x1017ab30 (412)
template<> void CFSOnlineSystemList<TbPtrList<CFSOnlineServer>, ccServerList>::CompleteGetList(int32_t error);   // 0x1017464c (468)
// 0x1017aa6c (140): makes the online system if there is none, then asks the server list: CompleteGetList(1) when error is nonzero,
// else LoggedInGetList. The context is unused.
// Weak fallbacks (the base placeholders) so unit libraries that do not link
// src/CFSOnlineSystemServerList_RequestListFromServer.cpp still resolve the vtable; the real bodies there are strong.
#ifndef TPW_SERVERLIST_STRONG
__attribute__((weak)) inline int32_t CFSOnlineSystemServerList::RequestListFromServer(City*, uint32_t) { return 0; }
__attribute__((weak)) inline void CFSOnlineSystemServerList::StoreReceivedList(const ccServerList*) {}
#endif
void ServerListPostLoginCallbackFn(int32_t error, const void* context);
// Unit v51110 (src/CFSOnlineSystemServerList_RequestListFromServer_CFSOnlineSystemServerList_ReturnFTPDownloadServe.cpp): the callees of the
// list destructors, weak defaults in the source (the real calls) that the test library replaces with recording fakes.
namespace onlinelist2_callee {
CFSOnlineRide* RideRemoveHead(TbPtrList<CFSOnlineRide>* list);          // 0x10177758
CFSOnlineServer* ServerRemoveHead(TbPtrList<CFSOnlineServer>* list);    // 0x1017782c
void DestroyRide(CFSOnlineRide* ride);                                  // the virtual destructor, slot +8, with the delete flag 1
void DestroyServer(CFSOnlineServer* server);
void DestroyRideList(TbPtrList<CFSOnlineRide>* list);
void DestroyServerList(TbPtrList<CFSOnlineServer>* list);
void DestroyRideInfos(ccRideInfo* infos);                               // __destroy_new_array (0x10238ad0) with the TVector 0x10267488
void DestroyServerInfos(ccServerInfo* infos);                           // the same with the TVector 0x10267490
void FreeBlock(void* block);                                            // operator delete (0x10238218)
}
class CFSOnlineSystemRideList : public CFSOnlineSystemList<TbPtrList<CFSOnlineRide>, ccRideList> {
public:
    CFSOnlineSystemRideList();   // 0x1017b1ac
    void GetList();              // 0x1017b938 (348, untranslated)
    ~CFSOnlineSystemRideList() override;   // 0x1017b4a4
    // Unit v5718
    int32_t RequestListFromServer(City* city, uint32_t id) override;   // 0x1017bb4c (88)
    void StoreReceivedList(const ccRideList* list) override;           // 0x1017bbf0 (116): one CFSOnlineRide (0x6c bytes) per 0x34-byte info
};
// Unit v5718: weak defaults so other units' libraries (which build this vtable but not the src file) link; the src file defines them for real.
#ifndef TPW_V5718_STRONG
__attribute__((weak)) inline int32_t CFSOnlineSystemRideList::RequestListFromServer(City*, uint32_t) { return 0; }
__attribute__((weak)) inline void CFSOnlineSystemRideList::StoreReceivedList(const ccRideList*) {}
#endif

// One park of an online list (0xc0 bytes). Names invented; the second constructor (0x1017c0d8, from a ccParkInfo, adds the park to its
// list) is not translated yet.
class CFSOnlinePark {
public:
    CFSOnlinePark();   // 0x1017bf30: every field "unset" (-1, empty strings, 0xff bytes)
    CFSOnlinePark(const ccParkInfo* info, CFSOnlineSystemParkList* list);   // 0x1017c0d8
    virtual ~CFSOnlinePark();   // 0x1017c464
    CFSOnlinePark& operator=(const CFSOnlinePark& other);   // 0x1017c2e4 (324, unit v5130, stand-in)

    CFSOnlineSystemParkList* mList;                 // 0x04  the list the park belongs to
    int32_t mParkID;                                // 0x08
    TbDynamicStringTemplate<char16_t> mText0C;      // 0x0c
    TbDynamicStringTemplate<char16_t> mText20;      // 0x20
    TbDynamicStringTemplate<char16_t> mText34;      // 0x34
    int32_t mF48;                                   // 0x48
    int32_t mF4C;                                   // 0x4c
    int32_t mF50;                                   // 0x50
    int32_t mF54;                                   // 0x54
    uint8_t mF58[0x20];                             // 0x58  32 bytes
    int32_t mF78;                                   // 0x78
    TbDynamicStringTemplate<char> mText7C;          // 0x7c
    int32_t mF90;                                   // 0x90
    TbDynamicStringTemplate<char16_t> mText94;      // 0x94
    int16_t mFA8;                                   // 0xa8
    int16_t mFAA;                                   // 0xaa
    int16_t mFAC;                                   // 0xac
    uint8_t mFAE[2];                                // 0xae  (never written)
    int32_t mFB0;                                   // 0xb0
    int32_t mFB4;                                   // 0xb4
    int32_t mFB8;                                   // 0xb8
    int32_t mFBC;                                   // 0xbc
};

// Unit v5049: one server of the online server list (0x80 bytes on the Mac). Names invented.
class CFSOnlineServer {
public:
    CFSOnlineServer(const ccServerInfo* info, CFSOnlineSystemServerList* list);   // 0x1017c7d0: adds itself to the list's tail
    virtual ~CFSOnlineServer();

    CFSOnlineSystemServerList* mList;               // 0x04
    int32_t mF08;                                   // 0x08
    int32_t mF0C;                                   // 0x0c
    TbDynamicStringTemplate<char16_t> mText10;      // 0x10
    TbDynamicStringTemplate<char16_t> mText24;      // 0x24
    int32_t mF38;                                   // 0x38
    int16_t mF3C;                                   // 0x3c
    TbDynamicStringTemplate<char16_t> mText40;      // 0x40
    TbDynamicStringTemplate<char16_t> mText54;      // 0x54
    int32_t mF68;                                   // 0x68
    TbDynamicStringTemplate<char16_t> mText6C;      // 0x6c
};

// Unit v5141: one ride of the online ride list (0x6c bytes on the Mac). Names invented.
class CFSOnlineRide {
public:
    CFSOnlineRide(const ccRideInfo* info, CFSOnlineSystemRideList* list);   // 0x1017ca7c: adds itself to the list's tail
    virtual ~CFSOnlineRide();

    CFSOnlineSystemRideList* mList;                 // 0x04
    int32_t mRideID;                                // 0x08
    TbDynamicStringTemplate<char16_t> mText0C;      // 0x0c
    TbDynamicStringTemplate<char16_t> mText20;      // 0x20
    int32_t mF34;                                   // 0x34
    int32_t mF38;                                   // 0x38
    int32_t mF3C;                                   // 0x3c
    int32_t mF40;                                   // 0x40
    int32_t mF44;                                   // 0x44
    int32_t mF48;                                   // 0x48
    TbDynamicStringTemplate<char16_t> mText4C;      // 0x4c
    int32_t mF60;                                   // 0x60
    int32_t mF64;                                   // 0x64
    int32_t mF68;                                   // 0x68
};

// Unit v5662 (src/FS_Online_init_CFSOnlineSystemList_CompleteGetList.cpp): the ride instance, 0x10174410 (468).
// The ride record's virtual destructor (vtable +8, flag 1) goes through this hook (default: delete); tests replace it.
// The list's callback goes through gOnlineListCallbackCaller (declared above).
template<> void CFSOnlineSystemList<TbPtrList<CFSOnlineRide>, ccRideList>::CompleteGetList(int32_t error);
extern void (*gRideRecordDeleter)(CFSOnlineRide* ride);

// Unit v51123 (src/FS_Online_init_CFSOnlineSystem_GotoPlayerByIDFTPPostLoginCallba.cpp): the park instance, 0x101741d4 (468); the server
// (0x1017464c) and theme (0x1017488c) instances are declared above. Their record's virtual destructor (vtable +8, flag 1) goes through
// these hooks (default: delete); tests replace them. The park and theme list heads are removed by the weak ParkRemoveHead /
// ThemeRemoveHead (0x10177b98, 0x10177904), which tests replace.
template<> void CFSOnlineSystemList<TbPtrList<CFSOnlinePark>, ccParkList>::CompleteGetList(int32_t error);
extern void (*gParkRecordDeleter)(CFSOnlinePark* park);
extern void (*gServerRecordDeleter)(CFSOnlineServer* server);
extern void (*gThemeRecordDeleter)(CFSOnlineTheme* theme);
CFSOnlinePark* ParkRemoveHead(TbPtrList<CFSOnlinePark>* list);
CFSOnlineTheme* ThemeRemoveHead(TbPtrList<CFSOnlineTheme>* list);

// Unit v5670: one theme of the online theme list (0x48 bytes on the Mac). Names invented.
class CFSOnlineTheme {
public:
    CFSOnlineTheme(const ccThemeInfo* info, CFSOnlineSystemThemeList* list);   // 0x1017c57c: adds itself to the list's tail
    virtual ~CFSOnlineTheme();   // 0x1017c6e0: leaves the list

    CFSOnlineSystemThemeList* mList;                // 0x04
    int32_t mThemeID;                               // 0x08
    TbDynamicStringTemplate<char> mText0C;          // 0x0c
    TbDynamicStringTemplate<char> mText20;          // 0x20
    TbDynamicStringTemplate<char> mText34;          // 0x34
};

// Unit v51065 (src/FS_Online_CFSOnlineSystemParkList_InWorldByPlayerName_Dtor_CFSOnlineSystemParkList_InCity_GetList_CFSOnlineSystemCityList_StoreReceivedList_CFSOnlineSystemCityList_ReturnCityInfo.cpp).
template<> void CFSOnlineSystemList<TbPtrList<CFSOnlinePark>, ccParkList>::LoggedInGetList();           // 0x10178ce4 (412)
template<> void CFSOnlineSystemList<TbPtrList<CFSOnlinePark>, ccParkList>::CompleteGetList(int32_t error);   // 0x101741d4 (468, untranslated)
// 0x10179b08 (288), the theme instance's out-of-line base destructor. The C++ destructor of that instance stays the empty one of unit
// v51129 (src/ui_Postcard_panel_std_basic_string_assign_2.cpp), because ~CFSOnlineSystemThemeList does the same work inline and C++
// chains to the base; the Mac's copy is this function, which a deleting call (flag above 0) follows with the free.
namespace online_list_base {
void ThemeListBaseDtor(CFSOnlineSystemList<TbPtrList<CFSOnlineTheme>, ccThemeList>* self);
}
// std::allocator<unsigned short>::destroy, 0x1016e988 (4): blr; neither argument is read.
void AllocatorUShortDestroy(void* allocator, uint16_t* object);
