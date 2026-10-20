// FS_MasterSave.cpp (main app): the game-wide save (CFSGlobalMasterSave) and the theme
// scan. PARTIAL: FS_ScanThemes is translated (src/FS_MasterSave.cpp); the master save's
// constructor, the keys and the theme records the island lobby reads are translated in
// src/FS_MasterSave_global.cpp (unit w32); the golden ticket bookkeeping, the current theme and the sign names are in
// src/FS_MasterSave_tickets.cpp and src/FS_MasterSave_signs.cpp (unit w89); loading and saving are not.
#pragma once
#include <cstdint>
#include <list>
#include <map>
#include <set>
#include <string>

#include "CBalanceTable.h"
#include "TbDynamicStringTemplate.h"
#include "TbError.h"

// 0x10169310 (the inventory files it under FS_MasterSave.cpp).
TbError FS_ScanThemes(const TbDynamicStringTemplate<char>& localPath);

// The balance table of a park's global save: "data:levels:<theme>:global.sam" (src/FS_MasterSave_global.cpp). Mac 0x1c bytes
// (mRows, the vptr 0x102a721c, then five value slots); its constructor and destructor are inline in the callers. The values
// the game reads are slot 3 (the keys the park costs to enter) and slot 4 (the id of the park's gate object, which names the
// park). Slot 0 is the reserved one.
class CGlobalParkDataBalance final : public CBalanceTable {
public:
    CGlobalParkDataBalance() { Init(); }
    ~CGlobalParkDataBalance() { UnInit(); }
    const BalanceDescription* GetBalanceDescription(int32_t row) override;   // 0x1016ccd8
    BalanceValue* GetBalanceValuesStart() override;                          // 0x1016cd2c
    BalanceValue mValues[5];   // 0x08  1 CanEarnTicketsInTheme, 2 CanSpendTicketsInTheme, 3 CostToEnter, 4 GateObjectId
};
extern const BalanceDescription gGlobalParkDataBalanceDescriptions[11];   // 0x10294614, the last row has type 12

// The golden ticket kinds, passed by value (CodeWarrior mangles them as by-value structs: GoldenTicketTypeLocal and the other
// two; they are plain integers in the calling convention). The values are indexes into the byte arrays below and nothing
// checks their range: a local ticket 0..5 indexes a park's mTickets (6 bytes), a global one 0..3 the save's mGlobalTickets and a
// park's mAwards and mAwardScores (4 each), a secret one 0..1 the save's mSecretTickets (2 bytes). The names of the values are
// not known.
enum GoldenTicketTypeLocal : int32_t {};
enum GoldenTicketTypeGlobal : int32_t {};
enum GoldenTicketTypeSecret : int32_t {};

// The stream object the serialising constructor of a park record takes (CFSGlobalParkData(CSerialise&, name), unit w89b). The
// original passes an empty stack object that the constructor never reads.
class CSerialise {};

// The swear filter (0x1020d1bc, OnlinePerson.cpp, untranslated): the first two lists of CFSGlobalMasterSave (offsets 0 and 0xc) are its
// word lists, so the master save's Load calls it with the master save as `this`. Init clears both lists and reads the word list
// file into the first and, when that worked, the allowed word file into the second; true when the first one was read.
class SwearFilter {
public:
    bool Init(const char* wordFile, const char* allowedFile, bool flag);   // 0x1020d1bc

    // Unit v5094 (src/SwearFilter_Filter.cpp).
    // 0x1020d2cc: walks the word list. A word starting with a space must start a word of the text, one starting with '*' may
    // sit anywhere, any other must be a whole word; a match that the allowed list does not excuse (case blind compare at the
    // match) makes the answer true and, when `replace` is set, is overwritten with '*' up to the end of the word. The text's
    // characters are changed in place. A word is empty after its prefix: it matches nothing.
    bool Filter(std::u16string& text, bool replace);
    // 0x1020cd08 (this is not read): reads the big-endian 16-bit characters of the file `path`, a line at a time (a line ends
    // with the character 0x000a; a line whose first byte is '#' is a comment), flips every character of a line (xor 0xffff) and
    // appends the line to `words`. false when `flag` is clear or the file does not open; a failed open or close raises
    // std::ios_base::failure when the stream's state meets its exception mask.
    bool LoadWordList(const char* path, std::list<std::u16string>& words, bool flag);
    // Host layout: the Mac's two lists are 0xc bytes each (offsets 0x00 and 0x0c); the filter's lists are the first two members
    // of CFSGlobalMasterSave there.
    std::list<std::u16string> mWords;      // 0x00  the words
    std::list<std::u16string> mAllowed;    // 0x0c  the words that are let through
    // Unit v51039: 0x101697bc (120, stand-in). The Mac destructor with its hidden delete flag, on the Mac's layout: the two lists at
    // +0xc then +0 are cleared (WStringListClear), and the block is freed when the flag is above 0. Answers `self`.
    static SwearFilter* Destruct(SwearFilter* self, int16_t deleteFlag);
};

