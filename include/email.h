// email.cpp (main app): the player's e-mail. PARTIAL: the two classes CFSUserInfo holds (their constructors, AddressBook's
// destructor and bthread_mutex::lock and unlock are in src/game/online/bthread.cpp; the outbox's destructor and the two loaders are in
// src/game/online/email.cpp, unit w90); the rest is untranslated.
#pragma once
#include <cstdint>
#include <list>
#include <string>

// The e-mail the outbox is writing (76 bytes on the Mac). Only its destructor is known here: the outbox deletes the e-mail it
// holds. Unit w90.
#include "Ptr32.h"
class Outbox;
class Email {
public:
    ~Email();   // 0x1020248c (216 bytes, unit v5129, src/game/online/EmailMessage.cpp); the outbox calls it with the delete flag 1
    // Unit v5129 (src/game/online/EmailMessage.cpp). The object is 0x4c bytes on the Mac; every field is kept as a 32-bit word at the Mac's offset
    // (strings and lists are one to three words there; the pointers to objects the code makes are Ptr32 arena offsets).
    static Email* Destruct(Email* self, int16_t deleteFlag);                               // 0x1020248c: the body, deleting form when the flag is > 0
    void Reset();                                                          // 0x10202588
    bool AttachData(const char* path);                                     // 0x10202950: 0 when the attachment was made
    bool Send(const char* server, int16_t port, char* extra, Outbox* outbox);   // 0x10202ee4
    // Unit v5162 (src/game/online/EmailMessage.cpp)
    int32_t SendToOutbox(Outbox* outbox);   // 0x10203010: copies the sender, subject, addresses and body text into the outbox, then WriteMailData; 0 for NULL
    void BuildBody();                       // 0x102031a8: the MIME message text into mString38
    // Unit v5912 (src/game/online/EmailMessage.cpp)
    static Email* Construct(Email* self);   // 0x10202244: Email::Email(), answers self
    void Callback(uint32_t message, int32_t code, uint8_t* data);   // 0x10202a54: the mail connection's callback (data unused); the cursor is the unaligned word at +0x3e
    uint32_t mString00;     // 0x00  std::string
    uint32_t mListSize;       // 0x04  std::list<std::string>: count
    uint32_t mListPrev;       // 0x08  the sentinel's prev (the sentinel is at +8)
    uint32_t mListNext;       // 0x0c  the sentinel's next (the first node)
    uint32_t mString10;       // 0x10  std::string
    uint32_t mString14;       // 0x14  std::string
    uint8_t mF18[8];          // 0x18
    uint32_t mWString20;      // 0x20  std::wstring
    uint32_t mWString24;      // 0x24  std::wstring
    uint8_t mF28[4];          // 0x28
    Ptr32<uint8_t> mAttachment;   // 0x2c  Email::Attachment*
    uint32_t mF30;            // 0x30  cleared by Send
    Ptr32<uint8_t> mMail;     // 0x34  Mail*
    uint32_t mString38;       // 0x38  std::string
    uint8_t mF3c;             // 0x3c
    uint8_t mF3d;             // 0x3d  1 when no outbox was given
    uint8_t mF3e[2];          // 0x3e
    uint8_t mF40[4];          // 0x40
    Ptr32<Outbox> mOutbox;    // 0x44  Outbox*
    uint32_t mCallback;       // 0x48  TVector of a callback(int32_t status, int32_t code), or 0
    // Unit v5950 (src/game/online/email.cpp): the attachment, 12 bytes on the Mac.
    struct Attachment {
        static Attachment* Destruct(Attachment* self, int16_t deleteFlag);   // 0x10202894: ~Attachment, deleting form when the flag is > 0
        uint32_t mString00;     // 0x00  std::string
        uint32_t mString04;     // 0x04  std::string
        Ptr32<uint8_t> mData;   // 0x08  new[] block, or 0
        // Unit v51015 (src/game/online/Connection.cpp)
        static Attachment* Construct(Attachment* self, const char* path);   // 0x10202670: reads the file, mData = its base64 text (or 0)
    };
};

// Unit v51015: base64enc 0x10202114 (src/game/online/EmailMessage.cpp): the text of `count` bytes in 76 column lines (CR LF), ends with 0.
void base64enc(unsigned char* out, unsigned char* in, int count);

