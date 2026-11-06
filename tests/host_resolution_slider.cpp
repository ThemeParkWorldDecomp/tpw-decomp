// host_resolution_slider.cpp: the options panel's size slider and shape buttons in the launcher (the port's fix resslider,
// include/TPSettings.h). DoScroll, CPlayerSettings::SetResolution, PortApplyGameSize and PortShowShape are the real ones, cut
// from src/game/ui/ui_parkStatus.cpp, src/game/TPSettings.cpp and src/game/ui/ui_gamesize.cpp, and the stops, the closest stop,
// the wait and the resampling are the real ones of include/UI_Base.h; the windows, the clock and PortChangeGameSize are
// stand-ins. Each shape must have its list, the size the panel opened at and the screen's size on the shape they are, a shape
// button must keep the closest size and stand down, the text beside the slider must be the exact size, the size must change
// only when the thumb stops (a new shape only once the button is up), Cancel's size must come back, and every setting an
// older version saved must keep its size. Run by ctest ("resolution_slider"); made only with the port's fixes, the original
// has none of this. Exits nonzero on a failure.
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "fit.h"
#include "InterfaceScrollBar.h"
#include "InterfaceWindow.h"
#include "platform/samsutils.h"
#include "TPMain.h"
#include "TPSettings.h"
#include "UI_Base.h"
#include "ui_controls.h"
#include "ui_main.h"
#include "ui_mainmenu.h"

CPlayerSettings gPlayerSettings;
char16_t gUIMainConvertBuffer[0x4000];
InterfaceWindow* gSetOnOffRoot;
int32_t gOptionsPanelF0C;
char16_t gOptionsPanelNumberFormats[16];
float gUIWideAspect = 1.6f;
int32_t gScreenWidth = 0;
int32_t gScreenHeight = 0;

void CPlayerSettings::Init() {}
CPlayerSettings::~CPlayerSettings() {}
void CPlayerSettings::SetGraphicsDetail(eDETAIL) {}
void CPlayerSettings::SetSoundQuality(int32_t) {}
void SOUND_SetSfxVolume(uint32_t) {}
void SOUND_SetMusicVolume(uint32_t) {}
void SOUND_SetSpeechVolume(uint32_t) {}

