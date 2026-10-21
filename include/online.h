// online.cpp (main app): COnlineControl, the online game client. PARTIAL; untranslated.
#pragma once
#include <cstdint>
#include <string>

class COnlinePerson;
struct IDXTYPEUniqueNameMapId;
struct IDXTYPEUniqueNameThingId;
struct MapPos;
struct nocase_string;   // opaque (unit v5285)
template<class Tag, class T> struct tag_IDX_TYPE;

class COnlineControl {
public:
    COnlineControl();            // 0x10213870
    ~COnlineControl();           // 0x10213dd4 (nonvirtual)
    void InitClient();           // 0x102143c4
    void InitGameWorld();        // 0x10214678
    void Shutdown();             // 0x10214840
    bool WantToShutDown();       // 0x102148bc
    void Update();               // 0x10214944
    void ProcessQueue();         // 0x10214c34
    // Unit w144 (untranslated): the avatar goes to a map cell / is moved there at once (CMMOnline's clicks). The cell id is passed by value
    // (the HIGH halfword of r4).
    void SetAvatarDestination(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell);   // 0x10218140 (the original answers whether SetDestination did; void here as other units' fakes define it)
    void TeleportAvatar(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell);         // 0x1021802c (the original answers whether Teleport did)
    // Unit v5016 (untranslated): the destructor saves the buddy and ignore lists (the argument is the empty string at 0x10250e99).
    void SaveBuddy(const char* path);                                                   // 0x1021c75c
    void SaveIgnore(const char* path);                                                  // 0x1021d74c
    bool LoadIgnore(const char* path);                                                  // 0x1021db98 (unit v5235)
    void IgnoreOn(std::u16string& name);                                                // 0x10219034 (unit v5235, untranslated)
    // Unit v5023: chat commands built as an argument vector and sent through DoCommand.
    void BlackMark(std::u16string& name);   // 0x102184a0 (command 0xe)
    void Page(std::u16string& name);        // 0x1021864c (command 0x11)
    void Locate(std::u16string& name);      // 0x102187f4 (command 0x14)
    void Goto(std::u16string& name);        // 0x1021899c (command 0x13)
    void BuddyOn(std::u16string& name);     // 0x10218c44 (command 0xf, only when the name is not in the buddy map at 0xe34)
    void DoCommand(void* arguments);        // 0x10219bec (vector of Argument, passed as its address; untranslated)
    // Unit v5103: first-person moves and the queue callback. Fields (Mac offsets, 32-bit pointers read raw): 0x00 Chat*, 0x04 CPeep*,
    // 0x11 and 0x12 flag bytes, 0x18 and 0x1c mutexes, 0xe60 vote flag byte, 0xe64 Vote*, 0xe68 deque of Argument vectors.
    void CallbackFn(int32_t id, void* arguments);              // 0x10214b70 (the vector of Argument passed as its address)
    void Teleport(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thing, MapPos x, MapPos y);   // 0x10219af4 (untranslated; the original answers whether the thing is an online person, see OnlineControlTeleportThing)
    void EnteringFirstPerson(MapPos x, MapPos y);              // 0x102177bc
    void LeavingFirstPerson(MapPos x, MapPos y);               // 0x10217884
    void UpdateFirstPersonPos(MapPos x, MapPos y);             // 0x1021797c
    void UnHideAvatar(MapPos x, MapPos y);                     // 0x10217aa8
    void UpdateAvatarPos(MapPos x, MapPos y);                  // 0x1021823c (unit v5133, untranslated)

    // Unit v5121 (src/COnlineControl_..._EarmuffsOff.cpp): commands through DoCommand and the avatar-table accessors. The earmuffs flag is the byte at 0xe50.
    void VoteForThisPark();     // 0x10218b44 (command 0x15)
    void EarmuffsOn();          // 0x10219424 (command 0xc, only when the byte at 0xe50 is 0)
    void EarmuffsOff();         // 0x1021952c (command 0xc, only when the byte at 0xe50 is not 0)
    COnlinePerson* GetPerson(std::u16string& name);   // 0x1021969c (walks the used things for the online person with that name; NULL if none)
    bool SetDestination(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thing, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell);   // 0x102197ec
    bool SetDestination(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thing, MapPos x, MapPos y);   // 0x10219908
    void Teleport(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thing, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell);   // 0x10219a10 (calls the MapPos overload with x = low 7 bits, y = high bits of cell - 1)

    // Unit v5248: the init response (the first Argument's command number picks a case of the jump table at 0x102b1b68).
    void ProcessInitResponse(void* arguments);                 // 0x10214e80 (the vector of Argument passed as its address)
    void AddContact(std::u16string& name, int32_t flag);       // 0x1021b950 (untranslated)
    bool LoadBuddy(const char* path);                          // 0x1021d004 (untranslated)

    // Unit v5276: the park-goto callbacks call these (untranslated).
    void GotoFailed();         // 0x1021c254
    void GotoStoredPerson();   // 0x1021c330

    // Unit v5285 (src/COnlineControl_..._WantToShutDown_COnlineControl_CreateVisitor.cpp). Fields: 0x00 Chat*, 0x04 CPeep*, 0x08 the player's name
    // (a wide string word), 0xe28 the contact set, 0xe34 the buddy map, 0xe44 the second nocase set (Mac offsets).
    void CreateVisitor(std::u16string& name, int32_t x, int32_t y, int32_t avatar, int32_t unused);   // 0x102182c0 (the fourth int is not read)
    bool IsThisMe(const char16_t* name);                       // 0x10217f34
    void ReceiveText(std::u16string& text, int32_t colour);    // 0x10217dbc (the text is passed by address on the Mac)
    void Feedback(std::u16string& text);                       // 0x1021b784
    uint32_t GetDataForUser(const nocase_string& key) const;   // 0x1021b854 (bit 0 buddy, 1 online, 2 flag at node+0x11, 3 in the second set)
    void NoLongerAFK(bool flag);                               // 0x10219634 (untranslated)
    // Unit v5660 (untranslated)
    void BuddyOff(std::u16string& name);                       // 0x10218e3c
    void IgnoreOff(std::u16string& name);                      // 0x1021922c
    void UpdateBuddyList() const;                              // 0x1021c02c

    // Unit v5658 (stand-in): sends the chat line (the string is passed by address on the Mac).
    void SendText(std::u16string text);   // 0x10217b5c