// Unit v51015: callees of Email::Attachment::Attachment outside the unit (weak defaults in the source, fakes in the test library).
namespace attach_callee {
void* Fopen(const char* path, const char* mode);                          // 0x102389b0
int32_t Fseek(void* file, int32_t offset, int32_t whence);                // 0x102388f0
int32_t Ftell(void* file);                                                // 0x10238908
void* NewArray(uint32_t size);                                            // 0x10238668 operator new[]
uint32_t Fread(void* buffer, uint32_t size, uint32_t count, void* file);  // 0x10239508
void Fclose(void* file);                                                  // 0x102388c0
void DeleteArray(void* p);                                                // 0x10238638 operator delete[]
}
class Outbox;

// Unit v5129: the callees of Email's members outside the unit (library code or untranslated; the test library fakes them). Objects the
// Mac stores as words are passed by the address of their field.
namespace email_callee {
void* OperatorNew(uint32_t size);                                    // 0x102384b8
void OperatorDelete(void* p);                                        // 0x10238218
void MailCtor(void* mail);                                           // 0x10239520 mail_shared::Mail::Mail
int32_t MailConnect(void* mail, char* server, uint16_t port, void (*callback)(int32_t, int32_t, uint8_t*, void*), void* context,
                    char* extra);                                    // 0x102395c8
void MailCbfn(int32_t a, int32_t b, uint8_t* data, void* context);  // 0x102029f4 (untranslated; its TVector is 0x10268718)
void AttachmentCtor(void* self, const char* path);                   // 0x10202670 Email::Attachment::Attachment
void AttachmentDtor(void* self, int16_t flag);                       // 0x10202894
void PtrGlue(uint32_t tvector, int32_t a, int32_t b);                // 0x1023baa8, r12 = the TVector, r3 and r4 the arguments
void StringAssign(void* str, const char* text, uint32_t length);     // 0x101b2440
void StringDtor(void* str, int16_t flag);                            // 0x1000221c
void WStringDtor(void* str, int16_t flag);                           // 0x101468f8
void ListErase(void* list, uint32_t first, void* last);              // 0x102084d8 (the result slot is not passed)
void ListClear(void* list);                                          // 0x1017fc04
void WSACleanup();                                                   // 0x102386f8
}

class Outbox {
public:
    Outbox();    // 0x10203940
    ~Outbox();   // 0x10203ba8 (276 bytes, nonvirtual, src/game/online/email.cpp): deletes mEmail when it is not NULL
    // unit w43, translated by unit w90 (src/game/online/email.cpp)
    // 0x10203ce0 (1,696 bytes): empties the three number lists, makes mF00 "<path>:outbox", opens "<mF00>:outbox.dat" and reads
    // three blocks of decimal numbers (a block ends at an empty line or the end of the file) into mF28, then mF40, then mF34.
    // A missing file creates the folder mF00 and answers whether that worked; a file that opens answers 1.
    int32_t Load(const char* path);
    void First();    // Unit v5017: 0x10205718 (unit v5950 translates it: the cursor to the first mail; answers ReadMailData's answer, 0 for no mail)
    bool Next();     // Unit v5017: 0x1020578c, untranslated: the cursor to the next mail, 0 at the end
    // Unit v5031 (all untranslated)
    void MarkToSend();        // 0x10205828
    void UnmarkToSend();      // 0x102058b4
    void MarkToDelete();      // 0x10205910
    void UnmarkToDelete();    // 0x102059a0
    // Unit v5218 (src/game/online/email.cpp)
    int32_t Send(const char* server, int16_t port, char* extra);   // 0x102059fc: 1 when the e-mail went out, else 0
    int32_t NextMail(Email* email);   // 0x10205ed0: drops the first mail, fills the e-mail with the next; 1 when it is ready
    // Untranslated callees (fakes in the test library).
    bool ReadMailData(int32_t mailId);   // 0x102049fc (Unit v5148 translates it: src/game/online/email.cpp; reads "<mF00>:%08x.dat", 1 when the file opened)
    void DeleteMail(int32_t mailId);     // 0x1020638c
    void Save();                         // 0x1020461c
    void Delete();                      // Unit v5239: 0x10206278 (unit v5950 translates it, src/game/online/email.cpp; answers like First)
    int32_t WriteMailData();             // Unit v5162: 0x10205154, untranslated (1432 bytes; fake in the test library)
    // Host layout (0x60 bytes on the Mac; MSL strings and lists are one pointer or three words there, so no size is claimed here).
    // Offsets are the Mac's, from the constructor's listing.
    std::string mF00;                      // 0x00
    std::string mF04;                      // 0x04
    std::string mF08;                      // 0x08
    std::list<std::string> mF0C;           // 0x0c
    std::string mF18;                      // 0x18
    uint8_t mF1C[0x28 - 0x1c];             // 0x1c  not written by the constructor
    std::list<uint32_t> mF28;              // 0x28  mail waits when its size (the Mac's word at +0x28) is not 0: lobby.cpp, UpdateOutboxMailIcon
    std::list<uint32_t> mF34;              // 0x34
    std::list<uint32_t> mF40;              // 0x40
    uint8_t mF4C[4];                       // 0x4c  not written by the constructor
    std::string mF50;                      // 0x50
    uint16_t mF54;                         // 0x54  25
    int32_t mF58;                          // 0x58  0
    Email* mEmail;                         // 0x5c  0; the destructor deletes it (the Mac's word at +0x5c)
};