// Unit v5094: what Filter and LoadWordList need outside the class.
extern uint16_t gSwearWctypeMap[256];   // 0x103bd05c (data import c/c++_shared::__wctype_map): the character class bits; 6 = a separator
extern char gIfstreamVtbl[];            // 0x102b1778 (TOC slot -0x5f04; initial word 0x102b1770)
char16_t* stristr(char16_t* haystack, char16_t* needle);   // 0x1020cbec: the first case blind match, NULL when none
// Unit v51009: stristr's table (data import c/c++_shared::__wlower_map, 0x103bcf9c: the lower case of the characters below 0x100;
// defined in src/COnlinePerson_SetDest_VoteNotifyCallback.cpp with the ASCII mapping) and the callee of SwearFilter::Init.
extern uint16_t gSwearLowerMap[256];   // 0x103bcf9c
namespace swear_callee {
void EraseList(std::list<std::u16string>* list);   // 0x1020db8c std::list::erase(begin, end) as Init calls it (weak default clears)
}
namespace NS_MacDoze {
int32_t _wcsnicmp(const char16_t* a, const char16_t* b, uint32_t count);   // glue 0x1023a660
}
// The local std::ifstream of LoadWordList (Mac 0x14c bytes at the frame's 0x170; native pointers here, Mac offsets in comments).
struct SwearIos {                // the stream's ios part (0x48 bytes on the Mac)
    const void* mVtbl;           // 0x00
    uint32_t mRdbuf;             // 0x24  0 when the stream has no buffer
    uint32_t mState;             // 0x34  1 bad, 2 eof, 4 fail
    uint32_t mMask;              // 0x38  exceptions mask
    intptr_t mOffset;            // 0x44  the offset of the ios part from the pointer at the stream's 0x00
};
struct SwearFilebuf {            // the narrow filebuf (0x40 bytes on the Mac)
    uint32_t mFile;              // 0x24  the FILE*; 0 when not open
};
struct SwearIfstream {
    SwearIos* mIos;              // 0x00  (frame 0x170)
    int32_t mGcount;             // 0x04
    const void* mVtbl;           // 0x08
    SwearFilebuf mFilebuf;       // 0x0c  (frame 0x17c)
    SwearIos mIosStorage;        // 0x4c  (frame 0x1bc)
};
// Callees outside the unit (fakes in the test library).
namespace swear_callee {
void IfstreamCtor(SwearIfstream* stream, int32_t flag);                 // 0x1020801c basic_ifstream()
void FilebufDtor(SwearFilebuf* buf, int16_t flag);                      // 0x100700e4
void IstreamDtor(SwearIfstream* stream, int16_t flag);                  // 0x102043a8
void IosDtor(SwearIos* ios, int16_t flag);                              // 0x1006ff50
SwearFilebuf* FilebufOpen(SwearFilebuf* buf, const char* path, int32_t mode);   // 0x10071c44, NULL when it failed
SwearFilebuf* FilebufClose(SwearFilebuf* buf);                          // 0x10071b54, NULL when it failed
void IstreamRead(SwearIfstream* stream, char* buffer, int32_t count);   // 0x1020d6b4
}