    // Unit v5638 (src/..._WantToShutDown_COnlineControl_ProcessResponse.cpp): the chat server's responses; the first Argument's command number
    // (0xb..0x52) picks a case of the jump table at 0x102b1c44. Fields (Mac offsets): 0x04 the own avatar (a handle word), 0x08 the player's
    // name, 0x220 the feedback text buffer (0x400 wide characters), 0xe24 a wide string, 0xe28 the contact set (end node 0xe2c), 0xe34 the
    // buddy map (end node 0xe38; a node's bytes 0x10 online and 0x11 in this park), 0xe44 the ignore set (end node 0xe48), 0xe84 the byte
    // that says a park goto is in progress.
    void ProcessResponse(void* arguments);                    // 0x10215560 (the vector of Argument passed as its address)
    void UpdateBuddyListToServer();                           // 0x1021c0ec (stand-in)
    void UpdateIgnoreListToServer();                          // 0x1021c190 (stand-in)
    void DestroyVisitor(std::u16string& name);                // 0x102183f4 (stand-in)
    void RemoveContact(std::u16string& name);                 // 0x1021ba28 (stand-in)
    bool AddBuddy(std::u16string& name, int32_t flags);       // 0x1021baf8 (stand-in; the caller tests the low byte)
    void RemoveBuddy(std::u16string& name);                   // 0x1021bdcc (stand-in)
    // Unit v5342 (declared by v5695 for ProcessQueue): the vote connection's answer picks a UI string (formatted into the buffer at
    // 0x220), shows it and sets the byte at 0xe60.
    void ProcessVoteResponse(int32_t vote);    // 0x1021c3cc
    // Unit v5897: walks the contact set (0xe28; first node in the word at 0xe30, end node 0xe2c) and adds each name to the contact list.
    void UpdateUserList() const;               // 0x1021bf30
    // Unit v5919 (stand-in elsewhere): hides the own avatar while the camera is in first person.
    void HideAvatar();         // 0x10217a38
    uint8_t mF00[0x10];
    uint8_t mClientReady;    // 0x10  1 once the client's start-up has finished
    uint8_t mF11[0xe8c - 0x11];  // 0xe8c bytes on the Mac
};
extern COnlineControl* gOnlineControl;   // 0x10396d28, created on first use (original name unknown)
// Unit v5919 (src/game/ui/ui_onlinePanel.cpp): the first-person bookkeeping of the chat panel (names invented).
extern int32_t gOnlineFirstPersonCountdown;   // 0x10395e6c, set to 5 on entering first person, counts down in UpdateAvatarPos
extern uint32_t gOnlineSavedFlags;            // 0x10395e70, the last gFirstPersonFlags UpdateAvatarPos saw
extern uint8_t gOnlineMapZ[2];                // 0x10395e74, MapPos of the avatar's z: cell, offset
extern uint8_t gOnlineMapX[2];                // 0x10395e76, MapPos of the avatar's x: cell, offset
// Unit v5907: online.cpp's static initialiser 0x1022a06c (src/game/online/online.cpp). Storage is raw bytes
// (the Mac objects are 0x14 bytes each, the host's larger); the narrow strings 0..15 are the language names (Mac 0x10396dac + 0x14 * i),
// 16 is "Theme Park World" (0x10396d8c), 17 is "Language" (0x10396d6c); the wide strings are 0x10396d4c and 0x10396d2c.
void __sinit_online_cpp();                                  // 0x1022a06c (720)
alignas(16) extern uint8_t gOnlineSinitNarrow[18][64];
alignas(16) extern uint8_t gOnlineSinitWide[2][64];
extern uint8_t gOnlineSinitNodes[20][16];                   // the registration records (12 bytes each on the Mac)
extern int32_t gOnlineSinitZero;                            // 0x10396fb0, set to 0
extern int32_t gOnlineSinitMask;                            // 0x10396fac, set to 0x10000
// Unit v5276 (src/game/online/online.cpp)
int32_t GotoPark_OK(void* context);                      // 0x10213728
void GotoCallbackFn(int32_t status, const void* data);   // 0x102137bc
// Unit v5725: std::queue<std::vector<Argument>, std::deque<...>> destructor. The queue is its deque: the deque's block table pointer
// is the word at 0x10.
struct MslArgumentQueue {
    uint32_t mDeque[4];   // 0x00 deque iterators (library code)
    uint32_t mBlocks;     // 0x10 block table (Ptr32), let go by operator delete
};
MslArgumentQueue* ArgumentQueueDestruct(MslArgumentQueue* self, int16_t deleteFlag);   // 0x10213a08

// ---- Unit v5016: the callees of COnlineControl's constructor, destructor, InitClient and InitGameWorld that no header declares (all
// untranslated or library code; the unit's test library fakes them). Objects the Mac builds from 32-bit words (strings, deques, sets) are
// passed as the address of their field.
#include <string>
class CFSOnlineSystem;
namespace online_callee {
uint32_t PointerToHandle(const void* p);   // the host's stand-in for a 32-bit pointer word: index + 1 into a table, 0 for NULL (src/game/online/online.cpp)
void* HandleToPointer(uint32_t handle);
void* OperatorNew(uint32_t size);                                // 0x102384b8
void OperatorDelete(void* p);                                       // 0x10238218
void WStringCtor(void* self);                                       // 0x101e0b84
void WStringCtorFrom(void* self, const char16_t* text);             // 0x10146a14
void WStringDtor(void* self, int16_t flag);                         // 0x101468f8
void WStringAssign(void* self, const char16_t* text, uint32_t n);   // 0x101b2510
void StringCtor(void* self);                                        // 0x1007363c
void StringDtor(void* self, int16_t flag);                          // 0x1000221c
void StringAssign(void* self, const std::string& other, uint32_t pos, int32_t count);   // 0x10001c80
void DequeCtor(void* self);                                         // 0x10225938
void DequeCopy(void* self, const void* other);                      // 0x10224fbc
void DequeTearDown(void* self);                                     // 0x1022462c
void ListDtor(void* self, int16_t flag);                            // 0x1016985c
void NocaseSetDtor(void* self, int16_t flag);                       // 0x1021425c
void BuddyMapDtor(void* self, int16_t flag);                        // 0x10214148
void WStringSetDtor(void* self, int16_t flag);                      // 0x10213fbc
void MutexCtor(void* self);                                         // 0x10200ca4
void MutexDtor(void* self, int16_t flag);                           // 0x10200d04
void ChatDataCtor(void* self);                                      // 0x1021e068
void IPBaseCtor(void* self);                                        // 0x1020ca80
int32_t IPSystemStartup(void* self);                                // 0x1016f3c8
void IPSystemShutdown(void* self);                                  // 0x1016f448
void ChatCtor(void* self);                                          // 0x1023a9f0
void ChatCallbackFn(int32_t a, void* b, void* c);                   // 0x10214a0c (a later function of the class)
int32_t ChatConnect(void* chat, const char* server, uint16_t port, void (*callback)(int32_t, void*, void*), char* extra);   // 0x1023aa08
void ChatCreatePlayer(void* chat, const void* name, int32_t customerId, int32_t playerId, const void* password, void* context);   // 0x1023b0c8
void ChatSetHearingRange(void* chat, uint32_t range, void* context);   // 0x1023ac18
void ChatEnumPlayers(void* chat, void* context);                    // 0x1023b158
void ChatRelease(void* chat);                                       // 0x1023aa50
void ChatDtor(void* chat, int16_t flag);                            // 0x1023b320
const char16_t* LoginCustomerName(CFSOnlineSystem* sys);            // 0x10171834
const char16_t* LoginPassword(CFSOnlineSystem* sys);                // 0x10171878
int32_t LoginPlayerID(CFSOnlineSystem* sys);                        // 0x101718fc
int32_t LoginCustomerID(CFSOnlineSystem* sys);                      // 0x1017193c
uint32_t AvatarID(CFSOnlineSystem* sys);                            // 0x101719bc
void COnlinePersonCtor(void* self, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell, uint32_t avatar, void* name, bool flag);   // 0x10212d98
}  // namespace online_callee

// ---- Unit v5023: the callees of Locate, Page, Goto, BlackMark and BuddyOn (the test library fakes them). An Argument is three words:
// 0 the char string (0x1007363c), 4 the wide string, 8 the command or type number. The vector of Arguments is three words.
namespace online_callee {
void WStringCopy(void* self, const void* other, uint32_t pos, uint32_t count);        // 0x101f7424
void NocaseWStringCopy(void* self, const void* other, uint32_t pos, uint32_t count);  // 0x1022213c
void NocaseWStringDtor(void* self, int16_t flag);                                     // 0x10222020
void* BuddyTreeFind(void* tree, const void* key);                                     // 0x1021f8d8 (answers the node)
void ArgumentCtor(void* self, const void* text);                                      // 0x102011a8
void ArgumentVectorPushBack(void* vec, const void* argument);                         // 0x10201478
void ArgumentVectorTearDown(void* vec);                                               // 0x10201310
}  // namespace online_callee

// ---- Unit v5051: COnlineParser (a CParser subclass; the chat line parser). Each Parse function pushes an Argument carrying the command number
// on the vector at 0x3c, reads one wide string with CParser::GetString and, when that works, pushes it as a second Argument.
// Unit v5187: a pointer to member function of CParser as CodeWarrior passes it (three words: this offset, vtable index, function).
struct ParserHandler {
    uint32_t mWords[3];
};
class COnlineParser {
public:
    bool ParseBlackMark();   // 0x1020fd70 (command 0xe)
    bool ParseBuddy();       // 0x1020ff0c (command 0xf)
    bool ParsePage();        // 0x102100a4 (command 0x11)
    bool ParseGoto();        // 0x10210314 (command 0x13)
    bool ParseLocate();      // 0x102104ac (command 0x14)