// The address book (0x10 bytes on the Mac, a std::list of std::strings first). Its
// destructor is inline in the original (the list's clear, an MSL instance at 0x1017fc04);
// here it is declared for email.cpp to define. The constructor's second statement (the word at +0xc takes the word at +8) puts the
// cursor at the first node.
class AddressBook {
public:
    AddressBook();    // 0x102064cc
    ~AddressBook();
    // unit w43, translated by unit w90 (src/game/online/email.cpp)
    // 0x10206b2c (1,236 bytes): reads "<path>:addrbook.dat" (one address per line, an empty line is an address too), adds the
    // complaint address (gOnlineConfig.mComplainEmail, 0x102b2b94) at the end, sorts the list and removes the repeats. Answers
    // 1 when the file opened, else 0 (the complaint address is added, and the list sorted, either way).
    int32_t Load(const char* path);
    // Unit v5009: 0x102067cc (820 bytes): writes "<path>:addrbook.dat", one address per line ("%s\n"). Answers 0 when the file
    // did not open, else 1. A failed open or close raises std::ios_base::failure when the stream's state meets its exception mask.
    int32_t Save(const char* path);
    std::list<std::string> mAddresses;
    // Unit v5L45 (src/game/online/email.cpp): the cursor functions answer a pointer to the entry's string (AddressBookNode), or to an empty
    // one at the end. First puts the cursor at the first address, then answers Next; Next answers the cursor's address and moves on.
    struct AddressBookNode** First();   // 0x10206534
    struct AddressBookNode** Next();    // 0x10206594
    std::list<std::string>::const_iterator mCursor;   // 0x0c  the Mac's word: the node Next answers
    // Unit v5722: Add answers the position of the address after the sort and unique (-1 when not found); Remove answers 1 when erased.
    void Add(const std::string& address);   // 0x102065f8 (the original also answers the position in r3: AddressBookAddIndex)
    int32_t Remove(int32_t position);          // 0x10206728
};

// Unit v5012: an address book entry as RemoveAddressBookEntry reads it: the address text pointer at +0xc (a 32-bit word on the Mac).
#pragma pack(push, 4)
struct AddressBookNode {
    uint32_t mF00;
    uint32_t mF04;
    uint32_t mF08;
    const char* mText;   // 0x0c
};
#pragma pack(pop)

