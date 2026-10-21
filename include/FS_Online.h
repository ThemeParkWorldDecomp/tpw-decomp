// FS_Online.cpp (main app): the online system and the game type. PARTIAL; mostly
// untranslated (the OnlineConfig constructor is in src/game/online/FS_Online.cpp).
#pragma once
#include <cstdint>
#include <string>

#include "FS_OnlineList.h"
#include "TbDynamicStringTemplate.h"
#include "TbTList.h"

// The park GetFTPDownloadParkInfo describes (name and layout invented; only the theme is
// read so far).
struct CFSFTPParkInfo {
    uint8_t mF00[0x4c];
    int32_t mTheme;          // 0x4c  index into the online theme list
};

// The online library's connection classes (imports of the "online" shared library; main has none of their functions). Only the calls
// the system makes are declared: Release, and the destructor called without a delete (the library owns the storage).
namespace NS_MacDoze { struct _GUID; }
class Auth {
public:
    void Release();   // Release__4AuthFv
    ~Auth();          // __dt__4AuthFv
    // Unit v5924 (imports of the online library; the Mac object is 0xc bytes, made by operator new(0xc)).
    Auth();           // __ct__4AuthFv
    int32_t Connect(char* server, char* unused, uint16_t port, void (*callback)(int32_t, int32_t, void*, void*));   // Connect__4AuthFPcPcUsPFiiPvPv_v
    void Login(uint16_t language, char16_t* name, char16_t* password, void* context);   // Login__4AuthFUsPwPwPv
};
// News, City and Upload each keep a queue of requests: GetCurrentMessage(0) answers the id of the request being worked on (0 when none),
// GetCurrentMessage(1) the id of the one waiting; MonitorMessage gives a request's progress (words done and total).
class News {
public:
    void Release();   // Release__4NewsFv
    ~News();          // __dt__4NewsFv
    uint32_t GetCurrentMessage(int32_t which);                                 // GetCurrentMessage__4NewsFi
    void MonitorMessage(uint32_t id, int32_t* done, int32_t* total);           // MonitorMessage__4NewsFUlPiPi
    // Unit v5924 (imports of the online library; the Mac object is 0xc bytes, made by operator new(0xc)). The Get calls answer a request id.
    News();           // __ct__4NewsFv
    int32_t Connect(char* server, char* unused, uint16_t port, void (*callback)(int32_t, int32_t, void*, void*));   // Connect__4NewsFPcPcUsPFiiPvPv_v
    uint32_t GetGameServerDetails(uint16_t language, NS_MacDoze::_GUID* guid, void* context);   // GetGameServerDetails__4NewsFUsPQ210NS_MacDoze5_GUIDPv
    uint32_t GetGameNews(uint16_t language, void* guid, void* context);        // GetGameNews__4NewsFUsPvPv
    uint32_t GetServerNews(uint16_t language, void* context);                  // GetServerNews__4NewsFUsPv
};
class City {
public:
    City();           // __ct__4CityFv (unit v5043; the Mac object is 0xc bytes, made by operator new(0xc))
    // Unit v5043 (imports of the online library).
    int32_t Connect(char* server, char* unused, uint16_t port, void (*callback)(int32_t, int32_t, void*, void*));   // Connect__4CityFPcPcUsPFiiPvPv_v
    int32_t GetParkInCityList(void* context, int32_t city, uint32_t arg, int32_t flag);   // GetParkInCityList__4CityFPviUli (import, unit v5532)
    int32_t GetCityList(void* context, uint32_t id, int32_t flag);   // GetCityList__4CityFPvUli (unit v5307, an import)
    void SetSessionInfo(uint16_t language, int32_t a, int32_t b, char16_t* password);   // SetSessionInfo__4CityFUsiiPw
    void UnpublishPark(void* context, int32_t park, int32_t user);   // UnpublishPark__4CityFPvii
    void Release();   // Release__4CityFv
    ~City();          // __dt__4CityFv
    uint32_t GetCurrentMessage(int32_t which);                                 // GetCurrentMessage__4CityFi
    void MonitorMessage(uint32_t id, int32_t* done, int32_t* total);           // MonitorMessage__4CityFUlPiPi
    void MarkParkVisited(void* context, int32_t park, int32_t arg);            // MarkParkVisited__4CityFPvii (unit v5655, an import)
    int32_t GetRideList(void* context, uint32_t id, int32_t flag);             // GetRideList__4CityFPvUli (unit v5718, an import)
    // Unit v5762 (an import, glue 0x102392b0)
    int32_t GetParkInWorldByName(void* context, char16_t* name, uint32_t arg, int32_t flag);   // GetParkInWorldByName__4CityFPvPwUli
    int32_t GetLogicalServerList(void* context, int32_t a, uint32_t b, int32_t c);   // GetLogicalServerList__4CityFPviUli (unit v5713, an import, glue 0x10239340)
    // Unit v5928 (imports)
    int32_t GetThemeList(void* context, uint32_t arg, int32_t flag);              // glue 0x10239310
    int32_t GetParkByID(void* context, int32_t park, uint32_t arg, int32_t flag);   // glue 0x10239298
    int32_t GetParkInCityListTop10(void* context, int32_t city, uint32_t arg, int32_t flag);   // glue 0x102392e0
};
// Unit v5043: what UploadParkRequest is given (0x42e bytes on the Mac, built on LoggedInRequestUpload's stack; the offsets are the Mac's).
struct ccParkFileInfo {
    int32_t mF00;                  // 0x00  the system's word at 0x344
    uint8_t mThemeID;              // 0x04  the theme list's id of the theme
    uint8_t mFlag;                 // 0x05  the low byte of the system's word at 0x370
    char16_t mName[20];            // 0x06
    char16_t mDescription[256];    // 0x2e
    uint16_t mWords[256];          // 0x22e  copied from the system's 0x120
};
class Upload {
public:
    // Unit v5043 (imports of the online library).
    void UploadParkRequest(char* file, ccParkFileInfo* info, void* context);   // UploadParkRequest__6UploadFPcP14ccParkFileInfoPv
    void UploadParkFile(char* file, void* context);                            // UploadParkFile__6UploadFPcPv
    void Release();   // Release__6UploadFv
    uint32_t GetCurrentMessage(int32_t which);                                 // GetCurrentMessage__6UploadFi
    void MonitorMessage(uint32_t id, int32_t* done, int32_t* total);           // MonitorMessage__6UploadFUlPiPi
    // Unit v5935 (imports of the online library; the Mac object is 0xc bytes).
    Upload();                                                                  // __ct__6UploadFv, 0x1023a978
    int32_t Connect(char* server, char* unused, uint16_t port, void (*callback)(int32_t, int32_t, void*, void*));   // Connect__6UploadFPcPcUsPFiiPvPv_v, 0x1023a9a8
    void SetSessionInfo(int16_t language, int32_t a, int32_t b, char16_t* password);   // SetSessionInfo, 0x1023a9c0
};
// The FTP download object at CFSOnlineSystem + 0x3a0: its virtual destructor (slot +8 on the Mac, called with 1: the deleting form) and
// the two progress words TickFTPDownload reads (bytes expected at 0x18 and bytes received at 0x1c; the rest of the object is not
// known; the host's layout differs, the offsets are in the comments).
// Unit v5101: the Metrowerks std::string the FTP client keeps: one pointer to a shared representation (length at +0, data pointer at +0xc
// on the Mac; the host's pointer is 8 bytes, so it sits at +0x10).
struct FTPStringRep {
    int32_t mLength;
    int32_t mCapacity;
    int32_t mRefCount;
    const char* mData;
};
struct FTPString {
    FTPStringRep* mRep;
};
class CFTPControl;
class FTPClient {
public:
    virtual ~FTPClient();
    uint8_t mF04[0x18 - 0x04];   // 0x04
    int32_t mTotal;              // 0x18  size of the download
    int32_t mDone;               // 0x1c  received so far