namespace {

constexpr int32_t kSliderId = 0x1d4d5;
unsigned char gWindowBytes[64];   // the windows, one byte apart by id; the stand-ins never read them
InterfaceWindow* WindowOf(int32_t id) { return reinterpret_cast<InterfaceWindow*>(gWindowBytes + (id - kSliderId)); }
std::string gReadout;
int32_t gLabelWrites = 0;         // text that went anywhere but the readout
int32_t gSliderSentValue = -1;
int32_t gSliderMax = -1;
int32_t gThumbPlaced = 0;
int32_t gButtonState[kUISizeShapeCount] = {};
uint64_t gNowMicroseconds = 0;
int16_t gButtons = 0;
std::vector<std::pair<int32_t, int32_t>> gChanges;
int gFailures = 0;

void Check(bool ok, const char* what) {
    std::printf("%s: %s\n", ok ? "ok" : "FAILED", what);
    if (!ok) ++gFailures;
}

void CheckEqual(int32_t got, int32_t want, const char* what) {
    if (got != want) std::printf("  got %d, wanted %d\n", got, want);
    Check(got == want, what);
}

// Written out here, not worked out: the slider's list at each shape (0 4:3, 1 16:10, 2 16:9).
constexpr int32_t kListed[kUISizeShapeCount][10][2] = {
    {{512, 384}, {640, 480}, {800, 600}, {1024, 768}, {1280, 1024}, {1600, 1200}, {2048, 1536}, {2560, 1920}, {3200, 2400}, {4096, 3072}},
    {{614, 384}, {768, 480}, {960, 600}, {1280, 800}, {1440, 900}, {1680, 1050}, {1920, 1200}, {2560, 1600}, {3200, 2000}, {3840, 2400}},
    {{682, 384}, {854, 480}, {1066, 600}, {1280, 720}, {1366, 768}, {1600, 900}, {1920, 1080}, {2560, 1440}, {3200, 1800}, {3840, 2160}}};
// What a shape button keeps, from each listed size: at 4:3, at 16:10 and at 16:9.
constexpr int32_t kKept[30][8] = {
    {512, 384, 512, 384, 614, 384, 682, 384},        {640, 480, 640, 480, 768, 480, 854, 480},
    {800, 600, 800, 600, 960, 600, 1066, 600},       {1024, 768, 1024, 768, 1280, 800, 1366, 768},
    {1280, 1024, 1280, 1024, 1440, 900, 1600, 900},  {1600, 1200, 1600, 1200, 1920, 1200, 1920, 1080},
    {2048, 1536, 2048, 1536, 2560, 1600, 2560, 1440}, {2560, 1920, 2560, 1920, 2560, 1600, 3200, 1800},
    {3200, 2400, 3200, 2400, 3840, 2400, 3840, 2160}, {4096, 3072, 4096, 3072, 3840, 2400, 3840, 2160},
    {614, 384, 512, 384, 614, 384, 682, 384},        {768, 480, 640, 480, 768, 480, 854, 480},
    {960, 600, 800, 600, 960, 600, 1066, 600},       {1280, 800, 1280, 1024, 1280, 800, 1366, 768},
    {1440, 900, 1280, 1024, 1440, 900, 1600, 900},   {1680, 1050, 1600, 1200, 1680, 1050, 1920, 1080},
    {1920, 1200, 1600, 1200, 1920, 1200, 1920, 1080}, {2560, 1600, 2560, 1920, 2560, 1600, 2560, 1440},
    {3200, 2000, 3200, 2400, 3200, 2000, 3200, 1800}, {3840, 2400, 3200, 2400, 3840, 2400, 3840, 2160},
    {682, 384, 512, 384, 614, 384, 682, 384},        {854, 480, 640, 480, 768, 480, 854, 480},
    {1066, 600, 800, 600, 960, 600, 1066, 600},      {1280, 720, 1024, 768, 1280, 800, 1280, 720},
    {1366, 768, 1024, 768, 1280, 800, 1366, 768},    {1600, 900, 1280, 1024, 1440, 900, 1600, 900},
    {1920, 1080, 1600, 1200, 1920, 1200, 1920, 1080}, {2560, 1440, 2048, 1536, 2560, 1600, 2560, 1440},
    {3200, 1800, 2560, 1920, 3200, 2000, 3200, 1800}, {3840, 2160, 3200, 2400, 3840, 2400, 3840, 2160}};
// The sizes the older settings mean, as the versions that saved them opened them: width, height, preset, shape.
constexpr int32_t kStopCount = 30;
constexpr int32_t kAll[kStopCount][4] = {
    {512, 384, 1, 0},   {614, 384, 1, 1},   {682, 384, 1, 2},   {640, 480, 2, 0},   {768, 480, 2, 1},   {854, 480, 2, 2},
    {800, 600, 3, 0},   {960, 600, 3, 1},   {1066, 600, 3, 2},  {1024, 768, 4, 0},  {1228, 768, 4, 1},  {1366, 768, 4, 2},
    {1280, 1024, 5, 0}, {1638, 1024, 5, 1}, {1820, 1024, 5, 2}, {1600, 1200, 6, 0}, {1920, 1200, 6, 1}, {2134, 1200, 6, 2},
    {2048, 1536, 7, 0}, {2560, 1440, 8, 2}, {2458, 1536, 7, 1}, {2560, 1600, 8, 1}, {2730, 1536, 7, 2}, {2560, 1920, 8, 0},
    {3200, 1800, 9, 2}, {3200, 2000, 9, 1}, {3200, 2400, 9, 0}, {3840, 2160, 10, 2}, {3840, 2400, 10, 1}, {4096, 3072, 10, 0}};

std::string Size(int32_t wide, int32_t high) { return std::to_string(wide) + " x " + std::to_string(high); }

// As the options panel does it when it opens (CreateOptionsPanel).
void OpenPanel(int32_t wide, int32_t high, int32_t stored) {
    gPortLiveResolution = true;
    gPortLargestGameSize = 0;
    gScreenWidth = wide;
    gScreenHeight = high;
    gPortPanelWide = wide;
    gPortPanelHigh = high;
    gPortWantedWide = wide;
    gPortWantedHigh = high;
    gPlayerSettings.mStored.mResolution = static_cast<CPlayerSettings::eRESOLUTION>(stored);
    gPlayerSettings.mCurrent.mResolution = static_cast<CPlayerSettings::eRESOLUTION>(stored);
    gReadout.clear();
    gLabelWrites = 0;
    gSliderSentValue = -1;
    gChanges.clear();
    PortShowShape(UI_NearestShape(wide, high));
}

int32_t Current() { return static_cast<int32_t>(gPlayerSettings.mCurrent.mResolution); }

int32_t StopOf(int32_t wide, int32_t high) {
    for (int32_t i = 0; i < gUISizeStopCount; ++i)
        if (gUISizeStops[i].mWidth == wide && gUISizeStops[i].mHeight == high) return i;
    return -1;
}

void TestNearestShape() {
    std::printf("-- the shape a size is\n");
    for (int32_t shape = 0; shape < kUISizeShapeCount; ++shape)
        for (const auto& size : kListed[shape]) CheckEqual(size[0] == 1280 && size[1] == 1024 ? shape : UI_NearestShape(size[0], size[1]), shape, "every listed size is its own shape");
    CheckEqual(UI_NearestShape(1280, 1024), 0, "5:4 is 4:3, the original's own size");
    CheckEqual(UI_NearestShape(1228, 768), 1, "1228 x 768 is 16:10");
    CheckEqual(UI_NearestShape(1820, 1024), 2, "1820 x 1024 is 16:9");
    CheckEqual(UI_NearestShape(3456, 2234), 1, "this Mac's screen is 16:10");
    CheckEqual(UI_NearestShape(3440, 1440), 2, "21:9 is 16:9");
    CheckEqual(UI_NearestShape(1080, 1920), 0, "upright is 4:3");
}

void TestStops() {
    std::printf("-- the stops at each shape\n");
    UISizeStop stops[kUISizeStopLimit];
    CheckEqual(kUISizeStopLimit, 12, "room for a list, the opening size and the screen");
    for (int32_t shape = 0; shape < kUISizeShapeCount; ++shape) {
        for (float aspect : {0.0f, 1.6f, 16.0f / 9.0f}) {
            gUIWideAspect = aspect;   // the shape running must not filter it
            CheckEqual(UI_SizeStops(shape, 0, 0, 0, 0, stops), 10, "ten stops at a shape");
            for (int32_t i = 0; i < 10; ++i) {
                CheckEqual(stops[i].mWidth, kListed[shape][i][0], "the width, in order");
                CheckEqual(stops[i].mHeight, kListed[shape][i][1], "the height, in order");
            }
        }
        CheckEqual(stops[0].mHeight, 384, "down to the game's smallest height");
        for (int32_t i = 1; i < 10; ++i)
            Check(stops[i - 1].mWidth * stops[i - 1].mHeight < stops[i].mWidth * stops[i].mHeight, "each bigger than the last");
    }
    gUIWideAspect = 1.6f;
    const auto stop = [&](int32_t at, int32_t wide, int32_t high, const char* what) {
        CheckEqual(stops[at].mWidth, wide, what);
        CheckEqual(stops[at].mHeight, high, what);
    };
    std::printf("-- the size the panel opened at\n");
    CheckEqual(UI_SizeStops(1, 1228, 768, 0, 0, stops), 11, "an older size off the list: a stop at its shape");
    stop(3, 1228, 768, "where it sorts");
    stop(4, 1280, 800, "the list goes on after it");
    CheckEqual(UI_SizeStops(0, 1228, 768, 0, 0, stops), 10, "and none at the others");
    CheckEqual(UI_SizeStops(2, 1228, 768, 0, 0, stops), 10, "and none at the others");
    CheckEqual(UI_SizeStops(2, 2730, 1536, 0, 0, stops), 11, "2730 x 1536 at 16:9");
    stop(8, 2730, 1536, "after 2560 x 1440");
    CheckEqual(UI_SizeStops(2, 1920, 1080, 0, 0, stops), 10, "on the list: no second stop");
    CheckEqual(UI_SizeStops(2, 1234, 500, 0, 0, stops), 11, "a size of no shape stands at the nearest");
    stop(2, 1234, 500, "itself");
    std::printf("-- the screen's own size\n");
    CheckEqual(UI_SizeStops(1, 0, 0, 3456, 2234, stops), 11, "this Mac's screen at 16:10");
    stop(9, 3456, 2234, "after 3200 x 2000");
    stop(10, 3840, 2400, "before 3840 x 2400");
    CheckEqual(UI_SizeStops(0, 0, 0, 3456, 2234, stops), 10, "not at 4:3");
    CheckEqual(UI_SizeStops(2, 0, 0, 3456, 2234, stops), 10, "not at 16:9");
    CheckEqual(UI_SizeStops(2, 0, 0, 1920, 1080, stops), 10, "1920 x 1080 is listed already");
    CheckEqual(UI_SizeStops(1, 0, 0, 2560, 1700, stops), 11, "3:2 at 16:10");
    CheckEqual(UI_SizeStops(0, 0, 0, 1500, 1200, stops), 11, "5:4 at 4:3");
    for (int32_t shape = 0; shape < kUISizeShapeCount; ++shape) {
        CheckEqual(UI_SizeStops(shape, 0, 0, 3440, 1440, stops), 10, "21:9: the UI has no layout for it, no stop");
        CheckEqual(UI_SizeStops(shape, 0, 0, 1080, 1920, stops), 10, "upright: no stop");
    }
    CheckEqual(UI_SizeStops(1, 1000, 625, 3456, 2234, stops), 12, "both at once: the most there can be");
}

void TestClosest() {
    std::printf("-- the closest stop\n");
    UISizeStop stops[kUISizeStopLimit];
    for (const auto& row : kKept) {
        for (int32_t shape = 0; shape < kUISizeShapeCount; ++shape) {
            const int32_t count = UI_SizeStops(shape, 0, 0, 0, 0, stops);
            const int32_t at = UI_ClosestStop(stops, count, row[0], row[1], 0);
            CheckEqual(stops[at].mWidth, row[2 + shape * 2], "the size kept, every listed size to every shape: width");
            CheckEqual(stops[at].mHeight, row[3 + shape * 2], "and height");
        }
    }
    const int32_t count = UI_SizeStops(2, 0, 0, 0, 0, stops);
    CheckEqual(stops[UI_ClosestStop(stops, count, 3840, 2160, 2048)].mWidth, 1920, "a limit of 2048: the closest that fits, 1920 x 1080");
    CheckEqual(stops[UI_ClosestStop(stops, count, 3840, 2160, 16384)].mWidth, 3840, "a limit past it: itself");
    CheckEqual(stops[UI_ClosestStop(stops, count, 3840, 2160, 100)].mWidth, 682, "nothing fits: the first");
    CheckEqual(UI_ClosestPreset(1920, 1080, 16.0f / 9.0f), 5, "1920 x 1080's preset at 16:9: the one that makes 1820 x 1024");
    CheckEqual(UI_ClosestPreset(512, 384, 0.0f), 1, "512 x 384's: the first");
    CheckEqual(UI_ClosestPreset(3840, 2160, 16.0f / 9.0f), 10, "3840 x 2160's: the last");
    CheckEqual(UI_ClosestPreset(1024, 768, 0.0f), 4, "and the presets are themselves");
}

void TestShapeClick() {
    std::printf("-- a shape button\n");
    OpenPanel(1920, 1080, PortResolutionSetting(6, 2));
    CheckEqual(gUISizeStopCount, 10, "the panel opens on the running shape's stops");
    CheckEqual(gSliderMax, 9, "the slider has that many");
    CheckEqual(gSliderSentValue, 6, "with the thumb on the running size");
    Check(gReadout == "1920 x 1080", "and the size beside it");
    CheckEqual(gButtonState[2], 1, "16:9 stands down");
    CheckEqual(gButtonState[0] + gButtonState[1], 0, "and no other");
    CheckEqual(Current(), PortResolutionSetting(6, 2), "opening it chooses nothing");
    const int32_t placed = gThumbPlaced;
    PortShowShape(1);
    CheckEqual(gThumbPlaced, placed + 1, "a click places the thumb");
    CheckEqual(gSliderMax, 9, "at the new list");
    CheckEqual(gSliderSentValue, 6, "on the closest size");
    CheckEqual(gUISizeStops[6].mHeight, 1200, "which is 1920 x 1200");
    Check(gReadout == "1920 x 1200", "named at once");
    CheckEqual(gPortWantedWide, 1920, "and asked for: width");
    CheckEqual(gPortWantedHigh, 1200, "and height");
    CheckEqual(Current(), PortResolutionExact(1920, 1200), "and chosen");
    CheckEqual(gButtonState[1], 1, "16:10 stands down");
    CheckEqual(gButtonState[0] + gButtonState[2], 0, "and no other");
    PortShowShape(0);
    Check(gReadout == "1600 x 1200", "16:10 to 4:3, the same height");
    CheckEqual(gButtonState[0], 1, "4:3 stands down");
    CheckEqual(gButtonState[1] + gButtonState[2], 0, "and no other");
    PortShowShape(0);
    Check(gReadout == "1600 x 1200", "the same button again changes nothing");
    CheckEqual(gButtonState[0], 1, "and it stays down, though the click put it up");
    PortShowShape(2);
    Check(gReadout == "1920 x 1080", "and back where it opened");
    CheckEqual(Current(), PortResolutionSetting(6, 2), "where nothing is left to save");
    std::printf("-- a size the panel opened at stays on offer at its shape\n");
    OpenPanel(1228, 768, PortResolutionSetting(4, 1));
    CheckEqual(gUISizeStopCount, 11, "one more stop");
    Check(gReadout == "1228 x 768", "named");
    PortShowShape(2);
    CheckEqual(gUISizeStopCount, 10, "not at 16:9");
    Check(gReadout == "1366 x 768", "the same height");
    PortShowShape(1);
    CheckEqual(gUISizeStopCount, 11, "back at 16:10");
    Check(gReadout == "1228 x 768", "the same height again");
    CheckEqual(Current(), PortResolutionSetting(4, 1), "the stored setting");
    std::printf("-- a limit\n");
    OpenPanel(1024, 768, 0);
    gPortLargestGameSize = 3000;
    gPortWantedWide = 4096;   // (asked for just now, and past it: not for this machine)
    gPortWantedHigh = 3072;
    PortShowShape(2);
    CheckEqual(gUISizeStops[gSliderSentValue].mWidth, 2560, "the shape's closest that fits");
    Check(gReadout == "2560 x 1440", "named");
    gPortLargestGameSize = 0;
}

void TestSliding() {
    std::printf("-- moving the slider\n");
    OpenPanel(1366, 768, PortResolutionSetting(4, 2));
    Check(gReadout == "1366 x 768", "the text names the running size when the panel opens");
    CheckEqual(Current(), PortResolutionSetting(4, 2), "and opening it changes no setting");
    CheckEqual(gLabelWrites, 0, "the row's label is left alone");
    for (int32_t stop = 0; stop < 10; ++stop) {
        options_panel::DoScroll(kSliderId, stop);
        const int32_t wide = kListed[2][stop][0];
        const int32_t high = kListed[2][stop][1];
        Check(gReadout == Size(wide, high), "the text is the stop's exact size");
        CheckEqual(gPortWantedWide, wide, "the size asked for is the stop's");
        CheckEqual(gPortWantedHigh, high, "both ways");
        if (wide != 1366) CheckEqual(Current(), PortResolutionExact(wide, high), "the setting holds it");
        CheckEqual(gSliderSentValue, 4, "the slider stays where it was put");
    }
    CheckEqual(static_cast<int32_t>(gChanges.size()), 0, "the slider itself changes no size");
    options_panel::DoScroll(kSliderId, 4);
    Check(gPlayerSettings.mCurrent.mResolution == gPlayerSettings.mStored.mResolution, "back on the panel's size, nothing to save");
    options_panel::DoScroll(kSliderId, -5);
    Check(gReadout == "682 x 384", "under the range: the first stop");
    options_panel::DoScroll(kSliderId, 50);
    Check(gReadout == "3840 x 2160", "over it: the last");
    std::printf("-- 1920 x 1080\n");
    options_panel::DoScroll(kSliderId, 6);
    Check(gReadout == "1920 x 1080", "it has a stop");
    CheckEqual(Current(), PortResolutionExact(1920, 1080), "and is what is saved");
    int32_t wide = 0;
    int32_t high = 0;
    Check(PortResolutionSize(Current(), wide, high), "which reads back");
    CheckEqual(wide, 1920, "1920");
    CheckEqual(high, 1080, "1080");
    std::printf("-- a game started with --resolution\n");
    OpenPanel(960, 600, PortResolutionSetting(4, 1));
    CheckEqual(Current(), PortResolutionSetting(4, 1), "opening the panel leaves the saved choice alone");
    options_panel::DoScroll(kSliderId, 2);
    CheckEqual(Current(), PortResolutionSetting(4, 1), "and so does a slider back on the start's size");
    options_panel::DoScroll(kSliderId, 3);
    CheckEqual(Current(), PortResolutionExact(1280, 800), "another size is a choice to save");
    std::printf("-- a running size that is not on the list\n");
    OpenPanel(1000, 625, PortResolutionSetting(4, 1));
    CheckEqual(gUISizeStopCount, 11, "it has its own stop");
    Check(gReadout == "1000 x 625", "the text names it");
    const int32_t at = StopOf(1000, 625);
    options_panel::DoScroll(kSliderId, at + 1);
    Check(gReadout == "1280 x 800", "the next stop");
    options_panel::DoScroll(kSliderId, at);
    Check(gReadout == "1000 x 625", "back on it");
    CheckEqual(Current(), PortResolutionSetting(4, 1), "the setting is what is stored");
    CheckEqual(gPortWantedWide, 1000, "and the size asked for is it");
}

void TestLimit() {
    std::printf("-- the machine's framebuffer limit, which is not the screen\n");
    OpenPanel(1366, 768, PortResolutionSetting(4, 2));
    gPortLargestGameSize = 16384;
    options_panel::DoScroll(kSliderId, 9);
    Check(gReadout == "3840 x 2160", "16384: the top stop is fine");
    CheckEqual(gSliderSentValue, 4, "and the thumb stays where the test left it");
    OpenPanel(1366, 768, PortResolutionSetting(4, 2));
    gPortLargestGameSize = 2048;
    gSliderSentValue = -1;
    options_panel::DoScroll(kSliderId, 9);
    CheckEqual(gSliderSentValue, 4, "2048: more than that sends the thumb back to the running stop");
    CheckEqual(gPortWantedWide, 1366, "and the size asked for stays the running one");
    CheckEqual(Current(), PortResolutionSetting(4, 2), "and chooses nothing");
    Check(gReadout == "1366 x 768", "and the text stays");
    gPortLargestGameSize = 0;
    Check(PortSizeFits(16384, 16384), "0: not known, not refused");
}

void TestStopping() {
    std::printf("-- the size changes when the thumb stops\n");
    OpenPanel(1280, 800, PortResolutionSetting(4, 1));
    gButtons = 0;
    gNowMicroseconds = 5000000;
    PortApplyGameSize();
    CheckEqual(static_cast<int32_t>(gChanges.size()), 0, "nothing asked for: nothing happens");
    for (int32_t stop = 4; stop <= 9; ++stop) {
        options_panel::DoScroll(kSliderId, stop);
        PortApplyGameSize();
        gNowMicroseconds += 100000;   // 100 ms on each: a drag going past them
        PortApplyGameSize();
    }
    CheckEqual(static_cast<int32_t>(gChanges.size()), 0, "a drag past the stops changes no size");
    gNowMicroseconds += 49000;
    PortApplyGameSize();
    CheckEqual(static_cast<int32_t>(gChanges.size()), 0, "149 ms on the last stop: not yet");
    gNowMicroseconds += 2000;
    PortApplyGameSize();
    CheckEqual(static_cast<int32_t>(gChanges.size()), 1, "151 ms: the size changes");
    CheckEqual(gChanges.back().first, 3840, "to the stop's width");
    CheckEqual(gChanges.back().second, 2400, "and height");
    gNowMicroseconds += 1000000;
    PortApplyGameSize();
    CheckEqual(static_cast<int32_t>(gChanges.size()), 1, "and only once");
    std::printf("-- a shape button waits the same way\n");
    gChanges.clear();
    PortShowShape(2);
    PortApplyGameSize();
    CheckEqual(static_cast<int32_t>(gChanges.size()), 0, "not at the click itself");
    gNowMicroseconds += 200000;
    PortApplyGameSize();
    CheckEqual(static_cast<int32_t>(gChanges.size()), 1, "150 ms on");
    CheckEqual(gChanges.back().first, 3840, "3840");
    CheckEqual(gChanges.back().second, 2160, "2160");
    PortShowShape(1);
    gNowMicroseconds += 200000;
    PortApplyGameSize();
    PortApplyGameSize();
    std::printf("-- Cancel\n");
    gChanges.clear();
    gPortWantedWide = gPortPanelWide;   // as OptionsPanelMsg does it
    gPortWantedHigh = gPortPanelHigh;
    PortApplyGameSize();
    gNowMicroseconds += 200000;
    PortApplyGameSize();
    CheckEqual(static_cast<int32_t>(gChanges.size()), 1, "the size the panel opened at comes back");
    CheckEqual(gChanges.back().first, 1280, "1280");
    CheckEqual(gChanges.back().second, 800, "800");
    std::printf("-- with the mouse button held\n");
    gChanges.clear();
    gButtons = 1;
    options_panel::DoScroll(kSliderId, 9);
    PortApplyGameSize();
    gNowMicroseconds += 200000;
    PortApplyGameSize();
    CheckEqual(static_cast<int32_t>(gChanges.size()), 1, "the same shape changes under the drag");
    CheckEqual(gChanges.back().first, 3840, "3840");
    gButtons = 0;
    PortShowShape(0);
    gButtons = 1;
    PortApplyGameSize();
    gNowMicroseconds += 200000;
    PortApplyGameSize();
    gNowMicroseconds += 5000000;
    PortApplyGameSize();
    CheckEqual(static_cast<int32_t>(gChanges.size()), 1, "a new shape waits for the button, however long");
    Check(gReadout == "3200 x 2400", "with the text already naming it");
    gButtons = 0;
    PortApplyGameSize();
    CheckEqual(static_cast<int32_t>(gChanges.size()), 1, "not in the tick it comes up: the panel has not heard of the release yet");
    PortApplyGameSize();
    CheckEqual(static_cast<int32_t>(gChanges.size()), 2, "and changes on the next");
    CheckEqual(gChanges.back().first, 3200, "3200");
    CheckEqual(gChanges.back().second, 2400, "2400");
    std::printf("-- back and forth\n");
    gChanges.clear();
    for (int32_t round = 0; round < 40; ++round) {
        options_panel::DoScroll(kSliderId, round % 2 == 0 ? 0 : 8);
        PortApplyGameSize();
        gNowMicroseconds += 200000;
        PortApplyGameSize();
    }
    CheckEqual(static_cast<int32_t>(gChanges.size()), 40, "each stop that stands for 150 ms is one change");
    gPortWantedWide = 0;
    gPortWantedHigh = 0;
}

void TestShapes() {
    std::printf("-- the same shape, to within the rounding of the sizes\n");
    Check(UI_SameShape(1228, 768, 3840, 2400), "16:10 and 16:10");
    Check(UI_SameShape(1366, 768, 3840, 2160), "16:9 and 16:9, rounding and all");
    Check(UI_SameShape(1024, 768, 4096, 3072), "4:3 and 4:3");
    Check(!UI_SameShape(1228, 768, 1366, 768), "16:10 and 16:9 are two");
    Check(!UI_SameShape(1024, 768, 1228, 768), "4:3 and 16:10 are two");
    Check(!UI_SameShape(1024, 768, 3840, 2160), "4:3 and 16:9 are two");
    for (int32_t i = 0; i < kStopCount; ++i)
        for (int32_t j = 0; j < kStopCount; ++j)
            // (1280 x 1024 is the one 4:3 preset that is 5:4: it is alike only with itself)
            Check(UI_SameShape(kAll[i][0], kAll[i][1], kAll[j][0], kAll[j][1]) ==
                      (i == j || (kAll[i][3] == kAll[j][3] && kAll[i][0] != 1280 && kAll[j][0] != 1280)),
                  "every pair of stops: alike only at one shape");
    CheckEqual(static_cast<int32_t>(UI_ShapeAspect(0) * 1000), 0, "4:3 keeps the presets' own sizes");
    CheckEqual(static_cast<int32_t>(UI_ShapeAspect(1) * 1000), 1600, "16:10");
    CheckEqual(static_cast<int32_t>(UI_ShapeAspect(2) * 1000), 1777, "16:9");
}

void TestSettingMeansPreset() {
    std::printf("-- what a saved setting means\n");
    for (int32_t preset = 1; preset <= kUISizePresetCount; ++preset) {
        CheckEqual(PortResolutionPreset(kPortResolutionChosen + preset, 10), preset, "an older version's choice gives its preset back");
        CheckEqual(PortResolutionShape(kPortResolutionChosen + preset), -1, "and names no shape: the launcher's stands");
        for (int32_t shape = 0; shape < kUISizeShapeCount; ++shape) {
            CheckEqual(PortResolutionPreset(PortResolutionSetting(preset, shape), 10), preset, "a choice gives its preset back");
            CheckEqual(PortResolutionShape(PortResolutionSetting(preset, shape)), shape, "and its shape");
        }
    }
    for (int32_t old = 0; old <= 3; ++old) {
        CheckEqual(PortResolutionPreset(old, 10), 10, "the game's own 0 to 3: the start's default");
        CheckEqual(PortResolutionShape(old), -1, "and no shape");
    }
    CheckEqual(PortResolutionPreset(kPortResolutionChosen, 7), 7, "no preset 0");
    CheckEqual(PortResolutionPreset(kPortResolutionChosen + 11, 7), 7, "a preset past the list");
    CheckEqual(PortResolutionPreset(PortResolutionSetting(1, 3), 7), 7, "a shape past the list");
    CheckEqual(PortResolutionShape(PortResolutionSetting(1, 3)), -1, "names none");
    CheckEqual(PortResolutionPreset(-1, 7), 7, "a damaged setting");
    CheckEqual(PortResolutionShape(-1), -1, "names none");
    CheckEqual(PortResolutionSetting(8, 2), 0x408, "2560 x 1440 as it stands in Config.tcf");
    CheckEqual(PortResolutionSetting(4, 1), 0x304, "1228 x 768");
    std::printf("-- the sizes the older settings mean: as they did\n");
    for (const auto& size : kAll) {
        int32_t wide = 0;
        int32_t high = 0;
        UI_PresetSize(PortResolutionPreset(PortResolutionSetting(size[2], size[3]), 0), UI_ShapeAspect(PortResolutionShape(PortResolutionSetting(size[2], size[3]))), wide, high);
        CheckEqual(wide, size[0], "a saved preset and shape: the width");
        CheckEqual(high, size[1], "and height");
        CheckEqual(PortResolutionSize(PortResolutionSetting(size[2], size[3]), wide, high), false, "and it is no exact size");
    }
    std::printf("-- an exact size\n");
    for (const auto& list : kListed) {
        for (const auto& size : list) {
            const int32_t setting = PortResolutionExact(size[0], size[1]);
            int32_t wide = 0;
            int32_t high = 0;
            Check(PortResolutionSize(setting, wide, high), "reads back");
            CheckEqual(wide, size[0], "the width");
            CheckEqual(high, size[1], "the height");
            CheckEqual(PortResolutionPreset(setting, 7), 7, "a version that knows only the older two takes it for none");
            CheckEqual(PortResolutionShape(setting), -1, "and no shape");
        }
    }
    int32_t wide = 77;
    int32_t high = 77;
    CheckEqual(PortResolutionSize(0, wide, high), false, "0 is none");
    CheckEqual(PortResolutionSize(3, wide, high), false, "the game's own 3 is none");
    CheckEqual(PortResolutionSize(-1, wide, high), false, "a damaged setting is none");
    CheckEqual(PortResolutionSize(kPortResolutionExact, wide, high), false, "no size is none");
    CheckEqual(PortResolutionSize(PortResolutionExact(320, 240), wide, high), false, "under the smallest is none");
    CheckEqual(PortResolutionSize(PortResolutionExact(16384, 8192), wide, high), false, "past any size is none");
    CheckEqual(PortResolutionSize(PortResolutionExact(1080, 1920), wide, high), false, "upright is none");
    CheckEqual(wide, 77, "and none leaves the size alone");
    CheckEqual(PortResolutionExact(1920, 1080), 0x43c00438, "1920 x 1080 as it stands in Config.tcf");
}

void TestResampling() {
    std::printf("-- a surface's picture follows its new size\n");
    const uint8_t from[4] = {1, 2, 3, 4};
    uint8_t up[16] = {};
    UI_ResamplePixels(from, 2, 2, up, 4, 4, 1);
    const uint8_t wantUp[16] = {1, 1, 2, 2, 1, 1, 2, 2, 3, 3, 4, 4, 3, 3, 4, 4};
    bool same = true;
    for (int i = 0; i < 16; ++i) same = same && up[i] == wantUp[i];
    Check(same, "twice the size: each pixel four times");
    uint8_t down[1] = {};
    UI_ResamplePixels(up, 4, 4, down, 1, 1, 1);
    CheckEqual(down[0], 1, "one pixel: the first");
    const uint16_t wide[2] = {0x1234, 0x5678};
    uint16_t two[4] = {};
    UI_ResamplePixels(reinterpret_cast<const uint8_t*>(wide), 2, 1, reinterpret_cast<uint8_t*>(two), 4, 1, 2);
    CheckEqual(two[1], 0x1234, "two bytes a pixel: whole pixels");
    CheckEqual(two[2], 0x5678, "the second");
    uint8_t untouched[1] = {9};
    UI_ResamplePixels(nullptr, 2, 2, untouched, 1, 1, 1);
    CheckEqual(untouched[0], 9, "no picture: nothing");
}

void TestOriginal() {
    std::printf("-- a program without the launcher: the Mac's panel\n");
    gPortLiveResolution = false;
    gReadout.clear();
    gPlayerSettings.mStored.mResolution = static_cast<CPlayerSettings::eRESOLUTION>(3);
    gPlayerSettings.mCurrent.mResolution = static_cast<CPlayerSettings::eRESOLUTION>(3);
    gPortWantedWide = 0;
    options_panel::DoScroll(kSliderId, 0);
    CheckEqual(Current(), 1, "0: 640 x 480");
    options_panel::DoScroll(kSliderId, 50);
    CheckEqual(Current(), 2, "50: 800 x 600");
    options_panel::DoScroll(kSliderId, 100);
    CheckEqual(Current(), 3, "100: 1024 x 768");
    CheckEqual(gPortWantedWide, 0, "and no size is asked for");
    Check(gReadout.empty(), "and no text beside the slider");
}

}  // namespace