// Unit v5009: the output stream AddressBook::Save writes (an MSL basic_ofstream on the Mac's stack at +0xf8, its filebuf at +0x100).
struct SaveIos {                 // the stream's ios part (0x60 bytes of stream state on the Mac)
    uint8_t f00[0x24];
    uint32_t mRdbuf;             // 0x24  0 when no buffer
    uint8_t f28[0x0c];
    uint32_t mState;             // 0x34  bit 1 fail-ish, bit 4 failbit
    uint32_t mMask;              // 0x38  exceptions mask
    uint8_t f3c[0x24];
};
struct SaveStream {
    SaveIos* ios;                // Mac +0x00
    uint32_t filebufOpen;        // the filebuf's word at +0x24 (nonzero once open worked)
    void* impl;                  // host only: the std::ofstream
};
void SaveStreamCtor(SaveStream* s);                      // basic_ofstream ctor 0x10071e34
bool SaveStreamOpen(SaveStream* s, const char* name);    // filebuf::open(name, 0x10) 0x10071c44
void SaveStreamWrite(SaveStream* s, const char* text);   // operator<< 0x10207150
bool SaveStreamClose(SaveStream* s);                     // filebuf::close 0x10071b54
void SaveStreamDestroy(SaveStream* s);                   // ~filebuf, ~ostream, ~ios (0x100700e4, 0x10070000, 0x1006ff50)
void SaveStreamThrow(const char* what);                  // __throw of an ios_base::failure (a fake may return)

// Unit v5700: std::operator<<(basic_ostream<char>&, const char*) 0x10207150 (src/game/MslSupport.cpp).
#include "std_basic_istream.h"
struct MacWriteIos {                    // the stream's ios part, Mac offsets
    uint8_t pad0[0x24];
    Ptr32<MacPutStreamBuf> mStreamBuf;  // 0x24
    uint8_t pad28[4];
    int32_t mWidth;                     // 0x2c
    uint32_t mFlags;                    // 0x30 (0x2000: unitbuf)
    uint32_t mState;                    // 0x34
    uint32_t mExceptions;               // 0x38
    uint8_t pad3c[4];
    uint8_t mFill;                      // 0x40
};
struct MacWriteStream {
    Ptr32<MacWriteIos> mIos;            // 0x00
};
// Answers the stream.
MacWriteStream* OstreamWriteText(MacWriteStream* os, const char* text);   // 0x10207150

// Unit v5031 (src/game/ui/ui_onlineOutbox.cpp)
class InterfaceWindow;
extern InterfaceWindow* gOutboxSendingWindow;   // 0x10395e0c: the sending window, NULL when none
extern int32_t gMailStatusShownB;       // 0x10395e00
extern int32_t gMailStatusShownA;       // 0x10395e08
extern int32_t gMailStatusB;            // 0x10395dfc
extern int32_t gMailStatusA;            // 0x10395e04
int32_t MarkOutboxEmail(int32_t flags);   // 0x101e4004
void SendMailStatusDisplay();             // 0x101e4184
int32_t OutboxSendingMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);   // 0x101e4250

// Unit v5116: std::basic_ostream<char> ctor 0x10072828 and dtor 0x10070000. The object holds the pointer to its virtual
// ios part at +0 (a 32-bit word on the Mac, kept as Ptr32) and the ios part itself at +8. Vtable words are the addresses of
// the three data blocks below, stored as 32-bit words (the host truncates the address).
#include "Ptr32.h"
struct MacOstreamIos {          // the virtual std::ios_base part
    uint32_t mVtbl;             // 0x00
    uint8_t f04[0x38];
    uint32_t mF3c;              // 0x3c  cleared by the ctor
    uint8_t mFill;              // 0x40  widen(' ')
    uint8_t f41[3];
    int32_t mVbaseOffset;       // 0x44  (this + 8) - the ios pointer
};
struct MacOstream {
    Ptr32<MacOstreamIos> mIos;  // 0x00
    uint32_t mVtbl;             // 0x04
    MacOstreamIos mIosPart;     // 0x08
};
extern char gOstreamVt1[];      // 0x1029d0b0 (initial word 0x1029d0a8)
extern char gOstreamVt2[];      // 0x1029d0d8 (initial word 0x1029d0d0)
extern char gIosBaseVt[];       // 0x103bd034 data import __vt__Q23std8ios_base
MacOstream* BasicOstreamCtor(MacOstream* self, int16_t withVirtualBase, uint32_t streambuf);   // 0x10072828
MacOstream* BasicOstreamDtor(MacOstream* self, int16_t deleteFlags);                           // 0x10070000
// Callees outside the unit (fakes in the test library).
void OstreamIosBaseDtor(MacOstreamIos* ios, int32_t flags);   // std::ios_base::~ios_base 0x10238950
void OstreamIosInit(MacOstreamIos* ios, uint32_t streambuf);  // std::ios_base::init 0x10238968
char OstreamWiden(MacOstreamIos* ios, char c);                // basic_ios<char>::widen 0x10072960
void OstreamDelete(void* object);                             // operator delete 0x10238218