    uint8_t mF00[0x3c];      // CParser base and the rest
    void* mArguments;        // 0x3c  the vector of Argument (three words); a native pointer on the host (offset 0x40 here)
    // Unit v5131: the constructor, destructor and four simple commands.
    void* mText;             // 0x40  the std::wstring& the constructor was given (native pointer, host offset 0x48)
    void ParseEarmuffs();    // 0x1020faf8 (command 0xc)
    void ParseAfk();         // 0x1021023c (command 0x12)
    void ParseVote();        // 0x10210648 (command 0x15)
    void ParseFilter();      // 0x10210720 (command 0x16)
    static COnlineParser* Construct(COnlineParser* self, void* arguments, void* text);   // 0x1020e400 (constructor)
    static COnlineParser* Destruct(COnlineParser* self, int16_t deleteFlag);             // 0x102107fc (destructor)
    // Unit v5187
    void InitToken(uint32_t id, ParserHandler handler);   // 0x1020ed70 (puts a CToken at the position held in the word at 0x38)
    void InitTokenList();    // 0x1020e898
    bool ParseSay();         // 0x1020ef70
    bool ParseTell();        // 0x1020f0cc (answers true when GetString fails)
    // Unit v5555
    bool ParseIgnore();      // 0x1020fbd4 (command 0xd)
    void DoParse(const void* text);   // 0x1020e608 (the std::wstring by value, passed by address)
    // Unit v5891
    bool ParseReply();       // 0x1020f2f0 (command 2)
    bool ParseLast();        // 0x1020f460 (command 3)
    bool ParseShout();       // 0x1020f5d0 (command 4)
    bool ParseEmote();       // 0x1020f72c (command 5)
    bool ParseEmoteTell();   // 0x1020f888 (command 6)
};
namespace online_parse_callee {
void ParserSetup(void* parser, const void* text, void* list, const uint8_t* props);   // 0x102113dc
void ParserSkipWhitespace(void* parser);                                                // 0x10211860
bool ParserGetChar(void* parser, char16_t* ch);                                         // 0x10211ef8
int32_t ParserParse(void* parser);                                                      // 0x102114f8
void WStringAssignRange(void* self, const void* src, uint32_t pos, uint32_t count);    // 0x101f7014
}  // namespace online_parse_callee
extern ParserHandler gOnlineTokenHandlers[36];   // 0x102b1960, 12 bytes each (TOC -0x3a4c down to -0x3ad8)
namespace token_callee {
void CTokenCtor(void* self, const void* name, uint32_t w0, uint32_t w1, uint32_t w2);   // 0x10211f88 CToken::CToken(std::wstring, memptr)
bool ParserGetRemainingString(void* parser, void* wstring, int32_t* count);               // 0x10211d2c
}  // namespace token_callee
extern char16_t gOnlineParserInitialText[16];   // 0x10250a28 (TOC -0x3a48), the text the constructor copies into the output string

namespace online_callee {
bool ParserGetString(void* parser, void* wstring);   // 0x10211b3c CParser::GetString(std::wstring&)
bool ParserIgnoreRemainingString(void* parser);      // 0x10211eb0
}  // namespace online_callee

// ---- Unit v5069: std::list<CToken>::insert (0x10210b44). The Mac list is { size, ... } and its nodes are { prev, next, CToken } with 32-bit
// links (native pointers on the host). The iterator is the node pointer, passed by value.
struct CToken {
    uint32_t mWords[4];                 // 0x00 (16 bytes on the Mac)
    CToken(const CToken& other);        // 0x10210d4c
};

// Unit v5132: std::list<CToken>::clear 0x102109fc. Mac layout, Ptr32 links (the list lives in the arena in the tests).
#include "Ptr32.h"
struct CTokenHandle {
    uint32_t mWords0[2];                // 0x00
    int32_t mCount;                     // 0x08 reference count
    Ptr32<void> mBlock;                 // 0x0c block deleted first
};
struct CTokenListNode {
    Ptr32<CTokenListNode> mPrev;        // 0x00
    Ptr32<CTokenListNode> mNext;        // 0x04
    Ptr32<CTokenHandle> mData;          // 0x08
};
struct CTokenList {
    uint32_t mSize;                     // 0x00
    Ptr32<CTokenListNode> mPrev;        // 0x04 the sentinel node's prev (the sentinel is this+4, no data)
    Ptr32<CTokenListNode> mNext;        // 0x08 first node
};
namespace slc_callee {
void OperatorDelete(void* p);           // 0x10238218
}
void CTokenListClear(CTokenList* list);   // 0x102109fc std::list<CToken>::clear
namespace token_list {
struct TokenListNode {
    TokenListNode* mPrev;   // 0x00 (native pointers here)
    TokenListNode* mNext;   // 0x04
    CToken mValue;          // 0x08
};
struct TokenList {
    uint32_t mSize;     // 0x00
};
// std::list<CToken>::insert(iterator position, const CToken& token): the new node goes before position; *result is the new iterator.
void TokenList_insert(TokenListNode** result, TokenList* list, TokenListNode* position, const CToken* token);   // 0x10210b44
}  // namespace token_list

// ---- Unit v5103: callees of Update, CallbackFn and the first-person moves (the test library fakes them).
namespace online_callee {
void VoteRelease(void* vote);                  // 0x1023b200
void VoteDtor(void* vote, int16_t flag);       // 0x1023b218
void DisableVoteButton(bool flag);             // 0x101e6224
void ArgumentDequePushBack(void* deque, const void* vec);   // 0x10224a3c
void ChatMovePlayer(void* chat, int32_t x, int32_t y, int32_t mode, void* context);   // 0x10239178
}  // namespace online_callee

// ---- Unit v5131: callees of COnlineParser's constructor and destructor (the test library fakes them).
namespace online_parser_callee {
void ParserCtor(void* parser);              // 0x10211064 CParser::CParser
void ParserDtor(void* parser, int16_t flag);   // 0x102111d4 CParser::~CParser
void ListCtor(void* list);                  // 0x10210dd4 std::list<CToken>::list
void ListClear(void* list);                 // 0x102109fc std::list<CToken>::clear
void ArgumentVectorClear(void* vec);        // 0x102108a4
uint32_t WcsLen(const char16_t* text);      // 0x102389c8
void InitTokenList(void* parser);           // 0x1020e898
}  // namespace online_parser_callee

// ---- Unit v5212: ChatCallbackFn(int, void*, std::vector<Argument>&) 0x10214a0c (the vector passed as its address; the void* is unused).
void ChatCallbackFn(int32_t id, void* unused, void* arguments);

// ---- Unit v5181: std::vector<Argument> copy constructor 0x10227200 and push_back 0x10201478 (src/game/MslSupport.cpp).
class Argument;
struct ArgumentVector {
    uint32_t mCapacity;   // 0x00
    uint32_t mSize;       // 0x04
    Argument* mData;      // 0x08  (native pointer here: host offset 8, size 16)
    ArgumentVector(const ArgumentVector& other);                 // 0x10227200
    void push_back(const Argument& argument);                    // 0x10201478
};
namespace std_vector_callee {
void ArgumentCopy(Argument* self, const Argument* other);        // 0x10201878 Argument::Argument(const Argument&)
void ArgumentDestroyRange(Argument* first, Argument* last);      // 0x10201744 / 0x102013c0 / 0x10222458 std::__destroy<Argument*>
}  // namespace std_vector_callee

// ---- Unit v5310: std::__destroy<Argument*> 0x10201744 / 0x102013c0 and std::__destroy<Argument> 0x102017d4 (src/game/MslSupport.cpp).
namespace std_destroy {
void DestroyOne(Argument* argument);                       // 0x102017d4
void DestroyRange(Argument* first, Argument* last);        // 0x10201744
void DestroyRangeTagged(Argument* first, Argument* last);  // 0x102013c0 (random_access_iterator_tag, empty)
}  // namespace std_destroy

