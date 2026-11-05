// The online sign-up and password reset (the online-server fix `signup`): a player makes their own online account in the
// game window, and sets a new password with a code e-mailed to them, which the original never offered (EA made accounts on
// its website). The online login panel (src/game/ui/ui_onlineLogin.cpp) gets "Create account" and "Forgot password?"
// buttons; they open popups built from the login panel's own window script (its frame, edit boxes, buttons and fonts), with
// the game's tick box, the game's progress bar while one waits and the game's message box for a refusal or a notice. The
// player chooses the password (typed twice, masked as the login panel's) and gives a private recovery e-mail; once the
// account is made or the password set, the message box's OK fills the login panel's boxes.
//
// Each request is one message on the online server's Login service, in the original protocol's framing (online_boss.h):
// types 20 (sign-up), 22 (ask for a reset code) and 24 (set a password with the code), which no game client sends,
// answered by 21, 23 and 25 (server/PROTOCOL.md, sections 3.1 and 3.2). It goes to the server the online client talks to
// (online_boss.h Server) on the login port the game's online.sam names (gOnlineConfig.mLoginPort), through the online
// client's connection (MakeTransport), on a thread of its own; the popup asks for the answer once a frame.
//
// platform/online_signup.cpp holds the requests, the replies and the rules (no game code, so the platform tests link it);
// platform/online_signup_ui.cpp holds the popups. No password, code, e-mail or request is ever logged; the name may be.
#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class InterfaceWindow;