// Unit v5527 (src/game/MslSupport.cpp): std::basic_ofstream<char>, word layout of the Mac:
// +0 virtual ios pointer, +4 vtable word, +8 filebuf (file word at +0x2c of the object), +0x24 locale, +0x48 ios part.
extern char gOfstreamVt[];   // 0x1029d1a8 (initial word 0x1029d1a0)
void* BasicOfstreamCtor(void* self, int16_t withVirtualBase);   // 0x10071e34
void* BasicOfstreamDtor(void* self, int16_t flags);             // 0x1006fdc0
void OfstreamClose(void* filebuf);                              // basic_filebuf<char>::close 0x10071b54
void OfstreamLocaleDtor(void* locale, int32_t flag);            // _RefCountedPtr<__locale_imp>::~_RefCountedPtr 0x10071f34

// Unit v5642 (src/game/MslSupport.cpp): the second copy of
// std::basic_ifstream<char>::~basic_ifstream (0x1020448c). Layout as the ofstream: +0 virtual ios pointer, +8 vtable word,
// +0xc filebuf (file word at +0x30), +0x28 locale, +0x4c ios part.
extern char gIfstreamVtblC[];   // 0x102b173c (TOC slot -0x5f08; initial word 0x102b1734)
void* BasicIfstreamDtorEmail(void* self, int16_t flags);   // 0x1020448c

// Unit v5138 (src/game/online/email.cpp): Argument, the three-word command argument of the online code. The strings are
// MSL objects the callees build (online_callee in online.h), so the host keeps the words at the Mac's offsets.
class Argument {
public:
    static Argument* Construct(Argument* self, const void* text);    // 0x102011a8: Argument(std::wstring)
    static Argument* Destruct(Argument* self, int16_t deleteFlag);   // 0x1020127c: ~Argument, deleting form when the flag is > 0
    uint32_t mString;     // 0x00  std::string
    uint32_t mWString;    // 0x04  std::wstring
    uint32_t mCommand;    // 0x08  command or type number
};

// Unit v5218: callees of Outbox::Send and Outbox::NextMail outside the unit (the test library fakes them). Strings the Mac holds as
// words are passed by the address of their field.
class CFSOnlineSystem;
extern CFSOnlineSystem* gOnlineSystem;   // 0x10380ab0 (also declared in FS_Online.h)
namespace outbox_callee {
void EmailCtor(void* email);                                        // 0x10202244 Email::Email
void OnlineSystemCtor(void* system);                                // 0x1016f7d0 CFSOnlineSystem::CFSOnlineSystem
const char16_t* LoginEmailAddress(CFSOnlineSystem* system);         // 0x101718b8
const char16_t* LoginCustomerName(CFSOnlineSystem* system);         // 0x10171834
const char16_t* LoginPassword(CFSOnlineSystem* system);             // 0x10171878
int32_t LoginPlayerID(CFSOnlineSystem* system);                     // 0x101718fc
int32_t LoginCustomerID(CFSOnlineSystem* system);                   // 0x1017193c
void OperatorDeleteArray(void* p);                                  // 0x10238638
void WStringAssign(void* str, const char16_t* text, uint32_t length);   // 0x101b2510
void StringCtor(void* str, const char* text);                       // 0x10002338 (the allocator argument is not passed)
void ListInsert(void* list, void* where, const void* str);          // 0x101b25e0 (the result slot is not passed)
}

// Unit v5304 (src/game/online/Connection.cpp): CSock, the socket wrapper deriving from Sock (0x18 bytes on the Mac). The host keeps
// the Mac offsets as plain words (the vtable word is the constant the constructor loads from the TOC, 0x102b1638).
class CSock {
public:
    CSock();                                          // 0x10201f50: Sock::Sock, the vtable word, 16 bytes cleared at +8
    int32_t SendData(const char* data, int32_t count);   // 0x10202058: 0 when all was sent, 1 on NULL data or a send error
    uint32_t mVtable;     // 0x00  0x102b1638
    int32_t mSocket;      // 0x04  the socket handle (Sock's field)
    uint8_t mF08[0x10];   // 0x08  cleared by the constructor
};
namespace csock_callee {
void SockCtor(void* self);                                               // 0x1020e2a0 Sock::Sock
int32_t Send(int32_t socket, const char* data, int32_t length, int32_t flags);   // 0x1023a798 winsock send
}