// ---- Unit v5229: std::basic_string<wchar_t, nocase_wchar_traits> compare 0x10225a5c and substring constructor 0x1022213c
// (src/game/MslSupport.cpp). The string is one pointer to an IME::MslWideStringRep (length 0, capacity 4, refs 8, data 0xc;
// native pointer at 0x10 here). NocaseWStringCopy (declared above) is the constructor.
namespace online_callee {
int32_t NocaseWStringCompare(const void* self, uint32_t pos, uint32_t n1, const char16_t* s, uint32_t n2);   // 0x10225a5c
// 0x1022705c init(first, last, allocator, random_access_iterator_tag): the constructor's callee; the test library fakes it.
void NocaseWStringInit(void* self, const char16_t* first, const char16_t* last);
}  // namespace online_callee

// ---- Unit v5756: std::operator< on two nocase wide strings 0x10225d2c (src/game/MslSupport.cpp)
namespace online_lt {
uint32_t NocaseWStringSize(const void* self);                 // 0x10226134 size() const
const char16_t* NocaseWStringBegin(const void* self);         // 0x102260c8 begin() const
}  // namespace online_lt
bool NocaseWStringLess(const void* a, const void* b);         // 0x10225d2c operator<(a, b)

// ---- Unit v5235: SaveIgnore 0x1021d74c and LoadIgnore 0x1021db98 (they answer 0 or 1; the member SaveIgnore is declared void above, so
// the answer comes from OnlineSaveIgnore). The stream objects are stack objects the library builds; the host structs keep the fields
// the code reads (Mac offsets in comments; host pointers are 8 bytes, so fields after a pointer sit 4 bytes higher).
extern uint32_t gOfsWVtbl[4];   // 0x102b1f50 (TOC -0x5eac): the ofstream part's vtable
bool OnlineSaveIgnore(COnlineControl* self, const char* path);   // 0x1021d74c
class CFSUserInfo;
namespace ignore_callee {
struct IgnoreIos {              // the stream's virtual base, pointed to by the stream's first word
    const void* mVtable;        // Mac 0x00
    uint8_t mPad04[0x20];       // Mac 0x04
    uint32_t mStreamBuf;        // Mac 0x24 (host 0x28)
    uint8_t mPad28[0xc];        // Mac 0x28
    uint32_t mState;            // Mac 0x34 (host 0x38)
    uint32_t mExceptions;       // Mac 0x38
    uint8_t mPad3c[8];          // Mac 0x3c
    int32_t mBaseOffset;        // Mac 0x44 (host 0x48): the ios part's address minus this block's
    uint8_t mPad48[0x10];
};
struct IgnoreStream {           // basic_ofstream / basic_ifstream (the stream part)
    IgnoreIos* mIos;            // Mac 0x00
    const void* mSlot;          // Mac 0x04 (host 0x08)
    uint8_t mRest[0x50];
};
struct IgnoreFilebuf {
    uint8_t mPad[0x24];
    uint32_t mFile;             // 0x24
    uint8_t mRest[0x30];
};
struct IgnorePathString {       // TbDynamicStringTemplate<wchar_t> (0x14 bytes on the Mac)
    const void* mVtable;        // Mac 0x00
    uint32_t mLength;           // Mac 0x04
    const char16_t* mData;      // Mac 0x08 (host 0x10)
    uint32_t mCapacity;         // Mac 0x0c
    uint8_t mRest[8];
};
struct IgnoreWString {          // std::wstring: one word
    const char16_t* mText;
};
struct IgnoreLocale {           // std::locale: two words
    uint32_t mWords[2];
};
void OfstreamCtor(IgnoreStream* stream, int32_t flag);           // 0x10221f20
void WostreamCtor(void* self, int32_t flag, void* streambuf);    // 0x10226c94 basic_ostream<wchar_t>(streambuf*), unit v5755's callee
void IfstreamCtor(IgnoreStream* stream, int32_t flag);           // 0x10221e20
void UserInfoCtor(void* self);                                   // 0x1017de54
void ReturnOnlinePath(IgnorePathString* result, CFSUserInfo* info);   // 0x1017f980
void PathCopy(IgnorePathString* self, const IgnorePathString* other);   // 0x102387a0
void PathDtor(IgnorePathString* self, int16_t flag);             // 0x10238710
void PathAppendString(IgnorePathString* self, const void* other);   // 0x10238788 (a TbStringBase)
void PathAppendText(IgnorePathString* self, const char16_t* text);  // 0x1023a8a0
void CharStringAssign(void* self, const char* text, uint32_t count);   // 0x101b2440
void CharStringAppend(void* self, const char* text, uint32_t count);   // 0x1020100c
void SprintfWide(char* buffer, const char* format, const char16_t* text);   // 0x10238578 sprintf(buffer, "%ls", text)
uint32_t FilebufOpen(IgnoreFilebuf* filebuf, const char* path, int32_t mode);   // 0x10221c30
uint32_t FilebufClose(IgnoreFilebuf* filebuf);                   // 0x10221b40
char* NewArray(uint32_t size);                                   // 0x10238668
void RefPtrCtor(void* self, char* pointer);                      // 0x100026ac
char* StrCpy(char* to, const char* from);                        // 0x10238410
void FilebufDtor(IgnoreFilebuf* filebuf, int16_t flag);          // 0x1021cd4c
void OstreamDtor(IgnoreStream* stream, int16_t flag);            // 0x1021cc68
void IstreamDtor(IgnoreStream* stream, int16_t flag);            // 0x1021d4a0
void IosDtor(void* ios, int16_t flag);                           // 0x1021cbb8
void SwprintfWide(char16_t* buffer, uint32_t count, const char16_t* format, const char16_t* text);   // 0x10239160
void OstreamWrite(IgnoreStream* stream, const char16_t* text);   // 0x1021f374
void TreeIncrement(uint32_t* node);                              // 0x10152db0
void IosGetloc(IgnoreLocale* out, IgnoreIos* ios);               // 0x10072a34
void* UseFacetCtype(IgnoreLocale* locale);                       // 0x1021eb10
char16_t CtypeWiden(void* facet, char c);                        // __ptr_glue, vtable 0x2c
void LocaleDtor(IgnoreLocale* locale, int16_t flag);             // 0x10071f34
IgnoreStream* IstreamGetline(IgnoreStream* stream, char16_t* buffer, int32_t count, char16_t delimiter);   // 0x102202b0
void ChatIgnore(uint32_t chat, const IgnoreWString* name, COnlineControl* context);   // 0x1023ac78
}  // namespace ignore_callee

// Unit v5727: CParser (the base of COnlineParser): constructor 0x10211064 and SetupWhitespaceSet 0x10211344.
extern uint8_t gCParserVtableBlock[16];   // 0x102b1b3c; the constructor stores the address of +8 (0x102b1b44, TOC -0x5ed8)
class CParser {
public:
    CParser();                              // 0x10211064
    void SetupSeparatorSet();               // 0x102112b0 (not in this unit)
    void SetupWhitespaceSet();              // 0x10211344
    void AddWhitespace(char16_t c);         // 0x102117fc (not in this unit)