    // Unit v5101 (src/game/online/FTPClient.cpp). The Mac object is 0x2773 bytes and derives from Connection/CSock; the host's layout
    // differs (the vtable pointer is 8 bytes), so these members keep the Mac offsets in the comments only.
    int32_t RecvData(bool save, int32_t count);         // 0x1020b59c: 0 ok, 1 file not opened, 2 recv failed, 3 accept failed
    FTPClient* Destruct(int16_t deleteFlag);            // 0x1020b46c: the body of the destructor (deleting form when the flag is > 0)
    int32_t Connect(char* a, char* b, char* c, char* server);   // 0x1020b730
    int32_t _EnterPassiveMode();                        // 0x1020b904: 0 ok, 0x38/0x34/0x43 errors
    int32_t _Connect();                                 // 0x1020bbe8 (unit v5227, src/game/online/FTPClient.cpp)
    int32_t _Download();                                // 0x1020c228 (unit v5227): 0 ok, 0x38 closed, 0x34/0x40 socket, 0x43/0x44 answers
    static void* _threadfn(void* self);                 // 0x1020c860 (untranslated; a fake in the test)

    uint8_t mBuffer[0x2000];     // 0x424
    int32_t mDataSocket;         // 0x2424
    int32_t mListenSocket;       // 0x2428
    uint16_t mFamily;            // 0x242c  the sockaddr_in (16 bytes at 0x242c): family 2
    uint16_t mPort;              // 0x242e
    union {
        uint32_t mIpWord;        // 0x2430
        uint8_t mIp[4];
    };
    uint8_t mSockZero[8];        // 0x2434
    char mStrings[0x120];       // 0x243c  three 0x60 byte copies of Connect's arguments (0x243c, 0x249c, 0x24fc)
    char mPath[0x204];           // 0x255c  the local file name
    FTPString mServer;           // 0x2760  the MSL std::string (one pointer)
    uint8_t mCondA[4];           // 0x2764  bthread_cond
    uint8_t mCondB[4];           // 0x2768  bthread_cond
    int32_t mF276c;              // 0x276c
    uint8_t mReady;              // 0x2770
    uint8_t mThreadStarted;      // 0x2771
    uint8_t mThreaded;           // 0x2772

    // Unit v5273 (src/game/online/FTPClient.cpp). Construct is the body of the constructor (the vptr is stored by the
    // caller's construction here); the data member keeps the end of the class so no earlier member moves.
    FTPClient* Construct(CFTPControl* control, bool threaded);   // 0x1020b368 (208)
    int32_t Disconnect();                                        // 0x1020beb8 (176)
    int32_t _Disconnect();                                       // 0x1020bf94 (164): 0 ok, 0x38 closed, 0x43 answer
    int32_t Download(char* remote, char* local);                 // 0x1020c064 (228)
    void _CloseDSocket(int32_t& sock);                           // 0x1020c174 (132)
    int32_t ParseFileSize(char* line);                           // 0x1020c744 (236)
    CFTPControl* mControl;       // 0x275c
    // Unit v51009: the worker thread's loop (0x1020c8cc, 388): waits on the first condition, runs the state in mF276c (0 stop, 1 connect,
    // 2 quit, 3 download, others nothing) and reports the state and its result to the control.
    void ProcessEvents();
};
namespace ftp_callee {
void ControlCallback(CFTPControl* control, int32_t state, int32_t result);   // virtual slot +8 of the control through __ptr_glue 0x1023baa8
}

// Unit v5101: the callees of FTPClient's members (the connection base class, the socket and C library imports, the thread library); all
// are fakes in the test library. Addresses are the Mac's.
namespace ftp_callee {
void SendCommandLine(FTPClient* self, const char* line);                    // 0x10201b14
int32_t RecvResponse(FTPClient* self, char* buffer, int32_t size, bool flag);   // 0x10201bb4
uint32_t GetLocalIPAddress(FTPClient* self);                                // 0x10201d80 (a word whose bytes are in network order)
int32_t Accept(int32_t sock, void* addr, int32_t* length);                  // 0x1023a840
void* Fopen(const char* path, const char* mode);                            // 0x102389b0
int32_t Recv(int32_t sock, void* buffer, int32_t size, int32_t flags);      // 0x1023a780
void Fwrite(const void* buffer, int32_t size, int32_t count, void* file);   // 0x1023a5e8
void Fclose(void* file);                                                    // 0x102388c0
int32_t Socket(int32_t family, int32_t type, int32_t protocol);             // 0x1023a708
uint32_t InetAddr(const char* text);                                        // 0x1023a720 (network order bytes)
int32_t Bind(int32_t sock, const void* addr, int32_t length);               // 0x1023a738
int32_t Listen(int32_t sock, int32_t backlog);                              // 0x1023a828
int32_t GetSockName(int32_t sock, void* addr, int32_t* length);             // 0x1023a810
void Sleep(uint32_t ms);                                                    // 0x1023a888
void CondSignal(void* cond);                                                // 0x10200fb4
void CondWait(void* cond, int32_t timeout);                                 // 0x10200f30
void CondDtor(void* cond, int16_t flag);                                    // 0x10200ea8
void WSACleanup();                                                          // 0x102386f8
void StringDtor(void* str, int16_t flag);                                   // 0x1000221c
void StringAssign(void* str, const char* text, uint32_t length);            // 0x101b2440
void CSockDtor(FTPClient* self, int16_t flag);                              // 0x10201fcc
void OperatorDelete(void* p);                                               // 0x10238218
int32_t BthreadCreate(void* (*fn)(void*), FTPClient* arg);                  // 0x10200c34
}  // namespace ftp_callee

// ---- Unit v5917: std::__tree<unsigned short> (a std::set<unsigned short>'s tree: find 0x1016d1f8, insert_one 0x1016e9c4, insert_node_at
// 0x1016f164), IPSystem::Startup 0x1016f3c8 / Shutdown 0x1016f448 and ccParkInfo::~ccParkInfo (FS_OnlineList.h).
// Mac words are 32 bits (offsets in comments), the host's are native. The end node is the tree +4 (the root word is its left link).
extern uint8_t gTreeInsertedTrue;    // 0x102a7230 (TOC -0x44e4): the byte insert_one answers for a new element, 1
extern uint8_t gTreeInsertedFalse;   // 0x102a7231 (TOC -0x44e8): the byte it answers for an existing one, 0
struct UshortSetTree {
    struct node {
        node* mLeft;         // 0x00
        node* mRight;        // 0x04
        uintptr_t mParent;   // 0x08  parent with the colour in bit 0
        uint16_t mValue;     // 0x0c  (the Mac node is 0xe bytes)
    };
    struct Iterator { node* mNode; };                          // 0x00
    struct InsertResult { node* mNode; uint8_t mInserted; };   // 0x00, 0x04

    uint32_t mCount;   // 0x00
    node* mRoot;       // 0x04  the end node's left word
    node* mBegin;      // 0x08  the first node

    node* end() { return reinterpret_cast<node*>(&mRoot); }
    Iterator find(const uint16_t& key);                     // 0x1016d1f8
    InsertResult insert_one(const uint16_t& key);           // 0x1016e9c4
    node* insert_node_at(node* parent, bool left, bool leftmost, const uint16_t& value);   // 0x1016f164
    // Unit v51125 (src/game/MslSupport.cpp): 0x1016e65c (448): frees the subtree under `subtree` and `subtree` itself.
    void destroy(node* subtree);
};
namespace v5917_callee {
void BalanceInsert(UshortSetTree::node* node, UshortSetTree::node* root);   // 0x10153b24 __red_black_tree::balance_insert
int32_t WSAStartup(int32_t version, void* data);   // 0x102394f0
int32_t WSACleanup();                              // 0x102386f8
void Free(void* p);                                // 0x10238a70
void OperatorDelete(void* p);                      // 0x10238218
}  // namespace v5917_callee
class IPBase {
public:
    void ShowError();   // 0x1020cac8 (stand-in elsewhere)
};
// Unit v51009 (defined in src/game/online/IPBase.cpp): ShowError's tables, Mac pointer words (original names unknown).
extern const int16_t* gIPErrorCodes;         // 0x10396d1c: the int16 error numbers, ended by 0 (a native pointer here, a word on the Mac)
extern const char* const* gIPErrorTexts;     // 0x10396d18: the texts, one per number (the ending 0 has one too; the Mac's entries are words)
namespace ip_callee {
int32_t WSAGetLastError();   // glue 0x1023b0b0 (weak default 0)
}
class IPSystem : public IPBase {
public:
    bool Startup();    // 0x1016f3c8: WSAStartup(0x101); true (after ShowError) when it failed
    bool Shutdown();   // 0x1016f448: WSACleanup(); true (after ShowError) when it failed
};