// The stand-ins for what the real PortApplyGameSize calls: the clock, and the change itself, which is played rather than tested here.
uint64_t SamsUtilities::UTimer::GetAbsolute() { return gNowMicroseconds; }
int16_t UI_GetButtonFlags() { return gButtons; }
void PortChangeGameSize(int32_t width, int32_t height) {
    gChanges.emplace_back(width, height);
    gScreenWidth = width;
    gScreenHeight = height;
}

InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) { return WindowOf(id); }
void InterfaceScrollBar::SetMinMax(int32_t, int32_t max) { gSliderMax = max; }
int16_t InterfaceScrollBar::ReCalcThumbPos(int32_t, int32_t) {
    ++gThumbPlaced;
    return 0;
}
int32_t InterfaceButton::SetState(int8_t state, int8_t) {
    gButtonState[reinterpret_cast<unsigned char*>(this) - reinterpret_cast<unsigned char*>(WindowOf(kPortShapeButton))] = state;
    return 0;
}

const char16_t* UIMain_GetString(int32_t) {
    gUIMainConvertBuffer[0] = 0;
    return gUIMainConvertBuffer;
}

namespace options_callee {
void WindowSetText(InterfaceWindow* window, char16_t* text) {
    if (window != WindowOf(kPortSizeReadout)) {
        ++gLabelWrites;
        return;
    }
    gReadout.clear();
    for (; *text != 0; ++text) gReadout += static_cast<char>(*text);
}
void WindowSetValue(InterfaceWindow*, int32_t value) { gSliderSentValue = value; }
}  // namespace options_callee