// One park's record in the master save (Mac 0xbc bytes, made by CreateNewTheme or by the master save's loader). The record
// keeps the golden tickets the player has won in the park, the park's balance table (read once, on the first question
// that needs it) and the sign names the player gave the park's gate.
class CFSGlobalParkData {
public:
    explicit CFSGlobalParkData(const TbDynamicStringTemplate<char>& name);   // 0x1016bbc4
    int32_t HowManyTickets() const;                                // 0x1016c0e0: the local tickets won (of 6)
    int32_t KeyCostToEnter();                                      // 0x1016c3bc: the keys the park costs, -1 without a balance file
    // 0x1016c43c: makes the balance table when there is none and reads the park's global.sam into it. false (and no table)
    // when the file cannot be read.
    bool CheckBalanceFile();
    // 0x1016c57c: the park's name: the player's sign names when there are any, else the name of the park's gate object (the
    // ride database's type name). Writes nothing when the balance file cannot be read.
    void GetFullName(char16_t* buffer, uint32_t size);
    // 0x1016c7e8 (src/FS_MasterSave_signs.cpp, unit w89): the first line of the gate sign. With player-given sign names the
    // first one (the last character dropped when the name ends in two hyphens), else the ride database's text for the gate
    // object; an empty text when the balance file cannot be read.
    void GetSignNameA(char16_t* buffer, uint32_t size);
    void GetSignNameB(char16_t* buffer, uint32_t size);            // 0x1016ca34: the second line, the same way (no hyphen rule)
    void SetSignNameA(const char16_t* text);                       // 0x1016cbe8: at most 0x21 characters, and the sign flag set
    void SetSignNameB(const char16_t* text);                       // 0x1016cc60

    // unit w89 (src/FS_MasterSave_tickets.cpp). No index is range checked.
    ~CFSGlobalParkData();                                          // 0x1016bcd4: takes the balance table down (nonvirtual)
    bool HaveTicket(GoldenTicketTypeLocal type) const;             // 0x1016c158: the byte, as a truth value
    bool EarnedTicket(GoldenTicketTypeLocal type);                 // 0x1016c1ac: sets the byte; false when it was exactly 1 already
    bool AwardInThisPark(GoldenTicketTypeGlobal type) const;       // 0x1016c220
    int32_t ScoreForAward(GoldenTicketTypeGlobal type) const;      // 0x1016c280: the score, -1 when the award is not in this park
    void ClearAward(GoldenTicketTypeGlobal type);                  // 0x1016c2f8
    void SetAward(GoldenTicketTypeGlobal type, int32_t score);     // 0x1016c354

    // unit w89b (src/FS_MasterSave_park.cpp)
    // 0x1016bb04: the loader's constructor: no balance table and the theme's name; the rest is read by Serialise (it is left as it is).
    CFSGlobalParkData(CSerialise& serialise, const TbDynamicStringTemplate<char>& name);
    // 0x1016bd9c: writes (save != 0) or reads the record's tickets, awards and their scores, the two sign names and the two flags; 0 on
    // success, 1 when a value could not be written or read. `version` is not used.
    int32_t Serialise(void* file, int32_t save, uint32_t version);

    uint8_t mTickets[6];                    // 0x00  the local golden tickets won in this park (0 or 1 each)
    uint8_t mAwards[4];                     // 0x06  which global awards were won in this park
    int32_t mAwardScores[4];                // 0x0c  their scores
    CGlobalParkDataBalance* mBalance;       // 0x1c  NULL until CheckBalanceFile has read the file
    TbDynamicStringTemplate<char> mName;    // 0x20  the theme's name, which is also its folder
    char16_t mSignNameA[0x21];              // 0x34  the first line of the park's gate sign, as the player typed it
    char16_t mSignNameB[0x21];              // 0x76  the second line
    uint8_t mHasSignNames;                  // 0xb8  1 once a sign name was set
    uint8_t mAllResearchCompleted;          // 0xb9  1 once all the park's research is done (HasAllResearchBeenCompleted)
};