// What the login and city jobs call back when they finish: the error (0 for success) and the caller's context word. On the Mac the
// pointer at CFSOnlineSystem + 4 is a function pointer (called through __ptr_glue, r3 = error, r4 = context).
typedef void (*OnlineLoginCallback)(int32_t error, void* context);
class TbITable;
// The interface's progress bar and the panels the online jobs raise (all untranslated; the progress bar quartet touches the interface
// classes deeply, so the tickers call them as fakes in the tests).
void CreateUIProgressBar(int32_t text);            // 0x101e01f0 (120): shows the bar with the interface string `text`
void UpdateUIProgressBar(int32_t percent);         // 0x101e0300 (92)
void UpdateParkFindPanelProgress(int32_t percent); // 0x101ea4e4 (192): the park find panel's own bar
void AddUIError(int32_t error, TbITable* table);   // 0x101f1df0: the interface's error dialog for an online error id
void RemoveOnlineLoginPanel(int32_t arg);          // 0x101e2be4

// The online system (0x404 bytes on the Mac; gOnlineSystem is the one instance). The host's layout differs (64-bit pointers, strings
// with vtables): the offsets are in the comments, and tests compare members. The strings are in anonymous unions: the constructor
// makes them (placement new), and the many test libraries that fake the constructor need no string library. Member names are invented
// except mThemeList, mF108 and mFTPParkPending (older units').
class CFSOnlineSystem {
public:
    int32_t CanWeSendEmail(); // 0x101717cc, untranslated
    CFSOnlineSystem();       // 0x1016f7d0: the strings, the connections cleared (InitLogin, News, City, Upload, FTP download), the nine lists made
    ~CFSOnlineSystem();      // 0x1017055c (nonvirtual)
    void Tick();             // 0x10170f80: the five tickers in order (login, news, city, upload, FTP download)
    CFSFTPParkInfo* GetFTPDownloadParkInfo();   // 0x10177654
    char* ReturnFTPDownloadLocalPath();         // 0x10177608
    void StopProcessing();   // 0x10171404: the lobby's island screens call it as they open; resets news, city, upload and FTP download, then removes the progress bar
    // 0x1017178c (8 bytes; unit w40's mail icon handler asks): the word at 0x60, nonzero when the player is logged in.
    int32_t AreWeLoggedIn();   // 0x1017178c (src/game/online/FS_Online.cpp, unit w69)

    // Back to the start state of one online job (src/game/online/FS_Online.cpp): each releases its connection first.
    void InitLogin();                       // 0x101719f8
    void ReleaseLoginConnection();          // 0x10171b68
    void InitNews();                        // 0x10172238
    void ReleaseNewsConnection();           // 0x1017249c
    void InitCity();                        // 0x101735cc: also the clock word, the wanted flag, and Reset on all nine lists
    void ReleaseCityServerConnection();     // 0x10174ac8
    void InitUpload();                      // 0x10175a18
    void ReleaseUploadServerConnection();   // 0x1017598c
    void InitFTPDownload();                 // 0x10176cec
    void ReleaseFTPDownloadServerConnection();   // 0x101770dc
    void TickLogin();                       // 0x10171adc: when connected and the login finished, CompleteLogin(0) and the flag cleared
    // The login job's end (src/game/online/FS_Online.cpp, unit w69): clears a pending error id, then either finishes the login (error 0) or reports
    // the error to the caller's callback and puts the login words back.
    void CompleteLogin(int32_t result);     // 0x10171c08 (444)
    // The state tickers (src/game/online/FS_Online.cpp, unit w69): each follows its job's progress bar and completes the job when it is done.
    void TickNews();           // 0x1017232c (320)
    void TickCity();           // 0x10173afc (1132)
    void TickUpload();         // 0x10175b00 (236)
    void TickFTPDownload();    // 0x10176da4 (180)
    // The job completions the tickers call (untranslated).
    void CompleteNews();                    // 0x1017253c (648)
    void LoggedInGetNews();                 // 0x10171eb8 (320)
    void CompleteUpload(int32_t failed);    // 0x10175c20 (228): 1 when the upload word at 0x32c was 0
    // Unit v5043 (src/game/online/FS_Online.cpp).
    void UnpublishPark(int32_t park);                       // 0x10174ca0 (372)
    void RequestUpload(const char* file, int32_t f344, const char16_t* name, const char16_t* description, int32_t f370,
                       void (*callback)(int32_t, const void*), const void* context);   // 0x10174fe8 (288)
    void LoggedInRequestUpload();                           // 0x10175220 (380)
    void UploadServerConnectionCallback(int32_t result, int32_t unused, void* unused2, void* context);   // 0x101753dc (480)
    TbDynamicStringTemplate<char16_t> ReturnUploadFilename();   // 0x10175608 (260): <users path with theme>:<separator><tail><extension>
    // Untranslated callees of the unit.
    int32_t ConnectUploadServer();                          // 0x10175808 (328)
    void RequestUploadPostLoginCallback(int32_t result, const void* context);   // 0x10175154 (128)
    void CompleteFTPDownload(int32_t failed);   // 0x10176e90 (528): 1 when the download word at 0x394 was 0
    // Unit v5063 (untranslated, a fake in the test): starts a login; the callback is called with the error and the context.
    void Login(void (*callback)(int32_t, const void*), const void* context);   // 0x101710ac (48)
    // Unit v5100 (stand-in, a fake in the test): 0x10171234 (304), starts the login with the name and the password.
    void DoLogin(const char16_t* name, const char16_t* password);
    // Unit v5113 (stand-ins, fakes in the test).
    void LoginRASCancelled();                                           // 0x10171114 (40)
    void LoginConnectionCallback(int32_t a, int32_t b, void* c, void* d);   // 0x101715a0 (392)
    // Unit v5924 (src/game/online/FS_Online.cpp): the news job's callback.
    void NewsConnectionCallback(int32_t a, int32_t b, void* c, void* d);   // 0x10172030 (452)
    // Unit v5271 (stand-in, a fake in the test): 0x101727f8 (2252).
    void CityServerConnectionCallback(int32_t a, int32_t b, void* c, void* d);
    // Unit v5130.
    void EnterChatSatellite(int32_t index);                                                   // 0x10174be8 (124)
    void SetPublishing(int32_t publishing, const char* text, const uint16_t* words);          // 0x10174e4c (164)
    void RequestFTPDownload(const CFSOnlinePark& park, void (*callback)(int32_t, const void*), const void* context);   // 0x10175dfc (208)
    void GotoPlayerRequestFTPDownloadByID(int32_t parkID, void (*callback)(int32_t, const void*), const void* context);   // 0x10176074 (184)
    void GotoPlayerFTPPostGetParkInfoCallback(int32_t result, const void* context, const CFSOnlinePark* park);   // 0x101762e8 (72)
    int32_t AreWeProcessing();                                                                // 0x10171498 (208)
    // Callees of the unit (stand-ins, fakes in the test).
    void Logout();                                                                            // 0x10171398 (60)
    void RequestFTPPostLoginCallback(int32_t result, const void* context);                    // 0x10175f24 (64)
    void GotoPlayerByIDFTPPostLoginCallback(int32_t result, const void* context);             // 0x10176180 (60)
    void LoggedInRequestFTPDownload(const CFSOnlinePark* park);                               // 0x10176418 (2084)
    // Unit v5215 (stand-ins, fakes in the test).
    City* ReturnConnectionToCityServer();                                                     // 0x1017346c (284)
    static CFSOnlineSystem* Instance();                                                       // 0x10178618 (100)
    // Unit v5307 (stand-in, a fake in the test): 0x10171980 (8).
    uint32_t GetChildLock();
    // Unit v51110: 0x101719bc (8), the word at 0x5c (inside mF54).
    uint32_t GetAvatarID();
    // Unit v5655 (src/game/online/FS_Online.cpp): the FTP download job's callback and next-file request.
    void FTPDownloadCallbackFn(int32_t state, int32_t error);   // 0x101771a4 (264)
    void RequestNextFileForFTPDownload();                       // 0x101772ec (724)

