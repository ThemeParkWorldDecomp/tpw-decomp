// platform.h: what the platform layer's own files share. Not for game code: translated
// files include the declarations in include/platform/ and nothing from this folder.
//
// The platform layer reimplements, on SDL3 and OpenGL, the libraries Elegant Chaos put
// under the PC source for the Mac version (sams utils, macdoze, indirectx) and the few Mac
// Toolbox calls the game makes directly (analysis/platform-layer-design.md in the project
// root). It is new code, not a translation, so nothing here is differentially tested;
// platform/tests/ checks what can be checked.
#pragma once
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include "log_file.h"
#include "platform/interfacelib.h"
#include "platform/samsutils.h"

struct SDL_Window;

namespace tpw {

// Set by the launcher before the game starts.
struct Config {
    std::filesystem::path gameDir;   // the installed game: the folder that holds "data"
    std::filesystem::path userDir;   // everything the game writes (saves, settings)
    // Which edition the game folder holds (--edition): "stp", SimTheme Park (the US Mac disc), or "tpw", Theme Park
    // World (the UK/European disc, made by devtools/import_eur.py). The code is the same; the data picks the language,
    // splash, speech and logos by itself. Empty: EditionOf the game folder. The window's title follows it.
    std::string edition;
    // The four bytes of the original's "debug options" resource (MacLoadDebugSettings in
    // TPEngine.cpp): option bit 0x4, full screen, sound flag 0x400, the front end. 0xff
    // leaves the game's own choice, 1 sets the option, 0 clears it.
    uint8_t debugOptions[4] = {0xff, 0xff, 0xff, 0xff};
    bool logToStderr = true;
    int32_t maxFrames = 0;           // stop UGamework::Run after this many passes (0: never)
    bool probe = false;              // log the frame probe's counters (include/tpw_probe.h) once a pass
    // When set, every frame the game presents is also saved here as frame-NNNN.bmp (the
    // first 200): what the window showed, for checking a run without watching it.
    std::filesystem::path dumpFramesDir;
    int32_t dumpEvery = 1;           // of the frames that differ, save every Nth (a film has 15 a second)
    int32_t dumpLimit = 200;         // save at most this many pictures (--dump-limit)
    bool movies = true;              // play the game's films (when the build has a decoder)
    bool mute = false;               // no sound at all (the films' is the only sound so far)
    // Which screen the window opens on (--display): an SDL display index, or a case-blind
    // part of the display's name ("builtin" picks the MacBook's own screen). Empty: SDL's
    // choice, the main display.
    std::string display;
    // Fit the window to the screen (the default, platform/fit.h): the game draws at its own resolution into an offscreen
    // framebuffer and the window shows it scaled to the largest size the screen has room for. --no-fit: the window is as
    // large as the game in points, drawn into directly.
    bool fit = true;
    // Open the window behind the others and never take the keyboard focus or move the mouse
    // (--background): a test run on the screen someone is working on. Clicking the window
    // focuses it as usual.
    bool background = false;
    // Scripted input for a test run (--click X,Y@N): a left click at the game pixel (X, Y). The pointer moves there and
    // the button goes down on pass N of the run loop and comes up on pass N + 1 (passes count from 1, as the probe's
    // lines do); the pointer stays there until the real mouse moves.
    struct ScriptedClick {
        int32_t pass;
        int32_t x;
        int32_t y;
    };
    std::vector<ScriptedClick> clicks;
    // Scripted typing (--type TEXT@N): character i of the text goes down on pass N + 2 i and comes up on the pass after,
    // as HandleTyping sees a key (letters, digits, space, '\r' for return; the shift modifier for a capital).
    struct ScriptedTyping {
        int32_t pass;
        std::string text;
    };
    std::vector<ScriptedTyping> typing;
    // A scripted drag with the left button (--drag X0,Y0-X1,Y1@N[/K]): the pointer goes to (X0, Y0) on pass N, the button
    // goes down there on pass N + 2, the pointer moves in K equal steps (default 8) to (X1, Y1) on passes N + 3 to N + 2 + K,
    // and the button comes up there on pass N + K + 4. Game pixels, as for clicks.
    struct ScriptedDrag {
        int32_t pass;
        int32_t x0, y0, x1, y1;
        int32_t steps;
    };
    std::vector<ScriptedDrag> drags;
    // A scripted pointer move with no button (--move X,Y@N): the pointer goes to the game pixel (X, Y) on pass N and stays.
    struct ScriptedMove {
        int32_t pass;
        int32_t x;
        int32_t y;
    };
    std::vector<ScriptedMove> moves;
    // A scripted right click (--rclick X,Y@N): as --click, with the right button.
    std::vector<ScriptedClick> rightClicks;
    // A scripted key (--key NAME@N: down on pass N, up on pass N + 1) or a held key (--hold NAME@N-M: down on pass N, up on
    // pass M), sent as a real key of that SDL scancode is (HandleTyping, the key state and the events' modifiers). NAME is
    // esc, return, space, tab, cmd, ctrl, shift, option or an SDL key name (a letter, a digit, "Delete", "F1").
    struct ScriptedKey {
        int32_t scancode;
        int32_t down;
        int32_t up;
    };
    std::vector<ScriptedKey> keys;
};
Config& config();

// The scripted pointer (Config::clicks) while one is in effect: its game pixel and whether its button is held. In effect
// from a click's pass until the next real mouse motion; false when none is, and the real mouse counts.
bool ScriptedPointer(float& gameX, float& gameY, bool& buttonDown);
// Whether a scripted right click (Config::rightClicks) holds the right button now.
bool ScriptedRightButton();
// Whether a scripted key (Config::keys) holds the key of this SDL scancode now, and the SDL modifier bits those keys make.
bool ScriptedKeyDown(int32_t sdlScancode);
uint16_t ScriptedSDLModifiers();
// The SDL scancode of a --key or --hold name (0: none).
int32_t ScancodeFromKeyName(const char* name);
// The pass of the run loop being run (from 1; counted only with --frames, else 0).
int32_t CurrentPass();

// One line in the platform log ("platform: ..." on stderr, and the log file of log_file.h). The game's own log lines
// (ULogManager) and the stubs' "unimplemented" lines go through LogLine as well.
void Log(const char* format, ...) __attribute__((format(printf, 1, 2)));
void LogLine(const char* prefix, const char* text);

// ---- Paths --------------------------------------------------------------------------
// The game names files the Mac way ("data:splash:640:splash.tga", full paths starting at a
// volume) and, through macdoze, the Windows way ("data\\levels\\x.wad"). Both are mapped
// onto the game folder; files the game creates go to the user folder, which is searched
// first, so the installed game is never written to.

// The Mac path of the application's folder, with its trailing ':' (what UFolder::GetPath
// gives the game as its project directory).
extern const char kProjectDirMac[];   // "TPW:Theme Park Data:"
// The path the game looks for its CD under (TPMain.cpp).
extern const char kRemoteDirMac[];    // "Theme Park:Theme Park Data:"

// Splits a game path into its components below the game folder. Accepts ':', '\\' and '/'
// as separators, a leading project or CD prefix, and a leading separator. In a double-byte
// code page (below) a lead byte's trail byte is never a separator, as on the PC.
std::vector<std::string> SplitGamePath(const std::string& gamePath);

// The code page of the game's narrow text (code_page.cpp): 0, the shim's Latin-1, unless the
// game runs in Hebrew (1255), Japanese (932), Korean (949), TraditionalChinese (950) or
// SimplifiedChinese (936), set once at start from the game's language (UseCodePageOfLanguage
// in platform/macdoze.h). The shim's WideCharToMultiByte, MultiByteToWideChar and
// IsDBCSLeadByte follow it.
uint32_t CodePageOfLanguage(uint32_t languageId);
void SetGameCodePage(uint32_t codePage);
uint32_t GameCodePage();
bool IsLeadByte(uint32_t codePage, uint8_t byte);
bool IsGameLeadByte(uint8_t byte);
// `bytes` in `codePage` as UTF-16 and back, both strict: false when a byte or a character has
// no counterpart, or would not come back the same. Code page 0 is Latin-1.
bool DecodeCodePage(uint32_t codePage, const std::string& bytes, std::u16string& text);
bool EncodeCodePage(uint32_t codePage, const std::u16string& text, std::string& bytes);
// The shim's conversions in the game's code page: a character it lacks becomes `defaultChar`
// (and sets usedDefault), a byte it has no character for becomes '?'.
std::string NarrowGameText(const char16_t* text, size_t length, char defaultChar, bool& usedDefault);
std::u16string WidenGameText(const char* text, size_t length);
// One component of a game path as the host names it, and back. With code page 0, or a name
// of ASCII alone, the bytes are the name (as before); otherwise the name is the code page's
// text, which the host stores as UTF-8 (Mac) or UTF-16 (Windows), so a Hebrew or Japanese
// player's folder is named in its own letters. A name the code page cannot express keeps its
// bytes.
std::filesystem::path HostNameOf(const std::string& gameName);
std::string GameNameOf(const std::filesystem::path& hostName);
// A host file opened with the C runtime (its UTF-16 name on Windows, so no letter is lost).
std::FILE* OpenHostFile(const std::filesystem::path& path, const char* mode);

// The host file or folder a game path names: the user folder's copy if there is one, else
// the game folder's. Names are matched ignoring case, as on the Mac. Empty if neither
// exists.
std::filesystem::path FindHostPath(const std::string& gamePath);
// The host file an FSSpec made by FSMakeFSSpec stands for (the spec itself keeps only the
// name); empty when there is none.
std::filesystem::path HostPathOfSpec(const FSSpec* spec);
// Where a game path is created or written: always under the user folder (the folders
// above it are created).
std::filesystem::path HostPathForWriting(const std::string& gamePath);

// One entry of a merged listing of a game folder (user folder first, then the game
// folder; the dump's ".finfo" and ".rsrc" side files are left out).
struct DirEntry {
    std::string name;
    bool isDirectory;
    uint64_t size;
    uint64_t fileTime = 0;   // the last write time as a Win32 FILETIME (100 ns ticks since 1601); 0 when it cannot be read
};
std::vector<DirEntry> ListGameFolder(const std::string& gamePath);
// The edition a game folder holds, by its data:Language folder: "stp" when it has American (the US disc's), else "tpw".
std::string EditionOf(const std::filesystem::path& gameDir);

// ---- Window -------------------------------------------------------------------------
// The one window. The game holds it as a GrafPort* and reads portRect.
struct Window : GrafPort {
    SDL_Window* sdl = nullptr;
    void* glContext = nullptr;
    bool visible = false;
    bool fullScreenBox = false;   // made with procID 2: the game's full-screen window
};
Window* MainWindow();             // NULL before NewCWindow
// The window's title for the game's Pascal-string title: the game calls its window "Theme Park World" in both editions,
// which is the UK edition's name; the US edition's window is "Sim Theme Park" (its box and splash write it SimTheme
// Park; the launcher's menus and the window write the name as the game is known). The title bar shows it out of full
// screen.
std::string WindowTitle(const std::string& edition, const unsigned char* title);
// The window's content origin in screen coordinates (the Mac's "global" coordinates).
void WindowOrigin(int32_t& x, int32_t& y);
// The size of the surface the game draws into, in points (what the game counts in) and in pixels; zeros without a
// window. With the fit this is the game's resolution, four times the same pair (not the window's size).
void WindowSizes(int& pointsWide, int& pointsHigh, int& pixelsWide, int& pixelsHigh);
// The game's resolution: what GetClientRect, DirectDrawCreate and the like report. Without the fit, the window's size.
void GameSize(int& wide, int& high);
// The game sets its resolution (SetDisplayMode, MoveWindow); with the fit the window is fitted to it again.
void SetGameSize(int wide, int high);
bool FitActive();
// Alt+Enter (Windows, Linux) and the game's Esc menu button (every system): the window goes between a real full-screen
// window and the fitted window, at run time (platform/window.cpp). False when it cannot (no window, no fit, --background)
// or the system refused.
bool ToggleFullScreen();
// Whether ToggleFullScreen can act now (a window, the fit, not --background), and whether the window is full screen now
// (as the window reports it, so the system's own switch counts). The Esc menu's button shows and reads them.
bool CanToggleFullScreen();
bool IsFullScreen();
// The largest width or height this machine's OpenGL renders a frame at (platform/offscreen.cpp's LargestSize), asked of a
// hidden 1 x 1 window before the game's own exists; 0 when it cannot be asked. The launcher picks a preset that fits.
int LargestGameSize();
// The SDL display --display names (Config::display), or 0 when none was asked for or none matches; SDL's video must be
// started. The launcher's own window (launcher/ui/window.h) opens there too.
uint32_t ChosenDisplay();
// A mouse position in window points as a game pixel: through the letterbox and clamped to the game's pixels with the
// fit, unchanged without it.
void MouseToGame(double x, double y, double& gameX, double& gameY);
// Where the cursor overlay draws: the mouse in the units of a surface of surfaceWide x surfaceHigh pixels, and how many
// of those pixels a point (a game pixel with the fit) takes. False when nothing should be drawn (the mouse is outside
// the window; a frame-dumping run puts it in the middle).
struct CursorPlace {
    float x = 0.0f;
    float y = 0.0f;
    float scaleX = 1.0f;
    float scaleY = 1.0f;
    int surfaceWide = 0;
    int surfaceHigh = 0;
};
bool CursorPlacement(CursorPlace& place);
void SwapBuffers();
void DrawCursorOverlay();   // platform/cursor.cpp: the game's cursor over the frame about to be presented
// platform/ddraw.cpp: what a lock of the back buffer reads the game's picture with, as RGBA bytes, the bottom row first,
// wide x high; false when there is nothing to read. By default the game's OpenGL target (glReadPixels); tests set their
// own, and nullptr puts the default back.
using PictureReader = bool (*)(uint8_t* rgba, int wide, int high);
void SetPictureReader(PictureReader reader);

// ---- Events -------------------------------------------------------------------------
// A UEvent as the platform makes it: the fields of the Mac EventRecord the game reads.
struct Event : SamsUtilities::UEvent {
    int32_t globalX = 0;
    int32_t globalY = 0;
    uint16_t modifiers = 0;   // Mac modifier bits: 0x100 command, 0x200 shift, 0x800 option, 0x1000 control
};

// Keyboard translation (keys.cpp).
uint8_t MacKeyCodeFromScancode(int32_t sdlScancode);        // 0xff: no Mac key
int32_t ScancodeFromWindowsKey(int32_t virtualKey);         // 0: none
uint16_t MacModifiersFromSDL(uint16_t sdlModifiers);
char PlainKeyCharacter(uint8_t macKeyCode);                 // 0: no character, or AltGr held

// How soon and how often a held key repeats (key_repeat.cpp): the run loop's autoKeys, as the Mac's Event Manager
// sent them. The defaults are the fallback where the system has no setting the platform reads.
struct KeyRepeatTiming {
    uint32_t delayMs = 500;      // from the key going down to its first autoKey
    uint32_t intervalMs = 33;    // between autoKeys
};
KeyRepeatTiming SystemKeyRepeat();                                                    // the host's keyboard settings
KeyRepeatTiming WindowsKeyRepeat(uint32_t delaySetting, uint32_t speedSetting);       // SPI_GETKEYBOARDDELAY, SPEED

// The number of GDI handles (DCs, fonts, bitmaps) alive in gdi.cpp; the platform test checks that none leak.
size_t GdiLiveHandles();

// Whether the game asked for the sound channels' safe mode (SamsUtilities::USoundChannel::SetSafeMode); only kept.
bool SoundSafeMode();

}  // namespace tpw