// The game-wide save: what the player owns across parks (src/FS_MasterSave_global.cpp). Mac 0x4c bytes; the two string lists, the
// set of bought things and the theme map are Metrowerks library containers there (a list is 0xc bytes, a set 0xc and a map
// 0x10, with the end node inside) and standard containers here.
class CFSGlobalMasterSave {
public:
    CFSGlobalMasterSave();                 // 0x101694dc
    ~CFSGlobalMasterSave();                // 0x1016997c (nonvirtual)
    // 0x10169a88: no global tickets, nothing spent, no extra keys, not an easy mode user, the swear filter on, no secret
    // tickets, and the first-time-player flag set.
    void ResetGlobalData();
    bool IsEasyModeUser() const;           // 0x1016a4fc
    int32_t Load(const char* path);        // 0x1016a6c8: 1
    bool IsFirstTimePlayer();              // 0x1016ba70 (src/FS_MasterSave_tickets.cpp)
    void ClearFirstTimePlayer();           // 0x1016bab8
    // 0x1016b47c: the record of the theme `name`, made (and its balance file read) when there is none; NULL when the
    // theme's global.sam cannot be read.
    CFSGlobalParkData* GetInfoOn(const TbDynamicStringTemplate<char>& name);
    // 0x1016b5a0: a new record for the theme, put in the map (a record already there stays and keeps the name: the new
    // record is returned but is not in the map, and a trace line says so).
    CFSGlobalParkData* CreateNewTheme(const TbDynamicStringTemplate<char>& name);
    // 0x10169ea0: the golden keys the player holds: a third of the golden tickets won (the global ones, the local ones of
    // every park whose balance file reads, and the secret ones), and the extra keys.
    int32_t HowManyKeys() const;

    // unit w43
    void GiveExtraKey();   // 0x1016a4b4

    // unit w89 (src/FS_MasterSave_tickets.cpp): the golden tickets, what they buy, the current theme, the sign names and flags.
    // The global ticket bytes count 4 (not the 6 ResetGlobalData clears), the secret ones 2.
    int32_t HowManyTickets() const;                         // 0x10169b24: global tickets won + the local ones of the parks whose balance file reads
    int32_t HowManyTicketsIncludingSecrets() const;         // 0x10169c24: the same + the secret ones
    int32_t SpendingPower() const;                          // 0x10169d68: the same + the secret ones - mTicketsSpent
    bool SpendTicketOn(uint16_t thing, int32_t cost);       // 0x10169fe8: true (and the cost spent) when the thing was not bought yet
    bool HasBeenBought(uint16_t thing) const;               // 0x1016a094
    bool HaveTicket(GoldenTicketTypeLocal type) const;      // 0x1016a12c: the current theme's (no NULL check)
    bool HaveTicket(GoldenTicketTypeSecret type) const;     // 0x1016a1a4
    // The three EarnedTicket answer 0 when the ticket was won already, else CheckTicketTypeWon's answer (the global one: 4 when
    // the award moved to the current park, see the definition).
    int32_t EarnedTicket(GoldenTicketTypeLocal type);       // 0x1016a200: through the current theme's record (no NULL check)
    int32_t EarnedTicket(GoldenTicketTypeGlobal type, int32_t score);   // 0x1016a2a0
    int32_t EarnedTicket(GoldenTicketTypeSecret type);      // 0x1016a420
    // 0x1016b788: 1 when no key was won by this ticket (no keys at all, or the tickets are not a multiple of 3), else 3 when a
    // park costs exactly the keys held (it can be entered now) and 2 when none does.
    int32_t CheckTicketTypeWon();
    // 0x1016b224: makes the theme `name` the current one (a record is made for a new one); its balance file is read.
    void ChangeCurrentTheme(const TbDynamicStringTemplate<char>& name);
    void SetSignNameA(const char16_t* text);                // 0x1016b31c: the current theme's, when there is one
    void SetSignNameB(const char16_t* text);                // 0x1016b388
    void SetSwearFilter(bool on);                           // 0x1016b3f4
    bool IsSwearFilterOn() const;                           // 0x1016b438
    void ClearThemeMap();                                   // 0x1016b6dc: deletes every theme record, then empties the map (mCurrent is left)
    bool HasAllResearchBeenCompleted();                     // 0x1016b94c: every theme's mAllResearchCompleted is set (true for no theme)
    void SetAllResearchCompletedInCurrentTheme();           // 0x1016ba04

    // unit w48
    void SetEasyModeUser(int32_t easyMode);   // 0x1016a540 (src/FS_MasterSave_easy.cpp): the low byte of the argument into the byte at 0x24
    // 0x1016a584, 272 bytes (untranslated): writes the master save to the file `path`; 1 when it was written.
    int32_t Save(const char* path);