    Auth* mLogin;                              // 0x00  the login connection, 0 when none
    OnlineLoginCallback mLoginCallback;         // 0x04  called with the error when the login fails; cleared when it ends
    void* mLoginContext;                        // 0x08  its second argument (a 32-bit word on the Mac)
    union { TbDynamicStringTemplate<char16_t> mLoginName; };        // 0x0c
    union { TbDynamicStringTemplate<char16_t> mLoginPassword; };    // 0x20
    int32_t mF34;                               // 0x34
    int32_t mF38;                               // 0x38
    union { TbDynamicStringTemplate<char16_t> mLoginEmail; };       // 0x3c
    int32_t mF50;                               // 0x50
    uint8_t mF54[0x60 - 0x54];                  // 0x54
    int32_t mLoggedIn;                          // 0x60  AreWeLoggedIn's word
    int32_t mLoginDone;                         // 0x64  set when the login answered; TickLogin completes it
    int32_t mLoginErrorID;                      // 0x68  an interface error string id waiting to be shown (CompleteLogin)
    News* mNews;                                // 0x6c  the news connection, 0 when none
    int32_t mF70;                               // 0x70
    uint32_t mNewsRequest1;                     // 0x74  id of the first news request (-1 none)
    int32_t mNewsDone1;                         // 0x78  nonzero once it was answered
    union { TbDynamicStringTemplate<char> mNewsText; };             // 0x7c
    uint32_t mNewsRequest2;                     // 0x90  id of the second news request (-1 none)
    int32_t mNewsDone2;                         // 0x94
    union { TbDynamicStringTemplate<char16_t> mF98; };              // 0x98
    uint32_t mNewsRequest3;                     // 0xac  id of the third news request (-1 none)
    int32_t mNewsDone3;                         // 0xb0
    union { TbDynamicStringTemplate<char16_t> mFB4; };              // 0xb4
    OnlineLoginCallback mLoginCallbackKept;     // 0xc8  CompleteLogin keeps the callback and its context here once the login worked
    void* mLoginContextKept;                    // 0xcc
    int32_t mNewsFinished;                      // 0xd0  nonzero: the news job is done (TickNews completes it)
    City* mCity;                                // 0xd4  the city server connection, 0 when none
    uint32_t mCityClock;                        // 0xd8  LbTime_GetClock at the last InitCity
    int32_t mCityListsWanted;                   // 0xdc  1 after InitCity
    CFSOnlineSystemCityList* mCityList;                             // 0xe0
    CFSOnlineSystemParkList_InCityTop10* mParksTop10;               // 0xe4
    CFSOnlineSystemParkList_InCity* mParksInCity;                   // 0xe8
    CFSOnlineSystemParkList_InWorldByPlayerName* mParksByPlayer;    // 0xec
    CFSOnlineSystemParkList_InWorldByParkID* mParksByID;            // 0xf0
    CFSOnlineSystemParkList_InCity* mParksInCity2;                  // 0xf4  a second InCity list
    CFSOnlineSystemThemeList* mThemeList;       // 0xf8
    CFSOnlineSystemServerList* mServerList;     // 0xfc
    CFSOnlineSystemRideList* mRideList;         // 0x100
    Upload* mUpload;                            // 0x104  the upload connection, 0 when none
    int32_t mF108;                              // 0x108  nonzero: the lobby opens on its screen at 0x10
    union { TbDynamicStringTemplate<char> mF10C; };                 // 0x10c
    uint8_t mF120[0x320 - 0x120];               // 0x120
    int32_t mF320;                              // 0x320
    int32_t mUploadActive;                      // 0x324  an upload is running (TickUpload follows it)
    int32_t mUploadDone;                        // 0x328  nonzero: it finished
    int32_t mUploadFailed;                      // 0x32c  its result word (0 means the upload failed)
    union { TbDynamicStringTemplate<char> mF330; };                 // 0x330
    int32_t mF344;                              // 0x344
    union { TbDynamicStringTemplate<char16_t> mF348; };             // 0x348
    union { TbDynamicStringTemplate<char16_t> mF35C; };             // 0x35c
    int32_t mF370;                              // 0x370
    int32_t mF374;                              // 0x374
    int32_t mF378;                              // 0x378
    int32_t mF37C;                              // 0x37c
    int32_t mF380;                              // 0x380
    int32_t mF384;                              // 0x384
    int32_t mF388;                              // 0x388
    int32_t mFTPActive;                         // 0x38c  a download is running (TickFTPDownload follows it)
    int32_t mFTPDone;                           // 0x390  nonzero: it finished
    int32_t mFTPResult;                         // 0x394  its result word (0 means it failed)
    int32_t mF398;                              // 0x398
    void* mF39C;                                // 0x39c  freed (operator delete) by ReleaseFTPDownloadServerConnection
    FTPClient* mFTP;                            // 0x3a0  the FTP download connection, deleted by ReleaseFTPDownloadServerConnection
    CFSOnlinePark* mFTPPark;                    // 0x3a4  made by the constructor
    int32_t mF3A8;                              // 0x3a8
    union { TbDynamicStringTemplate<char> mF3AC; };                 // 0x3ac
    int32_t mF3C0;                              // 0x3c0
    int32_t mF3C4;                              // 0x3c4
    union { TbPtrList<TbDynamicStringTemplate<char16_t>> mF3C8; };  // 0x3c8  (0x10 bytes; destroyed in order by ~CFSOnlineSystem)
    uint8_t mF3D8[4];                           // 0x3d8
    union { TbDynamicStringTemplate<char> mF3DC; };                 // 0x3dc
    int32_t mFTPParkPending;                    // 0x3f0  a downloaded park waits to be played
    uint8_t mF3F4[0x404 - 0x3f4];               // 0x404 bytes on the Mac

    // Unit v51123 (src/game/online/FS_Online.cpp): the login getters read the strings' data words.
    const char16_t* GetLoginCustomerName();   // 0x10171834: the word at 0x14 (mLoginName + 8)
    const char16_t* GetLoginPassword();       // 0x10171878: the word at 0x28
    const char16_t* GetLoginEmailAddress();   // 0x101718b8: the word at 0x44
    int32_t GetLoginPlayerID();               // 0x101718fc: mF34
    int32_t GetLoginCustomerID();             // 0x1017193c: mF38
    void LoggedInGotoPlayerRequestFTPDownloadByID();   // 0x10176390 (56)
};

// Unit v51123: std::__tree<unsigned short> helpers (the Mac words: the header words at 0, 4 and 8; the pointers are native here).
namespace macstd {
struct LessUShort {};
struct AllocUShort {};
extern void* gTreeUShortWord;   // the word at 0x102a722c
struct TreeUShort {             // 0x1016eaf0
    TreeUShort(const LessUShort& less, const AllocUShort& alloc);
    void* mF00;   // 0x00  copy of the global word
    void* mF04;   // 0x04  cleared
    void* mF08;   // 0x08  points at mF04
};
struct AllocUShortNode {
    void deallocate(void* node, unsigned long count);   // 0x1016e8b0: operator delete(node)
};
}  // namespace macstd
extern CFSOnlineSystem* gOnlineSystem;   // 0x10380ab0, original name unknown

