// The online server's park downloads (a visit): the game's FTPClient (src/game/online/FTPClient.cpp, the Mac's main application)
// fetches a published park from the online server's FTP service (decomp-onlinesrv's server/PROTOCOL.md section 7: read only,
// anonymous). On the port its thread and sockets are stand-ins, so with an online server set (online_boss.h's Active) the client's
// Connect, Download and Disconnect hand their work here (the ftpthread port fix in FTPClient.cpp; platform/online_library.cpp joins
// the two), and the answers go back to the client's control on the game's thread, from boss::Pump.
//
// How it differs from the Mac's FTPClient (each deliberate; the launcher's online-server fix):
// - The connection goes to the online server the launcher set (boss::Server), port 21, whatever host the server list names; the
//   address of a passive answer (227) is not used either, only its port, on that same host (as curl's --ftp-skip-pasv-ip does).
//   No other host is ever reached.
// - Every reply line is bounded (0x3ff bytes, the Mac's buffer), a listing too, and a file at kMaxPark bytes; a reply or a file over
//   its bound, or a server that sends nothing for kIdleMs, ends the step with an error.
// - Only a park file is downloaded (a local name ending ".LAYS" whose two last parts are the remote file's name, 36 GUID
//   characters): ride files are game data, never moved between players.
// - The file is received in memory, checked as the server checks an upload (CheckPark: PROTOCOL.md 6.1, less what only the
//   server can know), written beside its place and then moved there, so a download that fails leaves no file the game would load.
// - A path holding a CR, LF or NUL is refused (no command can be slipped into another).
//
// Only platform/online_boss_socket.cpp touches sockets (devtools/check_offline.py); the connections here are boss::MakeTransport's.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace tpw::online::boss::ftp {

constexpr size_t kMaxLine = 0x3ff;             // a reply line (the Mac's buffer)
constexpr uint32_t kMaxPark = 4u << 20;        // a park file (the server's limit)
constexpr uint32_t kMaxUnpacked = 32u << 20;   // a park's unpacked modules (the server's limit)
constexpr int kIdleMs = 30000;                 // a server that sends nothing for so long ends the step

// The game's answers (FTPClient's: CFSOnlineSystem::FTPDownloadCallbackFn passes them to the interface as error ids).
constexpr int32_t kErrorSocket = 0x34;
constexpr int32_t kErrorClosed = 0x38;
constexpr int32_t kErrorConnect = 0x40;   // "Couldn't connect to FTP server"
constexpr int32_t kErrorGreeting = 0x41;  // the server's first answer was not 220
constexpr int32_t kErrorLogin = 0x42;     // the password was not taken (230)
constexpr int32_t kErrorAnswer = 0x43;
constexpr int32_t kErrorNoFile = 0x44;    // "Remote file doesn't exist"

// The states the answers carry (FTPClient::ProcessEvents' command words): 1 connect, 2 quit, 3 download.
constexpr int32_t kConnect = 1;
constexpr int32_t kQuit = 2;
constexpr int32_t kDownload = 3;

// The game's legal texts (gSaveLegalText, src/game/save/SaveLoad.cpp), which the joining code gives; a park whose text is not its
// language's is refused, as LoadHeader refuses it.
struct LegalTexts {
    std::u16string text[7];
};

// PROTOCOL.md 6.1's checks a client can make: the size, the version (500), the language, the legal text (zero filled), the ride
// record's last 0xe0 bytes 0, the key bytes 01 22 19 85, the flag word (0, or 1 with a 0x23a-byte header), and the BILZ block (its
// parameters, its packed size the block's, its unpacked size at most kMaxUnpacked, a stream that inflates to exactly that and ends
// with the file). Whether the park's rides are known is the game's own check (DoIHaveTheseRides). nullptr when the park passes,
// else why not.
const char* CheckPark(const uint8_t* bytes, size_t size, const LegalTexts& legal);

// What the answers are given to, on the game's thread (Pump): the client's progress, and each command's end (state, result: 0 or
// one of the errors above).
struct Reports {
    void (*progress)(void* owner, int32_t done, int32_t total) = nullptr;
    void (*done)(void* owner, int32_t state, int32_t result) = nullptr;
};

void SetReports(const Reports& reports);
void SetLegalTexts(const LegalTexts& legal);
void SetPort(uint16_t port);   // 21; the tests' and a local server's own
void SetIdleMs(int milliseconds);   // kIdleMs; the tests' own

// The client's commands. Connect makes the owner's connection (its thread); each command is done in order on that thread.
void Connect(void* owner, const std::string& user, const std::string& password);
void Download(void* owner, const std::string& remote, const std::string& localHostPath);
void Quit(void* owner);
// The connection ends (its thread closes the connection) and its answers are dropped.
void End(void* owner);
bool Has(const void* owner);

// Gives the answers that arrived (boss::Pump calls it).
void Pump();

}  // namespace tpw::online::boss::ftp