    // unit w89b (src/FS_MasterSave_file.cpp). Load (0x1016a6c8, above) resets the save, empties the theme map and reads the file at `path`
    // (a version below 0xc is refused); 1 when it was read. Serialise (0x1016aa08) is the body of the file: the tickets, the keys, the flags,
    // the theme records (a name and a CFSGlobalParkData each), the player settings (CPlayerSettings::SerialiseLocal of gPlayerSettings) and
    // the bought things; 0 on success, 1 on a failed read or write.
    int32_t Serialise(void* file, int32_t save, uint32_t version);

    std::list<std::u16string> mList00;      // 0x00  (the Mac's wide strings are 16 bits)
    std::list<std::u16string> mList0C;      // 0x0c
    uint8_t mGlobalTickets[4];              // 0x18  the global golden tickets won (0 or 1 each); ResetGlobalData clears six bytes here
    int32_t mTicketsSpent;                  // 0x1c  SpendTicketOn adds to it
    int32_t mExtraKeys;                     // 0x20  GiveExtraKey adds one
    uint8_t mEasyMode;                      // 0x24  IsEasyModeUser
    uint8_t mSwearFilter;                   // 0x25  1 after a reset
    uint8_t mSecretTickets[2];              // 0x26  the secret golden tickets won
    std::set<uint16_t> mBought;             // 0x28  the things the player has spent tickets on (SpendTicketOn, HasBeenBought)
    std::map<TbDynamicStringTemplate<char>, CFSGlobalParkData*> mThemes;   // 0x34  by theme name
    CFSGlobalParkData* mCurrent;            // 0x44  the current theme's record (ChangeCurrentTheme); not set by the constructor
    uint8_t mFirstTimePlayer;               // 0x48  1 after a reset (IsFirstTimePlayer)
};
extern CFSGlobalMasterSave* gMasterSave;   // 0x10380a8c, created on first use (original name unknown)
// The work area GetSignNameA cuts the first sign name's hyphen in (0x10380a08, 0x21 characters, just before GetFullName's at 0x10380a4a).
extern char16_t gParkSignNameScratch[0x21];

// Unit v5126: std::__tree<std::pair<const TbDynamicStringTemplate<char>, CFSGlobalParkData*>, ...map::value_compare, ...>, the theme
// map's tree (src/FS_MasterSave_global_std___tree_std_pair_std_map_TbDynamicStringTempl.cpp). Mac words are 32 bits, the host's are
// native (the Mac offsets are in the comments). The end node is the root word itself, as in MslSubscriptionTree.h.
struct ParkThemeTree {
    using Key = TbDynamicStringTemplate<char>;
    using Pair = std::pair<const Key, CFSGlobalParkData*>;
    struct node {
        node* mLeft;        // 0x00
        node* mRight;       // 0x04
        uintptr_t mParent;  // 0x08
        Pair mValue;        // 0x0c  the key first
    };
    struct Iterator { node* mNode; };                     // 0x00
    struct InsertResult { node* mNode; uint8_t mInserted; };   // 0x00, 0x04

    uint32_t mCount;    // 0x00
    node* mRoot;        // 0x04  the end node's left word
    node* mSlot8;       // 0x08  not touched by these functions
    node* mBegin;       // 0x0c  the first node, end() when empty

    node* end() { return reinterpret_cast<node*>(&mRoot); }
    Iterator find(const Key& key);                  // 0x1016cf68
    void clear();                                   // 0x1016cd78
    InsertResult insert_one(const Pair& value);     // 0x1016e0b8
    void destroy(node* subtree);                    // 0x1016d784 (stand-in)
    // Unit v51039: std::map<TbDynamicStringTemplate<char>, CFSGlobalParkData*>::~map 0x101695a8 (116, stand-in), the Mac's destructor
    // with its hidden delete flag on the map's Mac layout (the count, the root word): destroys the tree from the root when there is
    // one, frees the block when the flag is above 0. Answers `self`.
    static ParkThemeTree* Destruct(ParkThemeTree* self, int16_t deleteFlag);
    node* insert_node_at(node* parent, bool left, bool leftmost, const Pair& value);   // 0x1016eb98 (stand-in)
};
extern uint8_t gParkTreeInserted;      // 0x102a7238 (initial 1): insert_one's "was inserted" byte
extern uint8_t gParkTreeNotInserted;   // 0x102a7239 (initial 0)