// Unit v5043. The context the upload server hands to UploadServerConnectionCallback (answer 0x50): the Mac's words are a buffer pointer
// at 0, an unknown word at 4 and a use count at 8 (the host's pointer is 8 bytes, so the count is at 8 on both).
struct OnlineUploadContext {
    void* mBuffer;           // 0x00  freed with the context
    int32_t mRefCount;       // 0x08
};
// A global wide string (0x10380ad4, original name unknown): ReturnUploadFilename appends it. Held in a union so that the program's
// static initialisation (the string constructor) is not this file's concern.
struct OnlineWideStringGlobal {
    union { TbDynamicStringTemplate<char16_t> mString; };
    OnlineWideStringGlobal() {}
    ~OnlineWideStringGlobal() {}
};
extern OnlineWideStringGlobal gUploadNameTail;   // 0x10380ad4
// The callbacks the system registers (untranslated; the Mac passes the addresses of their TVectors).
void CreateOnlineLoginPanel(void (*onFailure)());                 // 0x101e27bc (1012)
void FSOnlineLoginRASFailureCallback();                           // 0x1017100c
void FSOnlineLoginConnectionCallback(int32_t, int32_t, void*, void*);   // 0x10171178
void UploadPostLoginCallbackFn(int32_t error, void* context);     // 0x10174f2c
void FSOnlineCityServerConnectionCallback(int32_t, int32_t, void*, void*);   // 0x101733b0
// Unit v5961.
void PostLoginCallbackFn_City(int32_t error, const void* context);   // 0x101783a4 (60), untranslated: a fake in the test
void GotoPlayerFTPPostGetParkInfoCallbackFn(int32_t error, const void* context, TbPtrList<CFSOnlinePark>* list);   // 0x1017620c (124)
class CFSOnlineFTPControl {
public:
    void CallbackFn(int32_t state, int32_t error);   // 0x1017769c (132): `this` is not read
};
// Unit v5063 (untranslated): the park lists' login callback (the Mac passes the address of its TVector); the context is the list.
void PostLoginCallbackFn_Park(int32_t error, const void* context);   // 0x10178c74 (60)

// Unit v5935 (src/game/online/FS_Online.cpp): callees of ConnectUploadServer and LoggedInRequestFTPDownload
// that no unit has translated (fakes in the test). Addresses are the Mac's.
class CFSUserInfo;
namespace ftpreq_callee {
CFSOnlineServer* ReturnUploadServer(CFSOnlineSystemServerList* list);                       // 0x1017b134
void UploadConnectionCallback(int32_t, int32_t, void*, void*);                              // 0x101753dc (the TVector at 0x10267468)
CFSOnlineRide* ReturnRideInfo(CFSOnlineSystemRideList* list, int32_t theme, int32_t index);   // 0x1017bacc
TbDynamicStringTemplate<char16_t> GetFilePath(const CFSOnlineRide* ride);                   // 0x10176c90 (CFSOnlineRide::GetFilePath)
void UserInfoCtor(CFSUserInfo* self);                                                       // 0x1017de54 (CFSUserInfo::CFSUserInfo on raw storage)
}  // namespace ftpreq_callee

// Unit v5742: __sinit_FS_Online_cpp 0x10178308 (116).
void UIMain_DestroyWString(void* object);   // 0x103bcbb8, the wide string destructor glue
extern OnlineWideStringGlobal gOnlineSinitString2;   // 0x10380ab4 (original name unknown)
extern uint8_t gOnlineSinitNodeA[];   // 0x10380ae8, the exit registration node of gUploadNameTail (size unknown)
extern uint8_t gOnlineSinitNodeB[];   // 0x10380ac8, the exit registration node of gOnlineSinitString2 (size unknown)
void sinit_FS_Online_cpp();           // 0x10178308
// Unit v51110: __sinit_FS_OnlineList_cpp 0x1017ddb4 (116): two wide string globals made from the literals at 0x1024d840 and 0x1024d84e.
extern OnlineWideStringGlobal gOnlineListString1;   // 0x10380b14
extern OnlineWideStringGlobal gOnlineListString2;   // 0x10380af4
extern uint8_t gOnlineListNode1[];   // 0x10380b28, the exit registration node of gOnlineListString1 (size unknown)
extern uint8_t gOnlineListNode2[];   // 0x10380b08, the exit registration node of gOnlineListString2 (size unknown)
void sinit_FS_OnlineList_cpp();      // 0x1017ddb4
extern "C" void __register_global_object(void* object, void* destructor, void* node);   // runtime glue 0x10239940

// Unit v5100 (src/game/ui/ui_onlineCityInfoOverlay.cpp, src/game/ui/ui_onlineLogin.cpp): the online city info overlay and the login panel's message function.
class InterfaceWindow;
// A city record, only the two members the overlay reads (the Mac's offsets in the comments; the rest is not known).
class CFSOnlineCity {
public:
    CFSOnlineCity() = default;
    // Unit v5307 (stand-in, a fake in the test): 0x1017bcb4 (312); the Mac object is 0x68 bytes.
    CFSOnlineCity(const ccCityInfo* info, CFSOnlineSystemCityList* list);
    char16_t* mName;   // Mac 0x14  the city's name
    int32_t mValue;    // Mac 0x40  the number the overlay's second text formats (string 0x1ce, tag 0x1b)
    // Unit v5546: the flag's position on the globe.
    float mPosX;       // Mac 0x34
    float mPosY;       // Mac 0x38
    float mPosZ;       // Mac 0x3c
    int32_t mF60;      // Mac 0x60  (unit v5537) nonzero: the park info panel shows the short form
    char16_t* mFtpName;   // Mac 0x4c  (unit v5655, appended: the host's offset is further) the text the FTP download path appends
    // Unit v5994 (appended; the host's offsets are further than the Mac's). The three strings are raw storage that the constructor
    // builds in place, so this class stays trivially destructible for the code that does not know them.
    CFSOnlineSystemCityList* mList;   // Mac 0x04  the list the city belongs to
    int32_t mCityID;                  // Mac 0x08
    alignas(8) unsigned char mText0C[sizeof(TbDynamicStringTemplate<char16_t>)];   // Mac 0x0c
    alignas(8) unsigned char mText20[sizeof(TbDynamicStringTemplate<char16_t>)];   // Mac 0x20
    alignas(8) unsigned char mText44[sizeof(TbDynamicStringTemplate<char16_t>)];   // Mac 0x44
    int32_t mF58;                     // Mac 0x58
    int32_t mF5C;                     // Mac 0x5c
    int32_t mF64;                     // Mac 0x64
    // 0x1017be40 (196): the destructor's body (the vtable word is not kept): leaves the list, destroys the strings, and with a flag above 0
    // (the low 16 bits, signed) frees the object. The Mac checks this for NULL, a caller does that here.
    void Destruct(int16_t deleteFlag);
};
extern InterfaceWindow* gOnlineCityOverlay;            // 0x10395d04, the overlay window, NULL when none
extern const CFSOnlineCity* gOnlineCityOverlayShown;   // 0x10395d00, the city the overlay shows now
extern int32_t gOnlineLoginPanelFlag;                  // 0x10395d2c, cleared when the login panel gets message 0x14
extern char16_t gOnlineLoginPassword[33];              // 0x10395d30 (size from the next global)
extern char16_t gOnlineLoginName[33];                  // 0x10395d72 (size invented)
extern int16_t gCityInfoOverlayScript[];               // 0x102b0288, initialised data of the window script (not defined by this unit)
extern const char16_t kOnlineCityNoText[1];            // 0x1024ee5e, a literal of the original: the empty text
int32_t CityInfoOverlayMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);   // 0x101e1ee8, untranslated
void UpdateOnlineCityInfoOverlay(const CFSOnlineCity* city);          // 0x101e1f64 (328)
void CreateOnlineCityInfoOverlay();                                   // 0x101e20f0 (496)
int32_t OnlineLoginPanelDlgMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);   // 0x101e24c8 (496)