// Unit v5259 (src/game/MslSupport.cpp): std::list<std::string>::unique 0x1020833c (212 bytes).
// The list is a Mac-layout block of words: +0 size, +4 the sentinel (prev +4, next +8); a node: prev +0, next +4, string rep word +8.
void StringListUnique(void* list);

// Unit v5148: the input stream Outbox::ReadMailData reads (an MSL basic_ifstream on the Mac's stack at +0x140). It is a SaveStream:
// ios is the virtual ios part (state +0x34, mask +0x38, rdbuf +0x24), filebufOpen the filebuf's word at +0x24. Callees outside the
// unit (fakes in the test library; SaveStreamClose and SaveStreamThrow are the ones declared above).
namespace outbox_callee {
void StreamCtor(SaveStream* s);                                          // basic_ifstream ctor 0x1020801c
bool StreamOpen(SaveStream* s, const char* name);                        // filebuf::open(name, 8) 0x10071c44
char StreamWiden(SaveStream* s);                                         // getloc 0x10072a34, use_facet 0x10073934, widen('\n') through 0x1023baa8
void StreamGetline(SaveStream* s, char* buffer, int32_t size, char delim);   // istream::getline 0x102079f8
void StreamDestroy(SaveStream* s);                                       // ~filebuf 0x100700e4, ~istream 0x102043a8, ~ios 0x1006ff50
}

// Unit v5162: callee of Email::BuildBody outside the unit (the test library fakes it). The string is passed by the address of its field.
namespace sendbody_callee {
void StringAppend(void* str, const char* text, uint32_t length);   // 0x1020100c std::string::append(const char*, unsigned long)
}

// Unit v5530 (src/game/online/email.cpp): Outbox::Save 0x1020461c and Outbox::WriteMailData 0x10205154.
// Callees outside the unit (fakes in the test library): the C library's time (glue 0x1023a198) and localtime (glue 0x1023a1b0).
// LocalTime answers the Mac's struct tm words: tm_mday at +0xc, tm_mon at +0x10, tm_year at +0x14.
namespace outboxwrite_callee {
int32_t Time();
const int32_t* LocalTime(const int32_t* seconds);
}

// Unit v5711 (src/game/online/email.cpp): EmailInfo, word layout of the Mac (strings and the list are MSL objects
// the callees in email_callee handle; they are passed by the address of their field).
class EmailInfo {
public:
    static EmailInfo* Destruct(EmailInfo* self, int16_t deleteFlag);   // 0x10203aec: ~EmailInfo, deleting form when the flag is > 0
    uint32_t mString00;      // 0x00  std::string
    uint32_t mString04;      // 0x04  std::string
    uint32_t mList08[3];     // 0x08  std::list<std::string>
    uint32_t mString14;      // 0x14  std::string
};

// Outbox::DeleteMail 0x1020638c: callee outside the unit (the test library fakes it): the C library's unlink (glue 0x102394d8).
namespace outboxdel_callee {
void Unlink(const char* path);
}

// Unit v5912 (src/game/online/EmailMessage.cpp): callees of Email::Email and Email::Callback outside the unit (the test library
// fakes them). Strings and lists the Mac holds as words are passed by the address of their field.
namespace emailcb_callee {
void StringInit(void* str);                                                  // 0x1007363c std::string(allocator): the word is set
void WStringInit(void* str);                                                 // 0x101e0b84 std::wstring(allocator)
void ListInit(void* list);                                                   // 0x10208abc std::list<std::string>(allocator): three words
void StringSubCtor(void* dst, const void* src, uint32_t pos, uint32_t n);    // 0x101b2918 std::string(const std::string&, pos, n)
void StringSubAssign(void* dst, const void* src, uint32_t pos, uint32_t n);  // 0x10001c80 std::string::assign(const std::string&, pos, n)
void WinsockStart(int32_t version, void* data);                              // 0x102394f0 WSAStartup
void MailDoTo(void* mail, const char* address);                              // 0x10239538
void MailDoFrom(void* mail, const char* address);                            // 0x10239550
void MailDoData(void* mail, const char* text);                               // 0x10239580
void MailQuit(void* mail);                                                   // 0x10239598
void MailDoReset(void* mail);                                                // 0x102395b0
}