    const void* mVtable;                    // 0x00
    void* mString;                          // 0x04 std::wstring (one word on the Mac, host 0x08)
    uint8_t mSeparators[24];                // 0x08 std::set<wchar_t> (0xc bytes on the Mac)
    uint8_t mWhitespace[24];                // 0x14 std::set<wchar_t> (0xc bytes on the Mac)
    // Unit v5911 (host offsets here are the Mac's plus 8 after the string word, plus 4 more after each earlier pointer):
    char16_t* mCursor;                      // 0x20 the string's begin() (host 0x40)
    uint32_t mPos;                          // 0x24 (host 0x48)
    uint8_t mPad28;                         // 0x28 (host 0x4c)
    uint8_t mCaseFlag;                      // 0x29 (host 0x4d) set: exact compare
    uint8_t mStopFlag;                      // 0x2a (host 0x4e) set: Parse stops after one token
    uint8_t mPrefixFlag;                    // 0x2b (host 0x4f) set: prefix compare, no case
    void* mTokens;                          // 0x2c the std::list<CToken> (host 0x50)
    void SetupForParsing(const void* text, void* list, const uint8_t* props);   // 0x102113dc
    int32_t Parse();                        // 0x102114f8
    void* Match(const void* text);          // 0x10211630 answers the CParserToken or NULL
    static CParser* Destruct(CParser* self, int16_t deleteFlag);   // 0x102111d4 (destructor)
    int32_t SkipWhitespace();               // 0x10211860
    int32_t SkipWhitespaceAndSeparator();   // 0x1021193c
    int32_t CountNonWhitespace();           // 0x10211a60
    bool GetString(void* out);              // 0x10211b3c
    bool GetRemainingString(void* out, int32_t* count);   // 0x10211d2c
    // Unit v51059: AddSeparator inserts into the set at Mac +0x14 (the field named mWhitespace above), AddWhitespace into the set at Mac +8
    // (mSeparators above): the listing decides, the field names are the other way round.
    void AddSeparator(char16_t c);          // 0x10211798
    bool IgnoreRemainingString();           // 0x10211eb0 (host: mPos = the first word of the string's block; answers true)
};
// Unit v5911: the parser's token list. Mac: node { prev 0, next 4, name word 8, handler (pointer to member, 3 words) 0xc }; list { size 0,
// sentinel prev 4, sentinel next 8 }. The host's pointers are 8 bytes.
struct CParserToken {
    void* mName;                // Mac node+0x08 std::wstring (one word)
    uint32_t mHandler[3];       // Mac node+0x0c
};
struct CParserTokenNode {
    CParserTokenNode* mPrev;    // 0x00
    CParserTokenNode* mNext;    // 0x08 (Mac 0x04)
    CParserToken mToken;        // 0x10 (Mac 0x08)
};
struct CParserTokenList {
    uint32_t mSize;             // 0x00
    CParserTokenNode* mPrev;    // 0x08 (Mac 0x04) the sentinel: its address is end()
    CParserTokenNode* mNext;    // 0x10 (Mac 0x08) first node
};
namespace cparser_callee {      // weak stand-ins in the source file; recording fakes in the test library
void* TreeFind(void* tree, const char16_t* key);        // 0x10212060 std::__tree<wchar_t>::find (answers the node; end() is the tree + 4 on the Mac)
void TreeDestroy(void* tree, void* root);               // 0x10212604
void StringDtor(void* string, int16_t flag);            // 0x101468f8
void OperatorDelete(void* p);                           // 0x10238218
bool CallHandler(CParser* self, const uint32_t* handler);   // runtime glue 0x1023ba70: calls the pointer to member (3 words) on self
}
namespace parser_callee {
void TreeCtor(void* tree, const void* less, const void* allocator);   // 0x10212a88 std::__tree<wchar_t,...>::__tree
}

// Unit v5653: std::__tree<wchar_t, std::less<wchar_t>, std::allocator<wchar_t>>, a std::set<wchar_t>'s tree (insert_node_at 0x10212b2c).
// Mac words are 32 bits (offsets in comments), the host's are native. The end node is the root word itself.
struct WcharSetTree {
    struct node {
        node* mLeft;         // 0x00
        node* mRight;        // 0x04
        uintptr_t mParent;   // 0x08  parent with the colour in bit 0
        char16_t mValue;     // 0x0c  (the Mac node is 0xe bytes)
    };
    uint32_t mCount;   // 0x00
    node* mRoot;       // 0x04
    node* mBegin;      // 0x08  the first node

    node* insert_node_at(node* parent, bool left, bool leftmost, const char16_t& value);   // 0x10212b2c
};
namespace WcharTreeCallee {
void BalanceInsert(WcharSetTree::node* node, WcharSetTree::node* root);   // 0x10153b24 __red_black_tree::balance_insert
}

// ---- Unit v5242: Argument::Argument(const Argument&) 0x10201878 (src/game/MslSupport.cpp).
namespace argument_callee {
void StringInit(void* self, const char* first, const char* last);   // 0x10002b8c std::string init(first, last, allocator, tag): the test library fakes it
}  // namespace argument_callee

// ---- Unit v5248: callees of ProcessInitResponse that no header declares (the test library fakes them). Strings and std::wstring objects are
// one pointer word on the Mac (opaque 8 bytes here).
namespace init_callee {
void StringFromChars(void* self, const char* text);                 // 0x10002338 std::string(const char*, const allocator&)
void* ReturnCityInfo(uint32_t cityList, int32_t id);                // 0x10178b3c CFSOnlineSystemCityList::ReturnCityInfo
void ChatSetPlayerData(uint32_t chat, int32_t avatar, void* context);   // 0x1023b0f8
void ChatCreateRoom(uint32_t chat, const void* name, const void* topic, const void* city, const void* password, int32_t size, void* context);   // 0x1023b110
void LockChatPanelOff();                                            // 0x101e55ec
}  // namespace init_callee

// ---- Unit v5285: callees of CreateVisitor, TeleportAvatar, IsThisMe, ReceiveText, GetDataForUser and AddContact that no header declares
// (the test library fakes them). Chat/UI objects are passed as addresses.
namespace online_callee {
// 0x10219a10 COnlineControl::Teleport(thing, cell): the original answers whether it moved the avatar (the member is declared void).
bool TeleportCell(COnlineControl* self, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thing, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell);
void MasterSaveCtor(void* self);                                              // 0x101694dc CFSGlobalMasterSave::CFSGlobalMasterSave() on a block of 0x4c bytes
bool NocaseWStringEqual(const void* a, const void* b);                       // 0x1021f254 operator== on two nocase wide strings
void* NocaseSetFind(void* tree, const void* key);                             // 0x1021fd4c (answers the node; the end node is the tree + 4)
void WStringSetInsertOne(void* tree, const std::u16string& key);              // 0x10223a24 (the result iterator is dropped)
void UIOnlineReceiveStr(void* ui, const char16_t* text, int32_t colour);      // 0x101f0978 UIInterface::OnlineReceiveStr(wchar_t*, long)
void UIOnlineAddContact(void* ui, const char16_t* text, int32_t flag);        // 0x101f09dc UIInterface::OnlineAddContact(wchar_t*, long)
}  // namespace online_callee

// ---- Unit v5292: OnlineChatData constructor 0x1021e068 and destructor 0x10213cac (src/..._SaveIgnore_OnlineChatData_OnlineChatData.cpp).
// Every member is a 32-bit word on the Mac (opaque sub-objects; the callees are faked and take the sub-object's address).
namespace chatdata_callee {
void TreeWStringCtor(void* tree);                       // 0x10223c74 std::set<std::wstring> tree ctor
void TreeBuddyCtor(void* tree);                         // 0x10222fe0 std::map<nocase_string, BuddyInfo> tree ctor
void TreeNocaseCtor(void* tree);                        // 0x10224540 std::set<nocase_string> tree ctor
void ListCtor(void* list);                              // 0x1016d63c std::list<std::wstring>::list
void ListClear(void* list);                             // 0x1016d4e8 std::list<std::wstring>::clear
void TreeWStringDestroy(void* tree, uint32_t node);     // 0x10223204
void TreeBuddyDestroy(void* tree, uint32_t node);       // 0x10222564
void TreeNocaseDestroy(void* tree, uint32_t node);      // 0x10223e64
}  // namespace chatdata_callee
class OnlineChatData {
public:
    uint32_t mString0;      // 0x00 std::wstring
    uint32_t mString4;      // 0x04 std::wstring
    uint32_t mTreeA[3];     // 0x08 std::set<std::wstring> tree, root node at 0x0c
    uint32_t mTreeB[4];     // 0x14 buddy map tree, root node at 0x18
    uint32_t mTreeC[3];     // 0x24 std::set<nocase_string> tree, root node at 0x28
    uint8_t mFlag30;        // 0x30
    uint8_t mFlag31;        // 0x31
    uint8_t mPad32[2];
    uint32_t mList[3];      // 0x34 std::list<std::wstring>
    static OnlineChatData* Construct(OnlineChatData* self);                    // 0x1021e068 (constructor)
    static OnlineChatData* Destruct(OnlineChatData* self, int16_t deleteFlag); // 0x10213cac (destructor)
};