// The game type (0x1016f5d8..0x1016f79c; the inventory files it under FS_Online.cpp).
// SetGameType keeps the 0x800000 / 0x1000000 / 0x2000000 bits of gGameOptions in step.
enum GameType : int32_t {
    kGameTypeNormal = 0,     // names invented
    kGameTypeOnline = 1,
    kGameTypeEasy = 2,       // a master save "easy mode" user
};

class CGameType {
public:
    // 0x1016f5d8 (src/game/online/FS_Online.cpp, unit w40): the type is read back from the gGameOptions bits kGameOptionEasyGame,
    // kGameOptionOnlineGame and kGameOptionNormalGame (the first set one in that order wins; none set makes the normal game),
    // through SetGameType, which then rewrites all three bits.
    CGameType();
    // 0x1016f6a0: asserts the type is below 3 (a SIGNED test: -1 passes it; "Invalid GameType in SetGameType\n", the string at 0x1024ca4c), stores it WHATEVER it
    // is, and for 0, 1 or 2 sets that type's option bit and clears the other two; any other value leaves the option word alone.
    void SetGameType(GameType type);

    GameType mType;                     // 0x00
};

// The one game type object: a function-local static of an inline accessor in the
// original (0x102b3aa0, guard byte 0x102b3aa4), so every user inlines the guard test and
// constructs it on first use. The accessor's name is not known.
inline CGameType& TheGameType() {
    static CGameType sGameType;
    return sGameType;
}

// The online servers and ports, copied from COnlineBalance by SetInitialOnlineValues
// (TPMain.cpp). The strings are MSL std::strings (one pointer each on the Mac). The
// constructor (0x1016f4c0) is translated in FS_Online.cpp; the destructor is the implicit
// one (its out-of-line copy follows TPMain's static initialiser, 0x10003508).
struct OnlineConfig {
    OnlineConfig();            // 0x1016f4c0
    std::string mMailServer;   // 0x00
    std::string mChatServer;   // 0x04
    std::string mLoginServer;  // 0x08
    std::string mCityServer;   // 0x0c
    std::string mNewsServer;   // 0x10
    std::string mVoteServer;   // 0x14
    int16_t mMailPort;         // 0x18
    int16_t mChatPort;         // 0x1a
    int16_t mLoginPort;        // 0x1c
    int16_t mCityPort;         // 0x1e
    int16_t mNewsPort;         // 0x20
    int16_t mVotePort;         // 0x22, not set by the constructor
    std::string mComplainEmail;  // 0x24
};

// Unit v5368: OnlineConfig::~OnlineConfig (0x10003508, 892) over the Mac's object (string words, src/game/MslSupport.cpp).
namespace online_config_callee {
void OperatorDelete(void* p);   // 0x10238218
}
struct MslCharString;
namespace online_config_mac {
// `slots` is the object seen as an array of string words, Mac offset / 4 (native 8 byte pointers on the host): slot 0..5 and 9 are the strings.
MslCharString* Destroy(MslCharString* slots, int16_t flag);   // 0x10003508
}

// Unit v5130 (untranslated callees and the TVector callbacks the Mac passes by address).
class InterfaceWindow;
void CreateOnlineParkInfoPanel(const CFSOnlinePark* park, InterfaceWindow* parent, void (*onCancel)(),
                               void (*onDownload)(int32_t, const void*), void* context);   // 0x101eabb4 (3660)
void SatelliteOnlineParkInfoPanelCancelCallback();                      // 0x10174b6c
void GeneralParkDownloadCompleteCallback(int32_t, const void*);         // 0x101ea734
// Unit v5537 (src/game/ui/ui_onlineParkInfoPanel.cpp): the info panel's globals, its message function (untranslated; the Mac passes the address of
// its TVector at 0x10268588) and the two callees the panel calls that no header declared.
extern InterfaceWindow* gOnlineParkInfoPanel;                      // 0x103961e4, NULL when no panel is open
extern CFSOnlinePark gOnlineParkInfoPark;                          // 0x10396118, the copy of the park the panel shows
extern void (*gOnlineParkInfoCancel)();                            // 0x10396114
extern void (*gOnlineParkInfoDownload)(int32_t, const void*);      // 0x10396110
extern void* gOnlineParkInfoContext;                               // 0x1039610c
extern int16_t gOnlineParkInfoScript[];                            // 0x102b0e58, initialised data of the window script (not defined by this unit)
int32_t OnlineParkInfoPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);   // 0x101ea7f0, untranslated
namespace park_info_callee {
CFSOnlineCity* ReturnCityInfo(CFSOnlineSystemCityList* list, int32_t city);   // 0x10178b3c CFSOnlineSystemCityList::ReturnCityInfo
}
void RequestFTPPostLoginCallbackFn(int32_t error, void* context);       // 0x10175d3c
void GotoPlayerByIDFTPPostLoginCallbackFn(int32_t error, void* context);   // 0x10175fac

// Unit v5550 (src/game/ui/ui_onlineParkInfoPanel.cpp, src/game/ui/ui_onlineParkFindPanel.cpp): the park info panel's messages and
// the park find panel.
int32_t OnlineParkInfoPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);   // 0x101ea7f0 (896)
void VisitParkPostDownloadCallbackFn(int32_t error, const void* context);   // 0x101ea684 (116); the Mac passes its TVector at 0x10268598
void RemoveOnlineParkFindPanel();      // 0x101ea468 (76)
void RemoveOnlineParkInfoPanel();      // 0x101eba64 (untranslated)
void __sinit_ui_onlineParkFindPanel_cpp();   // 0x101ea5d8 (116)
extern InterfaceWindow* gOnlineParkFindPanel;           // 0x103960c8
extern InterfaceWindow* gOnlineParkInfoPanel;           // 0x103961e4 (forgotten by message 0x14)
extern void (*gOnlineParkInfoClose)();                  // 0x10396114: the function the info panel's caller gave (called on close)
extern const void* gVisitParkContext;                   // 0x1039610c
extern void (*gVisitParkPostDownload)(int32_t, const void*);   // 0x10396110
extern uint8_t gOnlineParkInfoParkStorage[];            // 0x10396118: the CFSOnlinePark the info panel shows (mParkID at 0x08 on the Mac)
extern uint8_t gOnlineParkFindStringA[];                // 0x103960cc (a wide string, 0x14 bytes on the Mac)
extern uint8_t gOnlineParkFindNodeA[];                  // 0x103960e0 (its __register_global_object record)
extern uint8_t gOnlineParkFindStringB[];                // 0x103960ec
extern uint8_t gOnlineParkFindNodeB[];                  // 0x10396100
// Unit v51037 (src/game/ui/ui_onlineParkInfoPanel.cpp, src/game/ui/ui_onlineParkListPanel.cpp): the static initialisers of ui_onlineParkInfoPanel.cpp (0x101ebae0)
// and ui_onlineParkListPanel.cpp (0x101ec90c); names invented. Strings are raw storage (the Mac's are 0x14 bytes), nodes the exit records.
void __sinit_ui_onlineParkInfoPanel_cpp();   // 0x101ebae0 (144)
void __sinit_ui_onlineParkListPanel_cpp();   // 0x101ec90c (116)
extern uint8_t gOnlineParkInfoStringA[];     // 0x10396208
extern uint8_t gOnlineParkInfoNodeA[];       // 0x1039621c
extern uint8_t gOnlineParkInfoStringB[];     // 0x103961e8
extern uint8_t gOnlineParkInfoNodeB[];       // 0x103961fc
extern uint8_t gOnlineParkInfoNodePark[];    // 0x103961d8
extern uint8_t gOnlineParkInfoParkDtor[];    // stands for the destructor word 0x10267550
extern uint8_t gOnlineParkListStringA[];     // 0x10396258
extern uint8_t gOnlineParkListNodeA[];       // 0x1039626c
extern uint8_t gOnlineParkListStringB[];     // 0x10396238
extern uint8_t gOnlineParkListNodeB[];       // 0x1039624c
// Virtual calls on lobby screens the host cannot make as the Mac does (weak definitions in src/game/ui/ui_onlineParkInfoPanel.cpp, fakes in the
// test library).
namespace parkinfo_callee {
void OnlineIslandsSlot30(void* screen);                      // slot 0x30 of the online islands screen (identity unknown; untranslated)
void DownloadGlobeSlot4c(void* screen);                      // slot 0x4c of the download globe screen (identity unknown; untranslated)
}  // namespace parkinfo_callee