// The shape buttons' hit areas through the real mouse's mapping (fit::WindowToGame, the same as MouseToGame in platform/window.cpp;
// the panel's unit box is that of the running shape): a window point over a button is that button at every size in the windows the
// game makes and keeps, and the gap between two is no button.
int32_t ButtonAt(int32_t wide, int32_t high, double gameX, double gameY) {
    const int32_t box = wide * 3 > high * 4 ? high * 4 / 3 : wide;
    const int32_t left = (wide - box) / 2;
    const double unitX = (gameX - left) * 2048.0 / box;
    const double unitY = gameY * 1536.0 / high;
    if (unitY < 363 || unitY >= 432) return -1;
    for (int32_t i = 0; i < 3; ++i)
        if (unitX >= 405 + i * 114 && unitX < 405 + i * 114 + 108) return i;
    return -1;
}
void TestHitAreas() {
    const struct {
        double wide;
        double high;
    } windows[] = {{1677, 1258}, {2012, 1258}, {2056, 1156}, {1728, 1117}, {1000, 750}, {1920, 1080}, {3840, 2160}, {1024, 768}};
    for (int32_t shape = 0; shape < kUISizeShapeCount; ++shape) {
        for (int32_t n = 0; n < kUISizeListLength; ++n) {
            const int32_t wide = kUISizeLists[shape][n].mWidth;
            const int32_t high = kUISizeLists[shape][n].mHeight;
            const int32_t box = wide * 3 > high * 4 ? high * 4 / 3 : wide;
            const int32_t left = (wide - box) / 2;
            for (const auto& window : windows) {
                const tpw::fit::Rect fitted = tpw::fit::Letterbox(window.wide, window.high, wide, high);
                for (int32_t i = 0; i < 3; ++i)
                    for (const int32_t unit : {405 + i * 114 + 8, 405 + i * 114 + 54, 405 + i * 114 + 100}) {
                        double wx = 0, wy = 0, gx = 0, gy = 0;
                        tpw::fit::GameToWindow(fitted, wide, high, left + unit * box / 2048.0, 397 * high / 1536.0, wx, wy);
                        tpw::fit::WindowToGame(fitted, wide, high, wx, wy, gx, gy);
                        Check(ButtonAt(wide, high, gx, gy) == i, "a window point over a shape button is that button");
                    }
                for (const int32_t unit : {516, 630, 400, 745}) {
                    double wx = 0, wy = 0, gx = 0, gy = 0;
                    tpw::fit::GameToWindow(fitted, wide, high, left + unit * box / 2048.0, 397 * high / 1536.0, wx, wy);
                    tpw::fit::WindowToGame(fitted, wide, high, wx, wy, gx, gy);
                    Check(ButtonAt(wide, high, gx, gy) == -1, "the gaps beside the shape buttons are no button");
                }
            }
        }
    }
}

int main() {
    gSetOnOffRoot = WindowOf(kSliderId);
    TestNearestShape();
    TestStops();
    TestClosest();
    TestShapes();
    TestShapeClick();
    TestHitAreas();
    TestSliding();
    TestLimit();
    TestStopping();
    TestSettingMeansPreset();
    TestResampling();
    TestOriginal();
    std::printf("%s\n", gFailures == 0 ? "all ok" : "FAILURES");
    return gFailures == 0 ? 0 : 1;
}