// ---- Unit v5568: cmp_nocase 0x10210e68 (src/game/online/Parser.cpp).
namespace IME { struct MslWideString; }
extern uint16_t gWUpperMap[256];   // 0x103bd0a0 (data import c/c++_shared::__wupper_map): the upper-case map of the characters below 0x100
int32_t cmp_nocase(const IME::MslWideString& a, const IME::MslWideString& b, bool prefix);   // 0x10210e68

// ---- Unit v5640: operator<<(wostream&, const wchar_t*) 0x1021f374 (src/game/MslSupport.cpp; declared above as ignore_callee::OstreamWrite).
// Callees (the test library fakes them). The ios is the ignore_callee::IgnoreIos, the iterator is the stream buffer address.
namespace wostream_callee {
bool Good(void* ios);                       // 0x102079b4 ios_base::good
void* Tie(void* ios);                       // 0x1021f888 basic_ios::tie
void Flush(void* stream);                   // 0x102268f8 basic_ostream::flush
void Setstate(void* ios, int32_t bits);     // 0x102077c0 ios_base::setstate
void* PutNumber(void* iter, void* ios, char16_t fill, const char16_t* prefix, int32_t prefixLen, const char16_t* suffix, int32_t suffixLen);   // 0x10226358 (the iterator comes back, NULL on failure)
void Throw(const char* type, void* object, const void* destructors);   // __throw (the fake returns)
uint32_t Wcslen(const char16_t* text);      // 0x102389c8
uint32_t Strlen(const char* text);          // 0x10238650
char* NewBytes(uint32_t size);              // 0x10238668 operator new[]
int32_t* NewInt();                          // 0x102384b8 operator new(4)
void CopyString(char* to, const char* from);   // 0x10238410 strcpy
}  // namespace wostream_callee

// ---- Unit v5636: COnlineControl::DoCommand 0x10219bec (src/..._WantToShutDown_COnlineControl_DoCommand.cpp). The callees no header
// declares (untranslated or imports; the test library fakes them). A std::wstring is one word, the 32-bit address of its
// IME::MslWideStringRep; Chat's first argument is the Chat word at COnlineControl 0x00.
namespace docommand_callee {
void ArgumentBStrVal(void* result, const void* argument);                       // 0x10217764 Argument::BStrVal() (the wstring by value)
void ChatNotAFK(uint32_t chat, void* context);                                  // 0x10239910 (glue)
void ChatSay(uint32_t chat, const void* text, bool flag, void* context);        // 0x1023aba0
void ChatTell(uint32_t chat, const void* name, const void* text, bool flag, void* context);   // 0x1023abb8
void ChatRoomShout(uint32_t chat, const void* text, bool flag, void* context);  // 0x1023ac00
void ChatEarmuffsOff(uint32_t chat, void* context);                             // 0x1023ac30
void ChatEarmuffsOn(uint32_t chat, void* context);                              // 0x1023ac48
void ChatUnIgnore(uint32_t chat, const void* name, void* context);              // 0x1023ac60
void ChatBlackmark(uint32_t chat, const void* name, void* context);             // 0x1023ac90
void ChatRemoveBuddy(uint32_t chat, const void* name, void* context);           // 0x1023aca8
void ChatAddBuddy(uint32_t chat, const void* name, void* context);              // 0x1023acc0
void ChatPage(uint32_t chat, const void* name, void* context);                  // 0x1023acd8
void ChatAFK(uint32_t chat, void* context);                                     // 0x1023acf0
void ChatGoto(uint32_t chat, const void* name, void* context);                  // 0x1023ad08
void ChatLocate(uint32_t chat, const void* name, void* context);                // 0x1023ad20
void OnlineSystemCtor(void* self);                                              // 0x1016f7d0 CFSOnlineSystem::CFSOnlineSystem() on 0x404 bytes
void VoteCtor(void* self);                                                      // 0x1023ad38 Vote::Vote() on 0xc bytes
int32_t VoteConnect(void* vote, char* server, char* extra, uint16_t port, void (*callback)(int32_t, int32_t, void*, void*));   // 0x1023ad50
void VoteSetSessionInfo(void* vote, int16_t kind, int32_t playerId, int32_t customerId, const char16_t* password);   // 0x1023ad68
void VoteSendVote(void* vote, int32_t id, void* data, int32_t count, void* extra);   // 0x1023ad80
}  // namespace docommand_callee
void VoteNotifyCallback(int32_t a, int32_t b, void* c, void* d);   // 0x10213590 (untranslated; DoCommand passes its TVector 0x10268998)
extern uint32_t gVoteSendData[4];   // 0x102b1b58 (TOC -0x3970): the 16 bytes DoCommand copies and sends with Vote::SendVote

// ---- Unit v5638: callees of COnlineControl::ProcessResponse that no header declares (the test library fakes them). The wide strings the
// function builds are std::u16string objects here (one word on the Mac, a pointer to a rep whose data pointer is at +0xc).
namespace response_callee {
void ArgumentBStrVal(std::u16string* result, const Argument* argument);   // 0x10217764 Argument::BStrVal (the wide string by value)
bool WStringEqual(const std::u16string& a, const void* b);                // 0x1021f130 operator==(const wstring&, const wstring&)
void* WStringSetFind(void* tree, const void* key);                        // 0x1021fae4 (answers the node; the end node is the tree + 4)
void NocaseSetInsertOne(void* tree, const void* key);                     // 0x102243c0 (the result pair is dropped)
void NocaseSetErase(void* tree, void* node);                              // 0x1022419c (the iterator by value)
void UIOnlineUpdateBuddy(void* ui, const char16_t* name, int32_t flags);     // 0x101f0bc8 UIInterface::OnlineUpdateBuddy(wchar_t*, long)
void UIOnlineUpdateContact(void* ui, const char16_t* name, int32_t flags);   // 0x101f0aa0 UIInterface::OnlineUpdateContact(wchar_t*, long)
}  // namespace response_callee
void EarmuffsButtonDown(bool down);   // 0x101e6168 (stand-in)

// ---- Unit v5695: callees of ProcessQueue, AddBuddy and LoadBuddy that no header declares (the test library fakes them).
// The buddy map entry AddBuddy inserts: the key (a nocase string, one word on the Mac) and the BuddyInfo's two flag bytes.
struct BuddyEntry {
    uint64_t mKey;      // Mac 0x00 (one word)
    uint8_t mDirect;    // Mac 0x04  BuddyInfo byte 0 (node offset 0x10)
    uint8_t mFlag;      // Mac 0x05  BuddyInfo byte 1 (node offset 0x11)
};
namespace online_callee {
void BuddyTreeInsert(void* tree, const BuddyEntry* entry);                   // 0x10222d9c insert_one (the result iterator is dropped)
void UIOnlineAddBuddy(void* ui, const char16_t* text, int32_t data);          // 0x101f0b08 UIInterface::OnlineAddBuddy(wchar_t*, long)
void UIOnlineUpdateContact(void* ui, const char16_t* text, int32_t data);     // 0x101f0aa0 UIInterface::OnlineUpdateContact(wchar_t*, long)
void ArgumentDequePopFront(void* deque);                                       // 0x1022488c std::deque<std::vector<Argument>>::pop_front
}  // namespace online_callee

// ---- Unit v5899: std::__tree<nocase_string, std::less<nocase_string>, std::allocator<nocase_string>> (a std::set<nocase_string>),
// src/game/MslSupport.cpp. Mac offsets in comments; the host's words are native. A nocase_string is one word: a pointer to its rep.
struct NocaseRep {
    uint32_t mLength;     // 0x00
    uint32_t mCapacity;   // 0x04
    uint32_t mRefs;       // 0x08
    char* mData;          // 0x0c
};
struct NocaseSetTree {
    struct node {
        node* mLeft;          // 0x00
        node* mRight;         // 0x04
        uintptr_t mParent;    // 0x08  parent with the colour in bit 0
        NocaseRep* mValue;    // 0x0c  the nocase_string
    };
    uint32_t mCount;    // 0x00
    node* mRoot;        // 0x04  (the end node's left word)
    node* mBegin;       // 0x08  the first node