// Unit v5227: the hostent as gethostbyname answers it (the host's native layout; on the Mac the address list pointer is at 0xc and the
// list holds pointers to the 4 byte addresses).
struct FTPHostent {
    char* mName;               // Mac 0x00
    char** mAliases;           // Mac 0x04
    int32_t mType;             // Mac 0x08
    int32_t mLength;           // Mac 0x0a
    uint32_t** mAddrList;      // Mac 0x0c
};
// More callees of FTPClient's members (fakes in the test library). Addresses are the Mac's.
namespace ftp_callee {
int32_t SockConnect(int32_t sock, const void* addr, int32_t length);        // 0x1023a768
FTPHostent* GetHostByName(const char* name);                                // 0x1023a750
void Shutdown(int32_t sock, int32_t how);                                   // 0x1023a7b0
void CloseSocket(int32_t sock);                                             // 0x1023a7c8
void SockClose(FTPClient* self);                                            // 0x1020e358 (Sock::Close)
void CloseDSocket(FTPClient* self, int32_t* sock);                          // 0x1020c174 (FTPClient::_CloseDSocket(int&))
int32_t ParseFileSize(FTPClient* self, char* text);                         // 0x1020c744
}  // namespace ftp_callee

// Unit v5962 (src/game/online/bthread.cpp, src/game/TPEmbed.cpp): bthread_cond, one word on the Mac (the event handle; a native pointer here).
namespace NS_MacDoze {
struct InternalHandle;
// CreateEvent(security, manualReset, initialState, name) glue 0x1023a8e8; WaitForSingleObject(handle, ms) glue 0x1023a6f0 (0: signalled).
InternalHandle* CreateEvent(void* security, int32_t manualReset, int32_t initialState, const char* name);
uint32_t WaitForSingleObject(InternalHandle* handle, uint32_t milliseconds);
int32_t CloseHandle(InternalHandle* handle);
}  // namespace NS_MacDoze
struct bthread_cond {
    NS_MacDoze::InternalHandle* mHandle;   // 0x00
    bthread_cond();                        // 0x10200e34 (72 bytes): CreateEvent(0, 0, 0, 0)
    // 0x10200ea8 (92 bytes), the destructor with CodeWarrior's hidden flag: `this` may be NULL; otherwise CloseHandle, and the storage is deleted when flag > 0.
    static bthread_cond* Destroy(bthread_cond* self, int16_t flag);
    bool wait(int32_t milliseconds);       // 0x10200f30 (88 bytes): a negative time waits forever; true when signalled
    int32_t signal();                      // 0x10200fb4 (44 bytes, unit v51056): SetEvent(mHandle); answers 0
};
// Unit v51056: glue imports of the thread functions and the small functions of TPEmbed.cpp (names invented).
namespace NS_MacDoze {
int32_t SetEvent(InternalHandle* handle);   // glue 0x1023a858
// glue 0x1023a870: answers the thread handle; the thread id goes to *id.
uint32_t _beginthreadex(void* security, uint32_t stackSize, void* (*start)(void*), void* arg, uint32_t flags, uint32_t* id);
}  // namespace NS_MacDoze
uint32_t bthread_create(void* (*start)(void*), void* arg);   // 0x10200c34 (64): _beginthreadex(0, 0, start, arg, 0, &id); answers id
void* operator_new(uint32_t size, void* where);              // 0x10201850 (8): placement new, answers `where`
void __sinit_TPEmbed_cpp();                                  // 0x10200bf0 (28)
extern uint32_t gTPEmbedWordA;   // 0x10396aa0 (set to 0x10000)
extern uint32_t gTPEmbedWordB;   // 0x10396aa4 (set to 0)
namespace bthread_callee { void OperatorDelete(void* p); }   // 0x10238218 (weak default in the source, a fake in the test)

// Unit v5273: more callees of FTPClient's members (fakes in the test library). Addresses are the Mac's.
namespace ftp_callee {
void ConnectionCtor(FTPClient* self);                                       // 0x10201a84 (Connection::Connection)
void StringCtor(void* str);                                                 // 0x1007363c (std::string constructor)
void CondCtor(void* cond);                                                  // 0x10200e34 (bthread_cond::bthread_cond)
int32_t WSAStartup(int32_t version, void* data);                            // 0x102394f0
char* Strncpy(char* dest, const char* src, int32_t n);                      // 0x10238500
int32_t Atoi(const char* text);                                             // 0x10238590
}  // namespace ftp_callee

// Unit v5655 (src/game/online/FS_Online.cpp, src/game/online/FS_OnlineList.cpp): the characters the FTP download paths append (the Mac's
// 0x102a72a4, 0x102a72a6, 0x102a72a8; one literal of slashes there) and two callees (weak defaults in the source, fakes in the test).
extern char16_t gFTPDownloadSlashA;   // 0x102a72a4
extern char16_t gFTPDownloadSlashB;   // 0x102a72a6
extern char16_t gFTPDownloadSlashC;   // 0x102a72a8
namespace ftpdl_callee {
CFSOnlineServer* ReturnFTPDownloadServer(CFSOnlineSystemServerList* list);   // 0x1017b0b4 CFSOnlineSystemServerList::ReturnFTPDownloadServer
TbDynamicStringTemplate<char16_t>* RemoveHead(TbPtrList<TbDynamicStringTemplate<char16_t>>* list);   // 0x10178114
}  // namespace ftpdl_callee

// Unit v5371 (src/game/online/Connection.cpp): the connection base class's receive buffer and local address.
// The Mac object has the vtable at 0; the host's class keeps a 4 byte stand-in there so the Mac offsets hold.
class Connection {
public:
    uint32_t mF00;               // 0x00  (the vptr on the Mac)
    int32_t mSocket;             // 0x04
    uint8_t mF08[0x18];          // 0x08
    char mBuf[0x400];            // 0x20  the receive buffer
    int32_t mLen;                // 0x420 bytes held in mBuf

    int32_t RecvResponse(char* out, int32_t size, bool flag);   // 0x10201bb4 (408): the reply code 100..599, or 0 when recv fails
    uint32_t GetLocalIPAddress();                               // 0x10201d80 (268): a word whose bytes are in network order
    // Unit v51015 (src/game/online/Connection.cpp)
    Connection();                          // 0x10201a84 (104): CSock::CSock, then the vptr word 0x102b1610, the words at 0x18, 0x1c and 0x420 cleared, mBuf cleared
    void SendCommandLine(char* line);      // 0x10201b14 (108): adds strlen to the word at 0x18, then CSock::SendData(line, strlen)
    // Unit v51056: ~Connection 0x10201ec0 with the hidden flag: NULL does nothing; else the vptr word 0x102b1610, ~CSock (flag 0), and the
    // storage is deleted when the flag is above 0. Answers this.
    static Connection* Destroy(Connection* self, int16_t flag);
};
namespace conn_callee {
int32_t Memmove(void* dest, const void* src, int32_t n);                    // 0x102386c8 (answers nothing used)
int32_t Gethostname(char* name, int32_t size);                              // 0x1023a7f8
}  // namespace conn_callee