// Unit v5950: std::basic_ifstream<char>::basic_ifstream() 0x1020801c (180). Word layout of the Mac (the ios part's vtable words are the
// addresses of globals, stored truncated to 32 bits on the host).
struct MacIfstream {
    Ptr32<MacOstreamIos> mIos;   // 0x00  virtual ios pointer
    uint32_t mGcount;            // 0x04
    uint32_t mVtable;            // 0x08
    uint8_t mFilebuf[0x40];      // 0x0c  basic_filebuf<char>
    MacOstreamIos mIosPart;      // 0x4c
};
extern char gIfstreamVtbl[];     // 0x102b1778 (also declared in FS_MasterSave.h)
MacIfstream* BasicIfstreamCtor(MacIfstream* self, int16_t withVirtualBase);   // 0x1020801c
namespace ifstream_char_callee {
void IstreamCtor(MacIfstream* self, int32_t flag, uint32_t streambuf);   // 0x1020a4c0 basic_istream(streambuf*) (stand-in; streambuf = arena offset of the filebuf)
void FilebufCtor(void* filebuf, uint32_t file);                          // 0x10072bbc basic_filebuf(FILE*)
}

// Unit v51057 (src/game/online/email.cpp): small functions on Mac-layout objects. An object
// is passed as a plain byte pointer (words at the Mac's offsets, 32-bit addresses); pointers answered are 32-bit words.
extern uint32_t gStringListInitWord;   // 0x102b1794 (TOC -0x3b14): the word list<string>::list copies to +0
extern uint32_t gUintListInitWord;     // 0x102b1790 (TOC -0x3b18): the word list<uint>::list copies to +0
namespace csock_callee {
void SockDtor(void* self, int16_t flag);   // 0x1020e2d4 Sock::~Sock
}
namespace v51057 {
uint32_t AddressBookFirst(uint8_t* self);    // 0x10206534: cursor (+0xc) = first (+8), then Next
uint32_t AddressBookNext(uint8_t* self);     // 0x10206594: the entry address (node + 8), or the address of gEmailBlob at the end
void OutboxUnmarkToSend(uint8_t* self);      // 0x102058b4: remove(list at +0x34, *(word at +0x4c) + 8)
void OutboxUnmarkToDelete(uint8_t* self);    // 0x102059a0: remove(list at +0x40, *(word at +0x4c) + 8)
uint32_t IosExceptions(const uint8_t* ios);  // 0x10207664: word +0x38
uint32_t* IosOrAssign(uint32_t* a, uint32_t b);   // 0x102076a0: *a |= b, answers a
uint32_t IosState(uint8_t* ios);             // 0x10207700: ios + 0x34
uint32_t IosAnd(uint32_t a, uint32_t b);     // 0x10207734
uint32_t IosRdstate(const uint8_t* ios);     // 0x10207788: word +0x34
uint32_t BasicIosTie(const uint8_t* ios);    // 0x10207964: word +0x3c
uint32_t IosGood(const uint8_t* ios);        // 0x102079b4: 1 when the state word is 0
uint8_t* IstreamSentryDtor(uint8_t* self, int16_t flag);   // 0x10207f84
uint8_t* IstreamDtor(uint8_t* self, int16_t flag);         // 0x102043a8
uint8_t* StringListCtor(uint8_t* self);      // 0x10208abc (answers nothing in the original: r3 is self)
uint8_t* UintListDtor(uint8_t* self, int16_t flag);        // 0x10203a4c
uint8_t* UintListCtor(uint8_t* self);        // 0x1020951c
uint32_t StringLess(const uint8_t* a, const uint8_t* b);   // 0x10209f04: compare(a, 0, a.size, b.begin, b.size) < 0
uint32_t StringBegin(const uint8_t* str);    // 0x1020a068: word +0xc of the rep
uint32_t StringSize(const uint8_t* str);     // 0x1020a0d4: word +0 of the rep
uint8_t* StringListDtor(uint8_t* self, int16_t flag);      // 0x1020236c
uint8_t* CSockDtor(uint8_t* self, int16_t flag);           // 0x10201fcc
}