    node* end() { return reinterpret_cast<node*>(&mRoot); }
    void erase(node* position);       // 0x1022419c (the iterator is the node pointer)
    void destroy(node* subtree);      // 0x10223e64
    node* insert_node_at(node* parent, bool left, bool leftmost, const nocase_string& key);   // 0x10228458
};
namespace nocase_callee {
void AllocDestroy(void* allocator, nocase_string* value);                      // 0x1021ecb8 std::allocator<nocase_string>::destroy
void Deallocate(void* allocator, void* node, unsigned long count);             // 0x1021eda4 std::allocator<tree node>::deallocate
void StringDtor(nocase_string* self, int16_t deleteFlag);                      // 0x102176d8 nocase_string::~nocase_string
nocase_string* StringCopy(nocase_string* self, const nocase_string* other);    // 0x10228770 nocase_string::nocase_string(const&)
void BalanceInsert(NocaseSetTree::node* node, NocaseSetTree::node* root);      // 0x10153b24 __red_black_tree::balance_insert
}  // namespace nocase_callee

// ---- Unit v5897: callees of RemoveBuddy and RemoveContact that no header declared (the test library fakes them).
namespace online_callee {
void BuddyTreeEraseOne(void* tree, const void* key);          // 0x1021e8e8 erase_one on the buddy map
void UIOnlineRemoveContact(void* ui, const char16_t* text);   // 0x101f0a40 UIInterface::OnlineRemoveContact(wchar_t*)
void UIOnlineRemoveBuddy(void* ui, const char16_t* text);     // 0x101f0b68 UIInterface::OnlineRemoveBuddy(wchar_t*)
}  // namespace online_callee

// Unit v5903: std::pair<const nocase_string, BuddyInfo>::~pair (the entry's first word is the key).
BuddyEntry* BuddyEntryDestruct(BuddyEntry* self, int16_t flag);   // 0x1021bd24

// ---- Unit v5938: std::deque<std::vector<Argument>> of the Metrowerks library (src/game/MslSupport.cpp).
// All words are the Mac's 32-bit words (the host's heap addresses are truncated to 32 bits: the arena is 32-bit addressable).
// The elements are 0xc bytes (the Mac's std::vector<Argument>). push_back 0x10224a3c, pop_front 0x1022488c and the copy
// constructor 0x10224fbc are online_callee::ArgumentDequePushBack, ArgumentDequePopFront and DequeCopy (declared above).
struct MslArgumentDeque {
    uint32_t mBlockSize;   // 0x00 elements per block
    uint32_t mCapacity;    // 0x04 the block table (a std::__cdeque<const void*>): capacity
    uint32_t mCount;       // 0x08   number of blocks
    uint32_t mStart;       // 0x0c   slot of the first block
    uint32_t mTable;       // 0x10   address of the table of block pointers
    uint32_t mFirst;       // 0x14 index of the first element
    uint32_t mSize;        // 0x18 number of elements
};
struct MslArgumentCursor {   // std::__cdeque<const void*>::const_iterator (0xc)
    uint32_t mCdeque;   // 0x0 the block table (deque + 4)
    uint32_t mIndex;    // 0x4
    uint32_t mSlot;     // 0x8 address of the table slot holding the block pointer
};
struct MslArgumentDequeIter {   // std::deque<std::vector<Argument>>::const_iterator (0x18)
    MslArgumentCursor mCursor;   // 0x00
    uint32_t mElement;           // 0x0c
    uint32_t mBlockEnd;          // 0x10
    uint32_t mIndex;             // 0x14
};
extern uint32_t gArgDequeBlockSize;   // 0x102b1f94 (0x2a): elements per block of a new deque
extern uint32_t gArgDequeFill;        // 0x102b1f98 (0): the word whose address is passed to the block table's push_back
// 0x10228c84 make_iterator(ulong) const; the Mac returns the iterator through r3.
void ArgumentDequeMakeIterator(MslArgumentDequeIter* out, const MslArgumentDeque* self, uint32_t index);
namespace argdeque_callee {
void CdequeReserve(void* cdeque, uint32_t n);                                // 0x101f2730
void CdequeResize(void* cdeque, uint32_t n, const void* fill);               // 0x101f2a10
void CdequePushBack(void* cdeque, const uint32_t* value);                    // 0x101f459c
void CdequePopFront(void* cdeque);                                           // 0x101f25e8
void* CdequeIndex(void* cdeque, uint32_t index);                             // 0x101f2004 answers the slot's address
bool GrowBufBack(MslArgumentDeque* self, uint32_t from, void* spare, uint32_t* added, uint32_t to);   // 0x102287e4
void VectorCopy(void* target, const void* source);                           // 0x10227200
void VectorDtor(void* vec, int16_t flag);                                    // 0x102010f8
void CursorAdd(MslArgumentCursor* cursor, int32_t count);                    // 0x101f5508
void* IterDeref(const MslArgumentDequeIter* it);                             // 0x10225874
void IterInc(MslArgumentDequeIter* it);                                      // 0x1022571c
bool IterNotEqual(const MslArgumentDequeIter* a, const MslArgumentDequeIter* b);   // 0x102255a0
}  // namespace argdeque_callee

// ---- Unit v5976 (src/game/MslSupport.cpp): grow_buf_back 0x102287e4 is
// argdeque_callee::GrowBufBack (declared above; the Mac returns the "swapped the block table" flag in r3), destroy 0x10228fd4 and the
// destructor 0x10213b80. Words are the Mac's 32-bit words; a host pointer is (gMslMacAddressHigh << 32) | word.
extern uint32_t gArgDequeGrowFill;   // 0x102b2088 (0): the word whose address grow_buf_back passes to the block table's push_back
// destroy(first, last): the iterators by value (r4..r9 and r10 + the stack); only the element word and the index are read.
void ArgumentDequeDestroy(MslArgumentDeque* self, MslArgumentDequeIter first, MslArgumentDequeIter last);   // 0x10228fd4
// ~deque (deleting form for a flag above 0); answers this.
MslArgumentDeque* ArgumentDequeDestruct(MslArgumentDeque* self, int16_t deleteFlag);   // 0x10213b80
namespace argdestroy_callee {
void DestroyRange(uint32_t first, uint32_t last);   // 0x1022935c std::__destroy<std::vector<Argument>*> (the iterator tag is ignored)
}
// Unit v51105 (src/CUI2DMap_..._CUI2DMap_UnRegister_CUI2DMap_Register.cpp): more deque and string pieces.
extern uint32_t gArgDequeCtorBlockSize;     // 0x102b1f90 (0x2a): the block size DequeCtor 0x10225938 reads
extern uint16_t gNocaseWStringTerminator;   // 0x102b208c (0): the character NocaseWStringInit 0x1022705c ends the data with
namespace argdeque_new {
void CdequeIterCopy(MslArgumentCursor* dest, const MslArgumentCursor* src);                           // 0x102295ec iterator::iterator(const&)
void MakeIterator(MslArgumentDequeIter* out, const MslArgumentDeque* self, uint32_t index);           // 0x10228e2c make_iterator(ulong)
}

// ---- Unit v5956: std::vector<Argument>::clear 0x102108a4 (calls the second std::__destroy<Argument*>), std::list<std::wstring>::insert
// 0x1020deec. A node is { prev, next, string } (Mac 0xc bytes; the host's pointers are 8 bytes, the string sits at 0x10); the list is
// { size 0, sentinel prev, sentinel next } and the iterator is the node address. The result iterator comes back through a pointer.
namespace std_destroy {
void DestroyRangeClear(Argument* first, Argument* last);   // 0x10210944 (random_access_iterator_tag, empty)
}
namespace wstring_list {
struct WStringNode {
    WStringNode* mPrev;     // 0x00
    WStringNode* mNext;     // 0x08 (Mac 0x04)
    void* mString;          // 0x10 (Mac 0x08) the std::wstring: one pointer
};
struct WStringList {
    uint32_t mSize;         // 0x00
};
void WStringList_insert(WStringNode** result, WStringList* list, WStringNode* position, const void* text);   // 0x1020deec
}  // namespace wstring_list

