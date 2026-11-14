// The online server client: the Mac's "online" shared library (Client, Auth, News, City, Upload, Chat and Vote), translated, speaking the
// original protocol (BOSS framing over TCP) to a revived server, the one the original PC clients reach too. The server's side of
// the wire is decomp-onlinesrv's server/PROTOCOL.md; the library's code is the Mac's "online symbols shared" library.
//
// Off unless the launcher sets a server (SetServer, the online-server fix in launcher/main.cpp: the player's, the one tpw-port's
// online.txt names, or the built-in one): Active() is false and platform/online_library.cpp answers as before ("cannot
// connect"). With a server set, online_library.cpp's Auth, News, City and Upload hand each call here.
//
// How it differs from the Mac's library (each deliberate; the launcher's online-server fix):
// - The connection is made on the connection's own thread, so Connect answers 1 at once; a server that cannot be reached is the
//   library's "disconnected" answer (-4), not Connect's 0, and the player is told so in plain words (TakeUnreachable), with the
//   cause in the log.
// - Answers are given on the game's thread (Pump, from the game's frame loop), not on the connection's thread, so a callback
//   never runs while the game is in the middle of something else.
// - The context word of a request is a token (the request's sequence word) and the context pointer stays here: a host pointer
//   does not fit the 32-bit word.
// - Every length a reply gives is bounded: a reply over kMaxReply bytes, a reply that does not start "Bs" or a reply to no
//   request drops the connection, and the parsers never read or write past the reply or the records they fill.
// - The e-mail of the login answer is copied with the room the block has (19 characters), not 0x7f.
// - The messages' progress records are guarded by a lock (the Mac's thread and the game's both touched them with none).
// - The blocks the game frees at a count of 0 are given with a count of 2 and freed here (platform/online_lists.h).
//
// Which host a connection goes to: the server named, unless the server's own details answer (the news request's
// onlineoverride.sam) named a host for that service and the game asks for it; the upload host is the one the server lists. No
// other host is ever reached.
//
// Only platform/online_boss_socket.cpp touches sockets (devtools/check_offline.py); everything else goes through Transport, which
// the tests replace.
#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

struct ccParkFileInfo;
class TbITable;