namespace tpw::online::signup {

inline constexpr uint16_t kTypeSignup = 20;
inline constexpr uint16_t kTypeSignupReply = 21;
inline constexpr uint16_t kTypeResetCode = 22;
inline constexpr uint16_t kTypeResetCodeReply = 23;
inline constexpr uint16_t kTypeResetSet = 24;
inline constexpr uint16_t kTypeResetSetReply = 25;

// The requests' payloads: fields of UTF-16LE units, each with its NUL, then (sign-up only) the flags.
inline constexpr size_t kSignupSize = 0x184;     // name, password, e-mail, flags
inline constexpr size_t kResetCodeSize = 0x140;  // name, e-mail
inline constexpr size_t kResetSetSize = 0x90;    // name, code, new password
inline constexpr size_t kAccountSize = 0x44;     // the made account's (and the new password's) reply: name, player id
inline constexpr size_t kFieldUnits = 0x20;      // a name or password field
inline constexpr size_t kEmailUnits = 0x80;      // the e-mail field
inline constexpr size_t kCodeUnits = 8;          // the code field
inline constexpr uint32_t kFlagAdult = 1;        // the player confirmed they are 13 or older

// The server's limits: a name (its internal/names), a password (what the login panel's password box holds, 16, of the
// characters its keys type, printable ASCII without the space; the City and Upload requests carry 19), the recovery e-mail
// (its basic address form, at most 100) and the code (6 digits).
inline constexpr size_t kNameMin = 3, kNameMax = 16;
inline constexpr size_t kPasswordMin = 6, kPasswordMax = 16;
inline constexpr size_t kEmailMax = 100;
inline constexpr size_t kCodeDigits = 6;

// The replies' statuses as one list (PROTOCOL.md 3.1 and 3.2), and the client's own for what never got a reply or never
// went. A sign-up reply's status is its value; the reset's are mapped (ParseReply).
enum class Outcome {
    Made = 0,           // made, the code request answered (sent or not), the password set
    Taken = 1,
    BadLength = 2,
    BadChars = 3,
    Reserved = 4,
    Blocked = 5,
    BadEmail = 6,
    NoAge = 7,
    IpLimit = 8,        // too many sign-ups from this address this hour
    DayLimit = 9,
    Closed = 10,
    Malformed = 11,
    ServerError = 12,
    BadPassword = 13,   // the password rule
    WeakPassword = 14,  // the name itself or a common password
    EmailFull = 15,     // the recovery e-mail has the most accounts already
    NoReset = 20,       // the server sends no reset codes (no mail set up)
    BadCode = 21,       // a wrong or expired code
    DeadCode = 22,      // the code took too many wrong tries
    CodeLimit = 23,     // too many code requests from this address this hour
    Unreachable = 100,  // no connection, or it ended or timed out before the reply
    BadReply = 101,     // a reply that is not the request's reply in the protocol's form
    Mismatch = 102,     // the two passwords typed differ (the client's own check)
};

enum class Kind { Signup, ResetCode, ResetSet };

struct Request {
    Kind kind = Kind::Signup;
    std::u16string name;
    std::u16string password;   // sign-up, and the new one of a reset
    std::u16string email;      // sign-up and the code request: the private recovery e-mail
    std::u16string code;       // the reset's code
    bool adult = false;        // sign-up: "I am 13 or older"
};

struct Result {
    Outcome outcome = Outcome::Unreachable;
    std::u16string name;   // when a sign-up or a reset is Made: the account's name as the server keeps it
    uint32_t id = 0;       // and its player id
    std::string why;       // for the log when Unreachable or BadReply (never holds a password, code or e-mail)
};

// The name rules the server checks first, for a quick answer before anything is sent: 3 to 16 characters, letters
// (Latin-1, the characters the game's fonts draw), digits, space, '-' and '_', no space at either end, a letter or digit
// somewhere, not a reserved name (read as the server reads it: case, separators and the digits that stand for letters
// ignored; accented letters are left to the server). Made when the name passes, else BadLength, BadChars or Reserved.
// The server's word list and whether a name is taken are only the server's to say.
Outcome CheckName(const std::u16string& name);
// The password rule: 6 to 16 characters from '!' to '~' (whether it is too weak is the server's to say).
bool PasswordOk(const std::u16string& password);
// The server's address form: at most 100 characters, an ASCII local part (no dot at either end or two together), one '@',
// a domain of two or more labels of letters, digits and inner hyphens.
bool EmailOk(const std::u16string& email);
// 6 digits.
bool CodeOk(const std::u16string& code);
// The checks a request passes before it is sent, in the popup's order: Made, or the first one it fails.
Outcome Check(const Request& request);

// The whole request message: the 0x14-byte header (the request's type, `language`, `seq`, the context word 0) and its
// payload.
std::vector<uint8_t> BuildRequest(const Request& request, uint16_t language, uint32_t seq);
// A whole reply message (header and payload) to the request of `kind` and `seq`: its reply type ("Bs", the length its
// payload has) with a status it can have, and for a made account or a password set a payload of 0x44 bytes holding a name
// that passes the character and length rules. BadReply (with `why`) otherwise.
Result ParseReply(Kind kind, const uint8_t* bytes, size_t size, uint32_t seq);

// Sends the request to `host`:`port` through the online client's connection (online_boss.h MakeTransport) and waits for
// its reply: the connection's own time limit, then `seconds` for the exchange. `stopping` set by another thread gives
// up. The request is pre-checked (Check) and nothing is sent when it fails.
Result Exchange(const std::string& host, uint16_t port, const Request& request, uint16_t language,
                const std::atomic<bool>& stopping, int seconds = 20);

// Exchange on a thread of its own: Start once, then Done until it answers true, then Take. The thread keeps its own share
// of the state, so a job let go of while it runs is never written to.
class Job {
public:
    Job();
    ~Job();   // asks a running request to give up (it does not wait for it)
    void Start(const std::string& host, uint16_t port, const Request& request, uint16_t language);
    bool Running() const;
    bool Done() const;
    Result Take();

private:
    struct State;
    std::shared_ptr<State> state_;
};

// Overwrites a text that held a password or a code.
void Wipe(std::u16string& text);
void Wipe(char16_t* text, size_t units);
// Wipes a request's password, e-mail and code.
void Wipe(Request& request);

// ---- The popups (platform/online_signup_ui.cpp) -----------------------------------------------------------------------------

// The ids of the login panel's "Create account" ('sign') and "Forgot password?" ('rset') buttons.
inline constexpr int32_t kCreateButtonId = 0x7369676e;
inline constexpr int32_t kForgotButtonId = 0x72736574;

// The texts of the popups, English for every language (the game's string tables have none of them).
enum class Text {
    Button,          // the login panel's "Create account"
    ForgotButton,    // the login panel's "Forgot password?"
    Title,           // the sign-up popup's title
    Confirm,         // the second password box's label
    Email,           // the e-mail box's label
    Warning,         // the line under the password boxes
    Adult,           // the tick box's label
    Waiting,         // the progress bar while a sign-up is out
    Made,            // the account made
    ForgotTitle,     // the code request popup's title
    SendCode,        // its button
    Sending,         // the progress bar while a code request is out
    Sent,            // the code request's answer
    SetTitle,        // the new password popup's title
    Code,            // its code box's label
    NewPassword,     // its password box's label
    SetButton,       // its button
    Setting,         // the progress bar while it is out
    PasswordSet,     // the password set
    Taken,
    BadLength,
    BadChars,
    NotAllowed,      // a reserved or blocked name
    BadPassword,     // the password rule
    WeakPassword,
    Mismatch,
    BadEmail,
    EmailFull,
    NoAge,
    TooMany,         // too many sign-ups from this address
    TooManyCodes,    // too many code requests from this address
    TooManyToday,    // too many sign-ups today
    Closed,
    NoReset,         // the server sends no reset codes
    BadCode,
    DeadCode,
    Failed,          // the server could not do it (a malformed request, its own error, a reply not in the protocol's form)
    Count,
};
const char16_t* GetText(Text text);
// The message for a refusal (any outcome but Made); Unreachable is the online client's own sentence (online_boss.h
// kUnreachableText).
const char16_t* RefusalText(Outcome outcome);

// The login panel's window script (`words` long) with the "Create account" and "Forgot password?" buttons added under the
// name and password boxes, which move up to make room: a copy of `original` the first time, kept for the next. `original`
// itself when its form is not the one expected (then the panel is the original's, without the buttons, and there are no
// popups).
int16_t* LoginPanelScript(int16_t* original, size_t words);
// The buttons' labels, once the login panel is built (nothing when it has no buttons).
void DecorateLoginPanel(InterfaceWindow* panel);
// The login panel's "Create account" button: opens the sign-up popup over it (nothing when a popup is open already). 1
// (the message is taken).
int32_t OpenPopup(InterfaceWindow* panel);
// The login panel's "Forgot password?" button: opens the code request popup, the same way.
int32_t OpenResetPopup(InterfaceWindow* panel);

}  // namespace tpw::online::signup