// ---- Unit v51059: the std::set<wchar_t> tree helpers (find 0x10212060, destroy 0x10212604, insert_one 0x10212960, __tree 0x10212a88 are
// declared above as cparser_callee::TreeFind / TreeDestroy and parser_callee::TreeCtor), the set destructor, the std::list<CToken> constructor
// (online_parser_callee::ListCtor), the three online set/map destructors (online_callee::NocaseSetDtor, BuddyMapDtor, WStringSetDtor) and
// IPSystem's destructor.
extern uint8_t gWcharSetInserted;      // 0x102b1b54 (1): the byte insert_one answers for a new element
extern uint8_t gWcharSetNotInserted;   // 0x102b1b55 (0): the byte it answers for an existing one
extern uint32_t gWcharSetCountInit;    // 0x102b1b50 (0): the word the tree constructor stores as the count
extern uint32_t gTokenListInit;        // 0x102b1b38 (0): the word the std::list<CToken> constructor stores as the size
extern uint8_t gIPBaseVtableBlock[16];     // 0x102b1f7c; the destructor stores the address of +8 (0x102b1f84, TOC -0x5e98)
extern uint8_t gIPSystemVtableBlock[16];   // 0x102b1938; the destructor stores the address of +8 (0x102b1940, TOC -0x5ee8)
namespace v51059 {
struct WcharInsertResult {          // pair<iterator, bool>, the hidden result of insert_one
    WcharSetTree::node* mNode;      // 0x00
    uint8_t mInserted;              // Mac 0x04 (host 0x08)
};
void TreeInsertOne(WcharInsertResult* result, WcharSetTree* tree, const char16_t* key);   // 0x10212960
void NodeDeallocate(void* allocator, WcharSetTree::node* node, uint32_t count);           // 0x10212854 (deletes the node)
void WcharAllocatorDestroy(void* allocator, char16_t* value);                              // 0x10212928 (empty)
WcharSetTree* WcharSetDtor(WcharSetTree* self, int16_t deleteFlag);                       // 0x10211110, answers self
void* IPSystemDtor(void* self, int16_t deleteFlag);                                       // 0x10214348, answers self
}  // namespace v51059
namespace COnlinePersonV51059 {
void FinalDestroy(COnlinePerson* self);   // see COnlinePerson::FinalDestroy 0x10212f58
}

// ---- Unit v51126 (src/game/MslSupport.cpp, src/game/ui/UIInterface.cpp, src/game/ui/ui_FeatureStatus.cpp): tree constructors and insert_one of the nocase set, the
// wstring set and the buddy map, the Argument destroyers, the nocase wstring destructor, the deque<vector<Argument>> tear_down.
// The structures keep the Mac's 32-bit words (the arena is 32-bit addressable: a host pointer is converted to its word).
namespace v51126 {
struct TreeHeader3 {          // std::set tree (nocase_string, std::wstring): 0xc bytes
    uint32_t mCount;          // 0x00
    uint32_t mRoot;           // 0x04 (the end node's left word)
    uint32_t mBegin;          // 0x08
};
struct TreeHeader4 {          // the buddy map's tree: 0x10 bytes
    uint32_t mCount;          // 0x00
    uint32_t mRoot;           // 0x04
    uint8_t mByte8[4];        // 0x08 (the compare functor's byte is stored at 0x08 and stays)
    uint32_t mBegin;          // 0x0c
};
struct TreeInsertResult {     // pair<iterator, bool>, the hidden result of insert_one
    uint32_t mNode;           // 0x00
    uint8_t mInserted;        // 0x04
};
struct ArgVectorPtrCdeque {   // std::__cdeque<std::vector<Argument>*>
    uint32_t mWord0;          // 0x00
    uint32_t mWord4;          // 0x04
    uint32_t mWord8;          // 0x08
    uint32_t mWordC;          // 0x0c the table (deleted by the destructor when not NULL)
};
extern uint32_t gNocaseTreeCount;           // 0x102b1f9c (0)
extern uint8_t gNocaseTreeInserted;         // 0x102b1fa0 (1)
extern uint8_t gNocaseTreeNotInserted;      // 0x102b1fa1 (0)
extern uint32_t gWStringTreeCount;          // 0x102b1fa4 (0)
extern uint8_t gWStringTreeInserted;        // 0x102b1fa8 (1)
extern uint8_t gWStringTreeNotInserted;     // 0x102b1fa9 (0)
extern uint32_t gBuddyTreeCount;            // 0x102b1fac (0)
void NocaseTreeCtor(TreeHeader3* self, const uint8_t* compare);       // 0x10224540
void WStringTreeCtor(TreeHeader3* self, const uint8_t* compare);      // 0x10223c74
void BuddyTreeCtor(TreeHeader4* self, const uint8_t* compare);        // 0x10222fe0
void NocaseTreeInsertOne(TreeInsertResult* result, TreeHeader3* self, const void* key);    // 0x102243c0
void WStringTreeInsertOne(TreeInsertResult* result, TreeHeader3* self, const void* key);   // 0x10223a24
void BuddyNodeDeallocate(void* allocator, void* node, uint32_t count);    // 0x102229f8 (calls operator delete on the node)
void WStringNodeDeallocate(void* allocator, void* node, uint32_t count);  // 0x10223670
void* NocaseWStringDestruct(uint32_t* self, int16_t deleteFlag);          // 0x10222020 ~basic_string<wchar_t, nocase_wchar_traits>; answers self
void ArgumentDestroy(Argument* argument);                                 // 0x102224e8 std::__destroy<Argument>
void ArgumentDestroyRange2(Argument* first, Argument* last);              // 0x10222458 std::__destroy<Argument*>
void ArgumentVectorDestroy(void* vector);                                 // 0x102254e4 std::__destroy<std::vector<Argument>>
void ArgumentVectorDestroyRange(void* first, void* last);                 // 0x102253cc std::__destroy<std::vector<Argument>*>
void* ArgVectorPtrCdequeDestruct(ArgVectorPtrCdeque* self, int16_t deleteFlag);   // 0x10224ea4 ~__cdeque; answers self
void ArgumentDequeTearDown(MslArgumentDeque* self);                       // 0x1022462c
}  // namespace v51126
namespace v51126_callee {
bool NocaseLess(const void* a, const void* b);              // 0x10225d2c operator< on two nocase wide strings
bool WStringLess(const void* a, const void* b);             // 0x10225e8c operator< on two wide strings
uint32_t NocaseInsertNodeAt(void* tree, uint32_t parent, bool left, bool leftmost, const void* key);    // 0x10228458 (answers the node)
uint32_t WStringInsertNodeAt(void* tree, uint32_t parent, bool left, bool leftmost, const void* key);   // 0x10227c78 (answers the node)
void OperatorDelete(void* p);                               // 0x10238218
void WStringDtor(void* string, int16_t flag);               // 0x101468f8
void StringDtor(void* string, int16_t flag);                // 0x1000221c
void ArgumentVectorTearDown(void* vector);                  // 0x10201310
void CdequeDecrement(MslArgumentCursor* cursor);            // 0x101f4c84 iterator::operator-- (the index is the second word)
void CursorAdd(MslArgumentCursor* cursor, int32_t count);   // 0x101f2bec iterator::operator+=
}  // namespace v51126_callee

// ---- Unit v5342: SaveBuddy 0x1021c75c (answers 0 or 1; the member is void like SaveIgnore's). Uses the ignore_callee fakes and this one.
bool OnlineSaveBuddy(COnlineControl* self, const char* path);   // 0x1021c75c
namespace buddy_callee {
void Throw(const char* typeName, void* object, void* destructorVector);   // 0x10238680 __throw(type string, exception object, destructor vector)
}