namespace tpw::online::boss {

// ---- The framing ------------------------------------------------------------------------------------------------------------

constexpr size_t kHeader = 0x14;
constexpr uint32_t kMaxReply = 0x100000;   // a longer reply drops the connection (the Mac's library had no bound)

// The 0x14-byte header, every word big endian.
struct Header {
    uint8_t magic = 'B';
    uint8_t from = 'c';       // 'c' from the client, 's' from the server
    uint16_t type = 0;
    uint16_t param = 0;       // the language
    uint16_t status = 0;
    uint32_t length = 0;      // the bytes after the header
    uint32_t context = 0;     // echoed by the server
    uint32_t seq = 0;         // echoed by the server; the client counts from 10
};
void PutHeader(uint8_t* out, const Header& header);
Header GetHeader(const uint8_t* in);

// ---- The library's text helpers ---------------------------------------------------------------------------------------------

// Client::CreateGuidText: "{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}", the bytes in their own order (the news requests' GUID).
std::string GuidText(const uint8_t bytes[16]);
// boss_CreateGuidText(bytes, out, 0): no braces, the Microsoft layout (the first four bytes reversed, then two, then two).
std::string BossGuidText(const uint8_t bytes[16]);
// swap_to_hex: "0x", then each unit as "%02X%02X" of its low byte, then its high byte; at most `most` units (0: all).
std::string HexText(const char16_t* text, size_t most);
// swap_copy_string: at most units - 1 characters, then a NUL; the rest of the field zero.
void CopyWide(char16_t* out, size_t units, const char16_t* text);

// ---- The connection ---------------------------------------------------------------------------------------------------------

// One TCP connection (platform/online_boss_socket.cpp; the tests give their own). Used only on the connection's thread.
class Transport {
public:
    static constexpr long kWouldBlock = -2;
    virtual ~Transport() = default;
    // Connects; gives up when `stopping` is set. False when the host cannot be reached.
    virtual bool Open(const std::string& host, uint16_t port, const std::atomic<bool>& stopping) = 0;
    // Waits at most `milliseconds` until the connection can be read (bit 1) or, with wantWrite, written (bit 2); 0 when
    // neither, -1 when it failed.
    virtual int Wait(bool wantWrite, int milliseconds) = 0;
    // The bytes sent or read, kWouldBlock, or -1 when the connection failed; Receive answers 0 when the server closed it.
    virtual long Send(const uint8_t* bytes, size_t size) = 0;
    virtual long Receive(uint8_t* bytes, size_t size) = 0;
    virtual void Close() = 0;
    // Why the last Open failed, for the log: the name not found, the connection refused, no answer in time; empty when unknown.
    virtual std::string Failure() const { return {}; }
};
constexpr int kConnectSeconds = 15;   // the socket connection gives up on a host that has not answered by then
using TransportMaker = std::function<std::unique_ptr<Transport>()>;
std::unique_ptr<Transport> MakeSocketTransport();
void SetTransportMaker(TransportMaker maker);   // the tests' transport; an empty one is the sockets again
std::unique_ptr<Transport> MakeTransport();     // the tests' transport when set, else MakeSocketTransport
// The tests' stand-in for the socket connection's name lookup, so a name that is never found does not wait on the machine's DNS:
// it answers why `host` was not found, or an empty text to look the name up as usual. An empty stand-in is the resolver alone.
using NameLookup = std::function<std::string(const std::string& host)>;
void SetNameLookup(NameLookup lookup);

// ---- The setting ------------------------------------------------------------------------------------------------------------

void SetServer(const std::string& host);   // empty: off
const std::string& Server();
bool Active();

// ---- The library's calls (online_library.cpp; `owner` is the game's Auth, News, City, Upload or Vote) --------------------------

using Callback = void (*)(int32_t, int32_t, void*, void*);   // (code, request, context, block)
enum class Kind { Auth, News, City, Upload, Chat, Vote };

int32_t Connect(const void* owner, Kind kind, const char* host, uint16_t port, Callback callback);
void Release(const void* owner);
uint32_t GetCurrentMessage(const void* owner, int32_t which);
void MonitorMessage(const void* owner, uint32_t request, int32_t* done, int32_t* total);

void Login(const void* owner, uint16_t language, const char16_t* name, const char16_t* password, void* context);
// News types: 4 server news, 5 game news, 7 server details; `guid` (16 bytes) for 5 and 7.
uint32_t News(const void* owner, uint16_t type, uint16_t language, const uint8_t* guid, void* context);
void SetSessionInfo(const void* owner, uint16_t language, int32_t player, int32_t customer, const char16_t* password);
// A City request (type 100) of sub-id `sub` with its query text.
int32_t CityQuery(const void* owner, void* context, uint32_t sub, uint32_t arg, int32_t flag, const std::string& query);
void UploadParkRequest(const void* owner, const char* file, const ccParkFileInfo* info, void* context);
void UploadParkFile(const void* owner, const char* file, void* context);
// VotingClient::SendVote (PROTOCOL.md 8): type 10, SetSessionInfo's words, the game's GUID (16 bytes) as text, the park and the
// count; the request's sequence word (0 when `owner` has no connection). The answer (Vote::internalCallback2) is 0 for a vote
// counted, -2, -6, -30 or -31 for the server's refusals (status 2, 3, 4 or 5 of type 0xffff), -1 for any other status, -4 and -5.
uint32_t Vote(const void* owner, int32_t park, const uint8_t guid[16], int32_t count, void* context);

// ---- The chat (ChatClient and Chat::internalCallback2; PROTOCOL.md 10) ------------------------------------------------------

// One argument of a chat message: narrow text (a number is its decimal text) or wide text.
struct ChatToken {
    bool wide = false;
    std::string narrow;
    std::u16string text;
    static ChatToken Narrow(std::string s) { return ChatToken{false, std::move(s), {}}; }
    static ChatToken Number(int32_t n) { return Narrow(std::to_string(n)); }
    static ChatToken Wide(std::u16string s) { return ChatToken{true, {}, std::move(s)}; }
};
// (owner, code, event, arguments, context): code 100 for every reply and push (the event is the message's first word), -4 for the
// end of the connection and -5 for the server's error reply (type 2), both with event 0 and no arguments.
using ChatCallback = void (*)(const void*, int32_t, int32_t, const std::vector<ChatToken>&, void*);

// A request's payload: the command word, then each argument, 0x02 UTF-16LE 0xffff for wide text and 0x01 text 0xff for narrow;
// no end byte (the Mac's message length is the text's).
std::vector<uint8_t> ChatPayload(uint32_t command, const std::vector<ChatToken>& tokens);
// A reply's or push's payload as internalCallback2 reads it: the event word, then the arguments up to a 0x00 byte (any byte other
// than 0x00, 0x01 and 0x02 is skipped; 0xff 0xff in narrow text is one 0xff). After the first narrow argument of event 99 and the
// first wide argument of event 0x15 the rest of the payload is one more narrow argument. Unlike the Mac's, a missing word reads as
// 0 and an argument that runs past the end stops there.
void ChatParse(const uint8_t* bytes, size_t size, int32_t& event, std::vector<ChatToken>& tokens);

// Chat::Connect: the chat connection of `owner` (a game Chat object); Release lets it go.
int32_t ChatConnect(const void* owner, const char* host, uint16_t port, ChatCallback callback);
// ChatClient's requests: type 300, param 0; the request's sequence word (0 when `owner` has no connection).
uint32_t ChatRequest(const void* owner, uint32_t command, const std::vector<ChatToken>& tokens, void* context);

// Gives the answers that arrived to their callbacks, on the caller's (the game's) thread, then frees the lists the game let go of.
void Pump();

// ---- A server that cannot be reached ---------------------------------------------------------------------------------------

// What the player is shown, in every language, when the server cannot be reached.
extern const char kUnreachableText[];
// kUnreachableText as a text table of one string (platform/online_library.cpp: the game's TbITable, which this file's tests do
// not link), for the error dialog.
TbITable* UnreachableTable();
// A connection that failed, or that ended before the server's first answer (a name not found, a connection refused or not
// answered, a server that does not speak the protocol), is counted when its "disconnected" answer (-4) is given; any other
// answer clears the count. The error dialog (ui_ErrorDisplay.cpp's AddUIError, the launcher's online-server fix) takes one for
// the game's "disconnected" error and shows kUnreachableText instead. True when there was one to take.
bool TakeUnreachable();

}  // namespace tpw::online::boss
