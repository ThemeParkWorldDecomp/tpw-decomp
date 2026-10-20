// TPMain.cpp (main app 0x10000208..0x10001c80, static initialiser 0x10003220): the
// application object ThemeParkWorld, a SamsUtilities::UGamework, and the program entry.
//
// UGamework::Run (platform layer) calls Initialise once, then Process once per pass of
// its event loop, dispatching mouse, key and window events to the Handle* overrides.
// Process runs the remaining start-up on its first call (text, CD check, resources,
// engine, game system) and then ticks the online system and GAMESYSTEM_Tick.
#pragma once
#include <cstdint>

#include "FS_Online.h"
#include "TbDynamicStringTemplate.h"
#include "platform/interfacelib.h"
#include "platform/samsutils.h"

class ThemeParkWorld : public SamsUtilities::UGamework {
public:
    ThemeParkWorld() : UGamework(0x318) {}   // inline in main
    ~ThemeParkWorld() override;               // 0x1000028c

    bool Initialise() override;               // 0x10000320
    void Shutdown() override;                 // 0x10000604
    bool Process() override;                  // 0x10000680
    void HandleUpdateWindow(GrafPort* window) override;   // 0x100008f8
    void HandleTyping(char character, char keyCode, uint32_t modifiers, bool down) override;  // 0x10000940
    void HandleMouseMoved(const SamsUtilities::UEvent& event) override;   // 0x10000cb0
    void HandleMouseDown(const SamsUtilities::UEvent& event, SamsUtilities::MouseButton button) override;  // 0x10000d38
    void HandleMouseUp(const SamsUtilities::UEvent& event, SamsUtilities::MouseButton button) override;    // 0x10000e90
    void HandleMouseWheel(const SamsUtilities::UEvent& event, int32_t delta) override;   // 0x10000f94
    void HandleActivateWindow(GrafPort* window, bool active) override;   // 0x1000104c
    void HandleCommand(int32_t command, uint32_t modifiers) override;    // 0x10001170

    // The event's position in the UI's 2048 x 1536 virtual screen (0x10000b54).
    void GetUIMousePos(const SamsUtilities::UEvent& event, int16_t& x, int16_t& y);
    void LoadSystemBalanceFiles();   // 0x1000129c
    bool IsCdPresent();              // 0x100017b8
    // 0x10001b6c. A member in the original that uses no member (its callers pass any
    // `this`), so static here.
    static bool LookForCd();

    // The "CD missing" dialog: it ends by itself as soon as the CD is found.
    class CdDialog : public SamsUtilities::UDialog {
    public:
        explicit CdDialog(const SamsUtilities::UResourceId& id) : UDialog(id, true), mFound(false) {}  // inline
        ~CdDialog() override {}      // deleting copy 0x10001a30
        bool Filter(const SamsUtilities::UEvent& event, int16_t& itemHit) override;  // 0x100016e4

        bool mFound;   // 0x28
    };
};

// The original `main` (0x10000208, called by the runtime's __start with no arguments),
// renamed so the platform layer can own the process entry point.
int ThemeParkWorldMain();
// Copies the servers and ports of the online balance table into gOnlineConfig (0x10001494).
void SetInitialOnlineValues();

// Globals in TPMain.cpp's part of the BSS (original names unknown). Most are shared by
// the start-up code of other files.
extern ThemeParkWorld* gThemeParkWorld;   // 0x102b2bb0
extern int32_t gTPMain_102b2bac;          // 0x102b2bac, zeroed by Initialise, no reader found
extern const char* gAppName;              // 0x102b2ba8, the application name (UGamework::mName)
extern int32_t gTPMain_102b2ba4;          // 0x102b2ba4, zeroed by Initialise, no reader found
// 0x102b2b4c: start-up state. 1 initialised (Initialise), 2 quit requested (Process
// returns false), 4 first-frame start-up done, 8 game window active.
extern uint32_t gTPWFlags;
extern uint8_t gNoSavedSettings;          // 0x102b2b44, set when save:config.tcf did not load
// 0x102b2b48: game-wide option bits, set by the start-up and settings code (EngineStartup,
// CGameType keeps the game-type bits). Only the bits GameSys.cpp tests are named.
extern uint32_t gGameOptions;
// The three game-type bits CGameType keeps in step with its type (src/FS_Online_gametype.cpp; names invented): exactly one is set once a
// type has been set (and none of the others cleared by anything else).
constexpr uint32_t kGameOptionNormalGame = 0x800000;    // type 0
constexpr uint32_t kGameOptionOnlineGame = 0x1000000;   // type 1
constexpr uint32_t kGameOptionEasyGame = 0x2000000;     // type 2: a master save "easy mode" user
constexpr uint32_t kGameOptionFullScreen = 0x1;     // movies at the window's top left; no ticks while inactive
constexpr uint32_t kGameOptionFrontEnd = 0x200;     // logo, intro movie and lobby (else straight into a park)
constexpr uint32_t kGameOptionSound400 = 0x400;     // passed to SOUND_Init
extern GrafPort* gMainWindow;             // 0x102b2bb4, created by EngineStartup
extern int32_t gScreenWidth;              // 0x102b2b54, set by InitialiseGameStage3
extern int32_t gScreenHeight;             // 0x102b2b50
// Half the screen size, set by InitialiseGameStage3: the pointer's projection centre
// (CalculatePointerWorldPosn) and a second copy, which CreateOnlinePanel saves to
// 0x102b2b5c and 0x102b2b58.
extern float gViewCentreX;                // 0x102b2b6c
extern float gViewCentreY;                // 0x102b2b68
extern float gScreenCentreX;              // 0x102b2b64
extern float gScreenCentreY;              // 0x102b2b60
extern OnlineConfig gOnlineConfig;        // 0x102b2b70
// The strings __sinit_TPMain_cpp constructs (the same set as TPDialog.cpp's).
extern TbDynamicStringTemplate<char> gTPMainLanguages[16];   // 0x102b2c18
extern TbDynamicStringTemplate<char> gTPMainTitle;           // 0x102b2bf8
extern TbDynamicStringTemplate<char> gTPMainLanguage;        // 0x102b2bd8
extern TbDynamicStringTemplate<char16_t> gTPMainUpload;      // 0x102b2bb8

// Unit v5710: the MSL std::exception destructor of the main app (0x10002184). The object is one word, the vtable (0x0).
namespace TPMainExc {
struct Exception {
    const void* mVtbl;   // 0x0
};
void OperatorDelete(void* p);   // 0x10238218, the runtime's operator delete (a fake in the test)
// std::exception::~exception: NULL is left alone; else the vtable is stored and a flag above 0 (signed 16 bit) deletes. Answers this.
Exception* ExceptionDtor(Exception* self, int16_t flag);   // 0x10002184 (108)
}  // namespace TPMainExc

// Unit v51047: the program entry 0x102381a4 (68 bytes, src/std_basic_istream_basic_istream.cpp): the runtime start-up, main, exit(0).
void entry();
void RuntimeInit();               // 0x10238134 (a fake in the test)
void RuntimeExit(int32_t code);   // exit 0x1023a0a8 (a fake in the test)