// Unit v51039: std::list<std::wstring>::clear 0x1016d4e8 (140, stand-in) on the Mac's layout (Ptr32 links, the list lives in the arena in
// the tests): a count (0x00), the sentinel (0x04: its first word is its previous link, 0x08 its next link, the first node); a node is
// previous (0x00), next (0x04), the string (0x08). Nothing is done for a count of 0.
struct MacWStringList {
    uint32_t mCount;          // 0x00
    uint32_t mSentinelPrev;   // 0x04
    uint32_t mFirst;          // 0x08
};
void WStringListClear(MacWStringList* list);
// Callees the host cannot call as the Mac does (the Mac's strings and heap): weak definitions in
// src/Serialise_basic_std_list_std_basic_string_std_allocator_std_basi_4_callees.cpp, faked by the test library.
namespace masterlist_callee {
void WStringDtor(void* string, int16_t flag);   // 0x101468f8 std::wstring::~basic_string
void OperatorDelete(void* block);               // 0x10238218
}

// Unit v5544: the callees of ParkThemeTree::destroy (0x1016d784) and insert_node_at (0x1016eb98); stand-ins elsewhere, recording fakes in the tests.
namespace ParkThemeTreeCallee {
void PairDestroy(ParkThemeTree* tree, ParkThemeTree::Pair* value);                       // 0x1016dfd0 allocator<pair>::destroy (this = the tree)
void NodeDeallocate(ParkThemeTree* tree, ParkThemeTree::node* node, unsigned long count);   // 0x1016dc80 allocator<node>::deallocate (this = tree + 4)
void PairDtor(ParkThemeTree::Pair* pair, int16_t flag);                                  // 0x1016b168 pair::~pair
void PairCopy(ParkThemeTree::Pair* pair, const ParkThemeTree::Pair& value);              // 0x1016f080 pair::pair(const pair&)
void BalanceInsert(ParkThemeTree::node* node, ParkThemeTree::node* root);                // 0x10153b24 __red_black_tree::balance_insert
}

// Unit v51118 (src/TPOptions_CTimeSig_GetTimeSecond.cpp): destructors with the Mac's hidden "delete storage" short (answer: self), and the
// constructors of the std::list<std::wstring> and of the park theme tree. std::set<unsigned short> on the Mac's 32-bit layout:
class CSkyObjects;
class CRideDatabase;
class CRideBalance;
struct MacU16Set {
    uint32_t mCount;    // 0x00
    uint32_t mRoot;     // 0x04  the end node's left word
    uint32_t mSlot8;    // 0x08
    uint32_t mBegin;    // 0x0c
};
extern uint32_t gWStringListCtorWord;   // 0x102a723c (initial 0): the count the list constructor stores
extern uint32_t gParkTreeCtorWord;      // 0x102a7234 (initial 0): the word the tree constructor stores first
namespace v51118 {
void OperatorDelete(void* block);                          // 0x10238218 (weak default in the src file; a fake in the test)
void KeyStringDtor(void* key, int16_t flag);               // 0x102382f0 TbDynamicStringTemplate<char>::~ (weak default; a fake in the test)
void SetTreeDestroy(MacU16Set* set, uint32_t node);        // 0x1016e65c std::__tree<unsigned short>::destroy (a fake in the test)
CSkyObjects* SkyObjectsDtor(CSkyObjects* self, int16_t deleteFlag);          // 0x1015b61c
MacU16Set* U16SetDtor(MacU16Set* self, int16_t deleteFlag);                  // 0x101696f8
MacWStringList* WStringListDtor(MacWStringList* self, int16_t deleteFlag);   // 0x1016985c
CRideDatabase* RideDatabaseDtor(CRideDatabase* self, int16_t deleteFlag);    // 0x10154660
CRideBalance* RideBalanceDtor(CRideBalance* self, int16_t deleteFlag);       // 0x101544ec
void WStringListCtor(MacWStringList* list);                                  // 0x1016d63c
void ParkThemeTreeCtor(ParkThemeTree* tree, const uint8_t* compare);         // 0x1016e37c
}