// Unit v5567 (src/game/online/email.cpp): email.cpp's static initialiser. The strings are TbDynamicStringTemplate
// objects (0x14 bytes on the Mac, the host's own size here); the nodes are the __register_global_object records.
void __sinit_email_cpp();                              // 0x1020afc0 (856)
extern uint8_t gEmailLanguageStrings[];                // 0x10396af8: 16 narrow strings, "English" .. "American"
extern uint8_t gEmailAppNameString[];                  // 0x10396ad8: "Theme Park World"
extern uint8_t gEmailLanguageKeyString[];              // 0x10396ab8: "Language"
extern uint8_t gEmailWideString[];                     // 0x10396cf8: a wide string
extern uint8_t gEmailNodes[];                          // 12 bytes each: 0..15 the language strings' records (0x10396cec down to 0x10396c38), 16 the wide string's (0x10396d0c), 17 the app name's (0x10396aec), 18 the key's (0x10396acc)
extern uint8_t gEmailBlobNode[];                       // 0x10396aac
extern uint8_t gEmailBlobDtorVec[];                    // 0x102656b8 (the TVector for the blob's destructor)
extern uint8_t gEmailDefaultByte;                      // 0x102b1798
struct EmailBlob {
    int32_t mF00;                                      // 0x00
    int32_t mLength;                                   // 0x04
    int32_t mFlag;                                     // 0x08
    uint32_t mDataOffset;                              // 0x0c  the buffer (a Ptr32 offset on the host)
};
extern uint32_t gEmailBlob;                            // 0x10396aa8: Ptr32 to the EmailBlob

// Unit v5697 (src/game/ui/ui_progressBar.cpp): the progress bar window.
void CreateUIProgressBar(char16_t* text);   // 0x101dffd8 (492): makes the bar window (or sets the text of the one that exists)
int32_t ProgBarMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);   // 0x101dff64, untranslated (the Mac passes its TVector at 0x10268490)
extern int16_t gProgBarScript[];     // 0x102afe24, initialised data of the window script (not defined by this unit)
extern char gProgBarMeshName[];      // 0x1024eda3, "progress.wct" (not defined by this unit)
extern int32_t gUIProgressBarText;   // 0x10395c68, the string id the bar shows now (original name unknown)

// Unit v5726 (src/game/ui/ui_playerLogin.cpp): ui_playerLogin.cpp's static initialiser.
void __sinit_ui_playerLogin_cpp();                 // 0x101afa90 (116)
extern const char16_t gPlayerLoginStrings[];       // 0x1024e19e: strings at +0x14 and +0x22 (bytes)
extern uint8_t gPlayerLoginStringA[];              // 0x10394ae4: wide string (text at +0x14), node 0x10394af8
extern uint8_t gPlayerLoginStringB[];              // 0x10394ac4: wide string (text at +0x22), node 0x10394ad8
extern uint8_t gPlayerLoginNodeA[];                // 0x10394af8
extern uint8_t gPlayerLoginNodeB[];                // 0x10394ad8

// Unit v5924 (src/game/online/FS_Online.cpp).
extern NS_MacDoze::_GUID gOnlineNewsGuid;   // 0x102a7248 (the news requests' GUID; the Mac's first word is 0x5c200a41, the rest was not read)
void FSOnlineNewsConnectionCallback(int32_t, int32_t, void*, void*);   // 0x10171dfc (the TVector at 0x102674a0)
void CreateOnlineNewsPanel();                                          // 0x101e3264 (820)
// Unit v51117 (src/game/save/FS_MasterSave.cpp, src/game/online/FS_Online.cpp)
void __sinit_FS_MasterSave_cpp();                                      // 0x1016f35c (64)
void FSOnlineUploadServerConnectionCallback(int32_t, int32_t, void*, void*);   // 0x10175748 (124)
extern char16_t gMasterSaveWideText[];  // 0x1024c95e: the wide text is at +0x5f bytes
extern uint8_t gMasterSaveString[];            // 0x10380a90: the wide string object (names invented)
extern uint8_t gMasterSaveNode[];              // 0x10380aa4: its __register_global_object record
namespace online_callee {
void Free(void* p);   // 0x10238a70, the C library's free (weak in the source, a fake in the tests)
}

// Unit v5765 (src/game/online/IPBase.cpp): the socket base class.
struct Sock {
    uint32_t mVtable;     // 0x00 (the Mac's vtable word)
    int32_t mSocket;      // 0x04 the socket handle, -1 when closed
    void Close();         // 0x1020e358 (132): shutdown, drain with recv, closesocket
};

// Unit v51070 (src/game/online/FTPClient.cpp): small library functions on the Mac's layouts.
// Objects are Mac words (uint32_t, 32-bit arena addresses); lists are { size 0x0, sentinel prev 0x4, sentinel next 0x8 } with nodes
// { prev 0x0, next 0x4, data 0x8 }; a wide string is one word pointing to { size 0x0, ..., refs 0x8, data 0xc }.
extern uint8_t gIPSystemVtableBlock[16];       // 0x102b1938 (the objects hold the address of +8, 0x102b1940)
extern const char* const* gIPErrorTexts;       // 0x10396d18
extern const int16_t* gIPErrorCodes;           // 0x10396d1c
extern const int16_t gIPErrorCodesData[];      // 0x102b1804 (only its start is known: 0x2714, 0x2719)
extern const char* const gIPErrorTextsData[];  // 0x102b186c (not read)
extern char gSockVtblData[8];                  // 0x102b1954
namespace v51070_callee {                      // weak-free fakes in the test library; the program provides them elsewhere
void Delete(void* p);                          // operator delete
void DeleteArray(void* p);                     // operator delete[]
void* New(uint32_t size);                      // operator new
void* NewArray(uint32_t size);                 // operator new[]
uint32_t Strlen(const char* s);
void Strcpy(char* dest, const char* src);
void Throw(const char* type, void* object, const void* destructors);
void WStringDtor(void* string, int16_t flag);                                            // 0x101468f8
void CTokenListClear(void* list);                                                        // 0x102109fc
int32_t WCompare(const void* self, uint32_t pos, uint32_t n1, const void* s, uint32_t n2);   // 0x101d5864
int32_t NocaseCompare(const void* self, uint32_t pos, uint32_t n1, const void* s, uint32_t n2);   // 0x10225a5c
bool WLess(const void* a, const void* b);                                                // 0x10225e8c
bool NocaseLess(const void* a, const void* b);                                           // 0x10225d2c
void OstreamFlush(void* stream);                                                         // 0x102268f8
}
namespace v51070 {
void IPBaseCtor(uint32_t* self);                                         // 0x1020ca80
uint32_t* IPBaseDtor(uint32_t* self, int16_t flag);                      // 0x1020cb7c
uint32_t* EraseWideStrings(uint32_t* result, uint32_t* list, uint32_t first, uint32_t last);   // 0x1020db8c
void SockCtor(uint32_t* self);                                           // 0x1020e2a0
Sock* SockDtor(Sock* self, int16_t flag);                                // 0x1020e2d4
void StrListSplice(uint32_t* self, uint32_t pos, uint32_t* other);       // 0x1020abec
uint32_t* CTokenListDtor(uint32_t* self, int16_t flag);                  // 0x1020e560
uint32_t* CTokenDtor(uint32_t* self, int16_t flag);                      // 0x1020eeec
bool WStringEq(const uint32_t* a, const uint32_t* b);                    // 0x1021f130
bool NocaseWStringEq(const uint32_t* a, const uint32_t* b);              // 0x1021f254
uint8_t* OstreamSentryDtor(uint8_t* self, int16_t flag);                 // 0x1021f7b0 (the stream word sits unaligned at +2)
uint32_t IosTie(const uint8_t* ios);                                     // 0x1021f888
void WStringSetFind(uint32_t* result, const uint32_t* tree, const uint32_t* key);   // 0x1021fae4
void NocaseSetFind(uint32_t* result, const uint32_t* tree, const uint32_t* key);    // 0x1021fd4c
void StreambufSeekpos(uint32_t* result);                                 // 0x10220194
void StreambufSeekoff(uint32_t* result);                                 // 0x1022021c
uint32_t* IstreamSentryDtor(uint32_t* self, int16_t flag);               // 0x1022083c
uint32_t* ErrorDequeIterCopy(uint32_t* dst, const uint32_t* src);        // 0x101f5d6c
}
