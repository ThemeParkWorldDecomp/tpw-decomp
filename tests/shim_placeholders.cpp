// Test-only placeholders so the shim library links while parts of the game and the
// platform layer are missing. Every definition here is WEAK: a real definition (a
// translated file in src/, or the platform layer) replaces it without a link error.
//
// Conventions (shared by all sessions, append below):
// - platform calls get a faithful or test-controlled implementation, and record what a
//   test may want to compare (call counts, last arguments) in tpw_* variables;
// - globals owned by files that are not translated yet get a zero-initialised weak
//   definition; tests write the fields they use before each case.
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "Wavelet.h"
#include "tb_particle.h"
#include "MapWho.h"
#include "platform/indirectx.h"
#include "Game.h"
#include "InGame.h"
#include "Mouse.h"
#include "ui_ErrorDisplay.h"
#include "platform/interfacelib.h"
#include "platform/macdoze.h"
#include "platform/msl.h"
#include "platform/samsutils.h"
#include "tb_ddraw.h"
#include "tb_printf_opengl.h"
#include "tb_resource_display.h"
#include "tb_resource_system.h"
#include "tb_starfield.h"
#include "tb_drawprimitive.h"
#include "tb_loaders.h"
#include "platform/opengl.h"
#include "Clock.h"
#include "CMessageCentre.h"
#include "LbFile.h"
#include "MsgCheatMode.h"
#include "clock_test_support.h"
#include "balance_test_support.h"
#include "HoardingsBalance.h"
#include "MainBalance.h"
#include "OnlineBalance.h"
#include "OptionsBalance.h"
#include "SoundBalance.h"
#include "ResolutionBalance.h"
#include "TbDynamicStringTemplate.h"
#include "LbString.h"
#include "LbTime.h"
#include "platform/fpu.h"
#include "batch3_test_support.h"
#include "Colours.h"
#include "InterfaceFontWrapper.h"
#include "LbPalette.h"
#include "Serialise.h"
#include "TbFontRenderParams.h"
#include "TbTextResourceManager.h"
#include "WorldSavvy.h"
#include "DrawRenderClasses.h"

#define TPW_WEAK __attribute__((weak))

// Draw.cpp's render classes call the still-untranslated colour formatter.
// Unit w239 supplies a controlled strong fake; the aggregate shim only needs
// a weak boundary value until the formatter's own unit lands.
TPW_WEAK uint32_t bfl::draw::CColour::GetFormattedValue(const TbPixFormat&) const { return 0; }

// Other units use DirectDraw and draw-primitive stand-ins. Keep their construction
// outside the display unit so these fakes do not pull its real platform calls in.
TPW_WEAK CDirectDraw::CDirectDraw() {}
TPW_WEAK CDirectDraw::~CDirectDraw() {}
TPW_WEAK CDrawPrimitive::CDrawPrimitive() {}

// ---- Batch 3: independent, opt-in fakes. Existing suites leave tpw_batch3_on zero.
// Virtual placeholder destructors below emit vtables, so remove them with the real owners.
extern "C" {
int32_t tpw_batch3_on = 0, tpw_batch3_n = 0;
Batch3Event tpw_batch3_log[128];
uint32_t tpw_batch3_mask = 0, tpw_batch3_answer = 0;
uint32_t tpw_batch3_clock = 0, tpw_batch3_count = 0, tpw_batch3_result = 0;
uint8_t tpw_batch3_data[4];
int32_t tpw_batch3_read_size = 4;
const void* tpw_batch3_colour_context = nullptr;
extern int32_t tpw_alloc_n;
}
namespace {
Batch3Event* batch3_note(int32_t kind, const void* obj = nullptr, const void* arg = nullptr) {
    if (!tpw_batch3_on || tpw_batch3_n >= 128) return nullptr;
    auto* ev = &tpw_batch3_log[tpw_batch3_n++];
    std::memset(ev, 0, sizeof *ev);
    ev->kind = kind; ev->allocBefore = tpw_alloc_n; ev->object = obj; ev->argument = arg;
    return ev;
}
void font_words(const InterfaceFontWrapper* f, uint32_t* w) {
    w[0] = (uint32_t(f->mF00) << 16) | f->mF02;
    w[1] = (uint32_t(f->mF04) << 16) | uint16_t(f->mHeight);
    w[2] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(f->mBitmap));
    w[3] = static_cast<uint32_t>(f->mID);
    w[4] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(f->mFont));
}
void font_fake(InterfaceFontWrapper* f, int32_t kind, const void* arg = nullptr) {
    if (auto* ev = batch3_note(kind, f, arg)) font_words(f, ev->words);
    if (!tpw_batch3_on) return;
    uint32_t mask = tpw_batch3_mask;
    f->mF00 ^= mask >> 16; f->mF02 ^= mask;
    f->mF04 ^= mask >> 16; f->mHeight = int16_t(uint16_t(f->mHeight) ^ uint16_t(mask));
    f->mBitmap = reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(f->mBitmap) ^ mask);
    f->mID = static_cast<int32_t>(static_cast<uint32_t>(f->mID) ^ mask);
    f->mFont = reinterpret_cast<TbIFont*>(reinterpret_cast<uintptr_t>(f->mFont) ^ mask);
}
template<size_t N> void batch3_words(int32_t kind, const void* obj, uintptr_t (&words)[N]) {
    if (auto* ev = batch3_note(kind, obj))
        for (size_t i = 0; i < N; ++i) ev->words[i] = static_cast<uint32_t>(words[i]);
    if (tpw_batch3_on) for (auto& word : words) word ^= tpw_batch3_mask;
}
}

// The colour ctor is a boundary fake, not the adjacent out-of-scope translation.
TPW_WEAK TbColour::TbColour() {
    if (auto* ev = batch3_note(1, this)) std::memcpy(ev->bytes, this, 5);
    if (tpw_batch3_on) for (auto* p = reinterpret_cast<uint8_t*>(this); p != reinterpret_cast<uint8_t*>(this) + 5; ++p)
        *p ^= static_cast<uint8_t>(tpw_batch3_mask);
}
// Palette internals are outside the unit. Give the imported data an opaque test block.
struct TbPalette { uint8_t mOpaque[1024]; };
TPW_WEAK TbPalette _lbGlobalPalette;
TPW_WEAK uint8_t LbPalette_FindColour(const TbPalette* palette, uint8_t red, uint8_t green, uint8_t blue) {
    if (auto* ev = batch3_note(2, palette)) {
        ev->args[0] = red; ev->args[1] = green; ev->args[2] = blue;
        if (tpw_batch3_colour_context) {
            auto* c = static_cast<const CColours*>(tpw_batch3_colour_context);
            const CColourRamp* ramps[] = {&c->mRed, &c->mGreen, &c->mBlue, &c->mYellow, &c->mWhite};
            for (int i = 0; i < 5; ++i) std::memcpy(ev->bytes + 50 * i, ramps[i]->mColours, 50);
        }
    }
    return static_cast<uint8_t>(red * 3 + green * 5 + blue * 7 + tpw_batch3_answer);
}
#ifdef TPW_LEGACY_STRING_FAKES
TPW_WEAK uint32_t LbTime_GetClock() {
    batch3_note(3);
    return tpw_batch3_clock;
}
#endif
extern "C" void tpw_batch3_debug(const char* text) {
    if (auto* ev = batch3_note(4)) std::strncpy(ev->text, text, sizeof ev->text - 1);
}
TbError tpw_batch3_read(void* file, void* buf, uint32_t size, uint32_t* count) {
    if (auto* ev = batch3_note(5, file, buf)) ev->args[0] = size;
    std::memcpy(buf, tpw_batch3_data, static_cast<size_t>(tpw_batch3_read_size));
    *count = tpw_batch3_count;
    return TbError(static_cast<int32_t>(tpw_batch3_result));
}
TbError tpw_batch3_write(void* file, const void* buf, uint32_t size, uint32_t* count) {
    if (auto* ev = batch3_note(6, file, buf)) {
        ev->args[0] = size;
        std::memcpy(ev->bytes, buf, size);
    }
    *count = tpw_batch3_count;
    return TbError(static_cast<int32_t>(tpw_batch3_result));
}
TPW_WEAK InterfaceFont::InterfaceFont() {}   // the fake leaves the object as it finds it
TPW_WEAK InterfaceFont::~InterfaceFont() {}
TPW_WEAK InterfaceFontWrapper::InterfaceFontWrapper(TbIFont* font) {
    font_fake(this, 7, font);
}
TPW_WEAK InterfaceFontWrapper::~InterfaceFontWrapper() { font_fake(this, 8); }
TPW_WEAK TbFontRenderParams::~TbFontRenderParams() { batch3_words(9, this, mState); }
TPW_WEAK TbFontRenderParams::TbFontRenderParams() {}
TPW_WEAK TbTextResourceManager::TbTextResourceManager() {}
TPW_WEAK TbTextResourceManager::~TbTextResourceManager() {
    uintptr_t words[3] = {State(0), State(1), State(2)};
    batch3_words(10, this, words);
    for (int i = 0; i < 3; ++i) SetState(i, words[i]);
}
TPW_WEAK CWorld* gWorld;
extern "C" CWorld* tpw_batch3_world = nullptr;
TPW_WEAK CWorld& CWorldSavvy::GetWorld() const {
    if (auto* ev = batch3_note(11, this)) std::memcpy(ev->words, this, 4);
    // Deliberate timestamp mutation verifies it is re-read after the call.
    if (tpw_batch3_on) {
        uint32_t word; std::memcpy(&word, this, 4); word ^= tpw_batch3_mask;
        std::memcpy(const_cast<CWorldSavvy*>(this), &word, 4);
    }
    return *tpw_batch3_world;
}
template<> TPW_WEAK int32_t LOADVAR<uint32_t>(void* file, uint32_t* value, const char* name, int32_t flags) {
    if (auto* ev = batch3_note(12, file, value)) {
        ev->words[0] = *value; ev->args[0] = flags;
        std::strncpy(ev->text, name, sizeof ev->text - 1);
    }
    *value ^= tpw_batch3_mask;
    return static_cast<int32_t>(tpw_batch3_answer);
}
template<> TPW_WEAK int32_t SAVEVAR<uint32_t>(void* file, uint32_t* value, const char* name, int32_t flags) {
    if (auto* ev = batch3_note(13, file, value)) {
        ev->words[0] = *value; ev->args[0] = flags;
        std::strncpy(ev->text, name, sizeof ev->text - 1);
    }
    *value ^= tpw_batch3_mask;
    return static_cast<int32_t>(tpw_batch3_answer);
}
// CTimeSig::Serialise (TimeSig.h) calls the unsigned long instances (0x1000f5dc / 0x1000f02c): the same stand-ins.
template<> TPW_WEAK int32_t LOADVAR<MacULong>(void* file, MacULong* value, const char* name, int32_t flags) {
    return LOADVAR<uint32_t>(file, reinterpret_cast<uint32_t*>(value), name, flags);
}
template<> TPW_WEAK int32_t SAVEVAR<MacULong>(void* file, MacULong* value, const char* name, int32_t flags) {
    return SAVEVAR<uint32_t>(file, reinterpret_cast<uint32_t*>(value), name, flags);
}

// ---- balance batch: test fakes, not translations of CBalanceTable/string code.
extern "C" {
int32_t tpw_balance_log_on = 0;
int32_t tpw_balance_log_n = 0;
BalanceTestEvent tpw_balance_log[64];
uint32_t tpw_balance_mask = 0;
extern int32_t tpw_alloc_n;
}
namespace {
BalanceTestEvent* balance_note(int32_t kind, const void* object) {
    if (!tpw_balance_log_on || tpw_balance_log_n >= 64) return nullptr;
    auto* event = &tpw_balance_log[tpw_balance_log_n++];
    std::memset(event, 0, sizeof *event);
    event->mKind = kind;
    event->mObject = object;
    event->mAllocBefore = tpw_alloc_n;
    return event;
}
void balance_fake(CBalanceTable* object, int32_t kind) {
    BalanceValue* values = nullptr;
    int32_t count = 0, type = -1;
    if (auto* p = dynamic_cast<CHoardingsBalance*>(object)) { values = p->mValues; count = 130; type = 0; }
    if (auto* p = dynamic_cast<CMainBalance*>(object)) { values = p->mValues; count = 504; type = 1; }
    if (auto* p = dynamic_cast<COnlineBalance*>(object)) { values = p->mValues; count = 14; type = 2; }
    if (auto* p = dynamic_cast<COptionsBalance*>(object)) { values = p->mValues; count = 28; type = 3; }
    if (auto* p = dynamic_cast<CSoundBalance*>(object)) { values = p->mValues; count = 30; type = 4; }
    if (auto* p = dynamic_cast<CResolutionBalance*>(object)) { values = p->mValues; count = 2; type = 5; }
    if (auto* event = balance_note(kind, object)) {
        event->mType = type;
        event->mRows = reinterpret_cast<uintptr_t>(object->mRows);
    }
    // Deliberately mutate all supplied storage so a lost write or wrong this is visible.
    object->mRows = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(object->mRows) ^ tpw_balance_mask);
    for (int32_t i = 0; i < count; ++i) values[i].mBits ^= tpw_balance_mask + static_cast<uint32_t>(i);
}
}
TPW_WEAK void CBalanceTable::Init() { balance_fake(this, 1); }
TPW_WEAK void CBalanceTable::UnInit() { balance_fake(this, 2); }

#ifdef TPW_LEGACY_STRING_FAKES
#include "legacy_balance_strings.inc"
#endif


// ---- globals of untranslated engine files
TPW_WEAK CDirectDraw* gei_DDraw;  // tb_ddraw.cpp
TPW_WEAK CMapWho* gei_MapWho;     // file not known (MapWho.h)
TPW_WEAK int32_t gDDResult;       // owner not known yet

// ---- InterfaceLib
extern "C" {
int32_t tpw_debugstr_count = 0;
unsigned char tpw_debugstr_last[256];
unsigned char tpw_debugstr_log[16][256];   // the first 16 messages since the count was 0
}

TPW_WEAK void DebugStr(const unsigned char* message) {
    if (tpw_debugstr_count >= 0 && tpw_debugstr_count < 16)
        std::memcpy(tpw_debugstr_log[tpw_debugstr_count], message, static_cast<size_t>(message[0]) + 1);
    tpw_debugstr_count++;
    std::memcpy(tpw_debugstr_last, message, static_cast<size_t>(message[0]) + 1);
}

// ---- Sam's Utilities
extern "C" {
// GetAbsoluteCycles returns these in order (then repeats the last); tests fill them.
uint64_t tpw_cycles[16];
int32_t tpw_cycles_n = 0;
int32_t tpw_cycles_i = 0;
}

namespace SamsUtilities {

// The platform layer's job: turn little-endian file words into host words. A no-op on a
// little-endian host (the test compares file bytes in, values out); a swap on a
// big-endian one, as on the Mac.
TPW_WEAK void _USwapBlock32(uint32_t* words, int32_t count) {
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    for (int32_t i = 0; i < count; i++) {
        uint32_t w = words[i];
        words[i] = (w >> 24) | ((w >> 8) & 0xff00u) | ((w << 8) & 0xff0000u) | (w << 24);
    }
#else
    (void)words;
    (void)count;
#endif
}

TPW_WEAK uint64_t UTimer::GetAbsoluteCycles() {
    if (tpw_cycles_n == 0) return 0;
    int32_t i = tpw_cycles_i < tpw_cycles_n ? tpw_cycles_i : tpw_cycles_n - 1;
    tpw_cycles_i++;
    return tpw_cycles[i];
}

}  // namespace SamsUtilities

// ---- InterfaceLib: Gestalt answers from the test; BlockMoveData is memmove.
extern "C" {
int16_t tpw_gestalt_err = 0;
int32_t tpw_gestalt_value = 0;
int32_t tpw_gestalt_calls = 0;
uint32_t tpw_gestalt_selector = 0;
}

TPW_WEAK int16_t Gestalt(uint32_t selector, int32_t* response) {
    tpw_gestalt_calls++;
    tpw_gestalt_selector = selector;
    if (tpw_gestalt_err == 0) *response = tpw_gestalt_value;
    return tpw_gestalt_err;
}

TPW_WEAK void BlockMoveData(const void* src, void* dest, int32_t count) {
    std::memmove(dest, src, static_cast<size_t>(count));
}

// ---- indirectx: one test surface. Lock hands out tpw_lock_surface and tpw_lock_pitch
// unless tpw_lock_result is nonzero; Unlock returns tpw_unlock_result. Both record their
// arguments (the descriptor's dwSize too) for the test to compare.
extern "C" {
int32_t tpw_lock_result = 0;
int32_t tpw_unlock_result = 0;
void* tpw_lock_surface = nullptr;
int32_t tpw_lock_pitch = 0;
int32_t tpw_lock_calls = 0;
int32_t tpw_unlock_calls = 0;
const void* tpw_lock_this = nullptr;
const void* tpw_lock_rect = nullptr;
const void* tpw_lock_event = nullptr;
uint32_t tpw_lock_flags = 0;
uint32_t tpw_lock_dwsize = 0;
const void* tpw_unlock_this = nullptr;
const void* tpw_unlock_rect = nullptr;
uint32_t tpw_ddsurfacedesc2_size() { return sizeof(DDSURFACEDESC2); }
}

TPW_WEAK int32_t IDirectDrawSurface4::Lock(SamsUtilities::URectangle::LongRect* rect, DDSURFACEDESC2* desc,
                                           uint32_t flags, NS_MacDoze::InternalHandle* event) {
    tpw_lock_calls++;
    tpw_lock_this = this;
    tpw_lock_rect = rect;
    tpw_lock_event = event;
    tpw_lock_flags = flags;
    tpw_lock_dwsize = desc->dwSize;
    if (tpw_lock_result == 0) {
        desc->lpSurface = tpw_lock_surface;
        desc->lPitch = tpw_lock_pitch;
    }
    return tpw_lock_result;
}

TPW_WEAK int32_t IDirectDrawSurface::Unlock(void* rect) {
    tpw_unlock_calls++;
    tpw_unlock_this = this;
    tpw_unlock_rect = rect;
    return tpw_unlock_result;
}

// ---- Untranslated engine functions: a call log. Each call appends (id, this, a..f);
// ids: 1 OpenGL_Init, 2 OpenGL_Close, 10 CMapWho::Render, 11 Link(link, list),
// 12 Unlink(link, list), 13 Reset(list), 14 Link(a, b, c, d, link, list),
// 15 Unlink(a, b, c, d, link), 16 Reset(a, b, c, d), 17 ResetAll.
extern "C" {
int32_t tpw_calls_n = 0;
uint64_t tpw_calls[64][8];
int32_t tpw_opengl_init_result = 0;
int32_t tpw_opengl_close_result = 0;
void tpw_calls_reset() { tpw_calls_n = 0; }
}

static void tpw_call(uint64_t id, const void* self, uint64_t a = 0, uint64_t b = 0, uint64_t c = 0, uint64_t d = 0,
                     uint64_t e = 0, uint64_t f = 0) {
    if (tpw_calls_n >= 64) return;
    uint64_t* r = tpw_calls[tpw_calls_n++];
    r[0] = id;
    r[1] = reinterpret_cast<uintptr_t>(self);
    r[2] = a;
    r[3] = b;
    r[4] = c;
    r[5] = d;
    r[6] = e;
    r[7] = f;
}

static uint64_t tpw_word(int32_t v) { return static_cast<uint32_t>(v); }
static uint64_t tpw_ptr(const void* p) { return reinterpret_cast<uintptr_t>(p); }

TPW_WEAK int32_t OpenGL_Init(int32_t mode) {
    tpw_call(1, nullptr, tpw_word(mode));
    return tpw_opengl_init_result;
}
TPW_WEAK int32_t OpenGL_Close() {
    tpw_call(2, nullptr);
    return tpw_opengl_close_result;
}
TPW_WEAK void CMapWho::Render() { tpw_call(10, this); }
extern "C" {
int32_t tpw_mapwho_result = 1;   // what Link / Unlink (link, list) return
}
TPW_WEAK int32_t CMapWho::Link(CMapLinkBase* link, int32_t list) {
    tpw_call(11, this, tpw_ptr(link), tpw_word(list));
    return tpw_mapwho_result;
}
TPW_WEAK int32_t CMapWho::Unlink(CMapLinkBase* link, int32_t list) {
    tpw_call(12, this, tpw_ptr(link), tpw_word(list));
    return tpw_mapwho_result;
}
TPW_WEAK int32_t CMapWho::Reset(int32_t list) { tpw_call(13, this, tpw_word(list)); return tpw_mapwho_result; }
TPW_WEAK int32_t CMapWho::Link(int32_t a, int32_t b, int32_t c, int32_t d, CMapLinkBase* link, int32_t list) {
    tpw_call(14, this, tpw_word(a), tpw_word(b), tpw_word(c), tpw_word(d), tpw_ptr(link), tpw_word(list));
    return tpw_mapwho_result;
}
TPW_WEAK int32_t CMapWho::Unlink(int32_t a, int32_t b, int32_t c, int32_t d, CMapLinkBase* link) {
    tpw_call(15, this, tpw_word(a), tpw_word(b), tpw_word(c), tpw_word(d), tpw_ptr(link));
    return tpw_mapwho_result;
}
TPW_WEAK int32_t CMapWho::Reset(int32_t a, int32_t b, int32_t c, int32_t d) {
    tpw_call(16, this, tpw_word(a), tpw_word(b), tpw_word(c), tpw_word(d));
    return tpw_mapwho_result;
}
TPW_WEAK int32_t CMapWho::ResetAll() { tpw_call(17, this); return tpw_mapwho_result; }

// ---- InterfaceLib FrontWindow and macdoze GlobalMemoryStatus: test-controlled answers.
extern "C" {
void* tpw_front_window = nullptr;
int32_t tpw_front_window_calls = 0;
uint32_t tpw_memory_status[8];   // what GlobalMemoryStatus fills in
int32_t tpw_memory_status_calls = 0;
}

TPW_WEAK GrafPort* FrontWindow() {
    tpw_front_window_calls++;
    return static_cast<GrafPort*>(tpw_front_window);
}

namespace NS_MacDoze {
TPW_WEAK void GlobalMemoryStatus(_MEMORYSTATUS* status) {
    tpw_memory_status_calls++;
    std::memcpy(status, tpw_memory_status, sizeof *status);
}
}  // namespace NS_MacDoze

// ---- tb_starfield.cpp sprite classes (call log ids 20..42). The constructors leave the
// objects alone; SSpriteEngineData::Init answers from tpw_sprite_init (in call order) and
// logs the names; CSpriteList::Init hands out arrays from a static pool, or NULL when
// tpw_spritelist_init_fail is set. Classes with virtual functions here (CSpriteList,
// CMapLinkBase) must lose their placeholders when the owning file is translated (the
// vtables would be emitted twice).
extern "C" {
int32_t tpw_sprite_init[16];
int32_t tpw_sprite_init_n = 0;
int32_t tpw_sprite_init_i = 0;
char tpw_sprite_names[16][64];
int32_t tpw_spritelist_init_fail = 0;
void* tpw_spritelist_pool[4][64];
int32_t tpw_spritelist_pool_i = 0;
void tpw_sprites_reset() {
    tpw_sprite_init_i = 0;
    tpw_spritelist_pool_i = 0;
    std::memset(tpw_spritelist_pool, 0, sizeof tpw_spritelist_pool);
}
}

TPW_WEAK SSpriteEngineData::SSpriteEngineData() { tpw_call(20, this); }
TPW_WEAK SSpriteEngineData::~SSpriteEngineData() { tpw_call(21, this); }
TPW_WEAK int32_t SSpriteEngineData::Init(char* name) {
    int32_t i = tpw_sprite_init_i++;
    if (i < 16) std::strncpy(tpw_sprite_names[i], name, 63);
    tpw_call(22, this, static_cast<uint64_t>(i));
    if (tpw_sprite_init_n == 0) return 1;
    return tpw_sprite_init[i < tpw_sprite_init_n ? i : tpw_sprite_init_n - 1];
}
TPW_WEAK int32_t SSpriteEngineData::Close() {
    tpw_call(23, this);
    return 1;
}
TPW_WEAK SSpriteGameData::SSpriteGameData() { tpw_call(30, this); }
// Also logs the CMapLinkBase subobject, so tests can name it after the list is gone.
TPW_WEAK CSpriteList::CSpriteList() { tpw_call(40, this, tpw_ptr(static_cast<CMapLinkBase*>(this))); }
TPW_WEAK CSpriteList::~CSpriteList() { tpw_call(41, this); }
// The link interface's slots: only the vtable needs them here.
TPW_WEAK int32_t CSpriteList::Prepare() { return 0; }
TPW_WEAK int32_t CSpriteList::GetLinkType() { return 0; }
TPW_WEAK void* CSpriteList::GetData() { return nullptr; }
TPW_WEAK SSpriteGameData** CSpriteList::Init(int32_t count) {
    tpw_call(42, this, tpw_word(count));
    if (tpw_spritelist_init_fail != 0 || tpw_spritelist_pool_i >= 4) return nullptr;
    return reinterpret_cast<SSpriteGameData**>(tpw_spritelist_pool[tpw_spritelist_pool_i++]);
}

// ---- MSL __assertion_failed: recorded (condition, file, line), then returns, so the
// translation's code after a failed assert runs as the original's would have if MSL had
// not aborted. tpwtest.host_asserts() reads the log.
extern "C" {
int32_t tpw_asserts_n = 0;
char tpw_assert_cond[16][32];
char tpw_assert_file[16][64];
int32_t tpw_assert_line[16];
void tpw_asserts_reset() { tpw_asserts_n = 0; }
}

// MSL's generator (platform/msl.cpp in the native build): the same arithmetic.
static uint32_t gShimRandState = 1;
TPW_WEAK int32_t msl_rand() {
    gShimRandState = gShimRandState * 0x41c64e6du + 0x3039u;
    return static_cast<int32_t>((gShimRandState >> 16) & 0x7fff);
}
TPW_WEAK void msl_srand(uint32_t seed) { gShimRandState = seed; }
TPW_WEAK void msl_assertion_failed(const char* condition, const char* file, int32_t line) {
    if (tpw_asserts_n >= 16) return;
    std::strncpy(tpw_assert_cond[tpw_asserts_n], condition, 31);
    std::strncpy(tpw_assert_file[tpw_asserts_n], file, 63);
    tpw_assert_line[tpw_asserts_n] = line;
    tpw_asserts_n++;
}

// ---- Clock.cpp/Cheat.cpp: controlled untranslated main-app callees.
// Tests stub the same callees on PowerPC. Masks deliberately mutate whole subobjects
// so forwarding the wrong object, losing writes or reordering calls is observable.

extern "C" {
extern int32_t tpw_alloc_n;
int32_t tpw_clock_log_on = 0;
int32_t tpw_clock_log_n = 0;
ClockTestEvent tpw_clock_log[256];
int32_t tpw_clock_returns[32];
uint32_t tpw_clock_masks[32];
const void* tpw_clock_context = nullptr;
uint32_t tpw_cheat_msg_flags = 0;
uint32_t tpw_cheat_subscription_words[4];
uint32_t tpw_cheat_post_mask = 0;
int32_t tpw_cheat_io_i = 0;
uint32_t tpw_cheat_io_count[2];
uint32_t tpw_cheat_io_result[2];
uint8_t tpw_cheat_io_data[2];
uint8_t tpw_cheat_io_write[2];
}

namespace {
ClockTestEvent* clock_note(int32_t kind, const void* object, const void* argument = nullptr,
                           uint32_t value = 0, uint32_t count = 0) {
    if (tpw_clock_log_on == 0 || tpw_clock_log_n == 256) return nullptr;
    ClockTestEvent& e = tpw_clock_log[tpw_clock_log_n++];
    std::memset(&e, 0, sizeof e);
    e.kind = kind;
    e.allocBefore = tpw_alloc_n;
    e.object = object;
    e.argument = argument;
    e.value = value;
    e.count = count;
    if (tpw_clock_context != nullptr) std::memcpy(e.context, tpw_clock_context, sizeof e.context);
    return &e;
}

int32_t clock_fake(int32_t kind, void* object, uint32_t words, void* file = nullptr) {
    ClockTestEvent* e = clock_note(kind, object, file, 0, words);
    if (e != nullptr) std::memcpy(e->words, object, words * 4);
    for (uint32_t i = 0; i < words; i++) {
        uint32_t v;
        auto* p = static_cast<unsigned char*>(object) + i * 4;
        std::memcpy(&v, p, 4);
        v ^= tpw_clock_masks[kind];
        std::memcpy(p, &v, 4);
    }
    return tpw_clock_returns[kind];
}
}  // namespace

TPW_WEAK CMessageCentre* gMessageCentre;

TPW_WEAK CVariableTime::CVariableTime() { clock_fake(kClockVariableCtor, this, 8); }
TPW_WEAK CFixedTime::CFixedTime() { clock_fake(kClockFixedCtor, this, 3); }
TPW_WEAK int32_t CSaveableTime::ReadSaveableTime() { return clock_fake(kClockRead, this, 17); }
TPW_WEAK int32_t CSaveableFixedTime::ReadSaveableFixedTime() { return clock_fake(kClockReadFixed, this, 8); }
TPW_WEAK void CPausableTime::Pause() { clock_fake(kClockPause, this, 11); }
TPW_WEAK void CPausableFixedTime::Pause() { clock_fake(kClockPauseFixed, this, 6); }
TPW_WEAK void CPausableTime::UnPause() { clock_fake(kClockUnPause, this, 11); }
TPW_WEAK void CPausableFixedTime::UnPause() { clock_fake(kClockUnPauseFixed, this, 6); }
TPW_WEAK void CPausableTime::TogglePause() { clock_fake(kClockToggle, this, 11); }
TPW_WEAK void CPausableFixedTime::TogglePause() { clock_fake(kClockToggleFixed, this, 6); }
TPW_WEAK int32_t CPausableTime::GetPauseFlag() { return clock_fake(kClockPauseFlag, this, 11); }
TPW_WEAK int32_t CSaveableTime::Save(void* file) { return clock_fake(kClockSave, this, 17, file); }
TPW_WEAK int32_t CSaveableFixedTime::Save(void* file) { return clock_fake(kClockSaveFixed, this, 8, file); }
TPW_WEAK int32_t CSaveableTime::Load(void* file) { return clock_fake(kClockLoad, this, 17, file); }
TPW_WEAK int32_t CSaveableFixedTime::Load(void* file) { return clock_fake(kClockLoadFixed, this, 8, file); }
TPW_WEAK void CSaveableTime::AdjustTime() { clock_fake(kClockAdjust, this, 17); }
TPW_WEAK void CSaveableFixedTime::AdjustTime() { clock_fake(kClockAdjustFixed, this, 8); }

TPW_WEAK CMsgCheatMode::CMsgCheatMode(bool cheating) {
    clock_note(kCheatMsgCtor, this, nullptr, cheating);
    mFlags = tpw_cheat_msg_flags;
    mCheating = cheating;
}

TPW_WEAK CSubscriptionHeader::CSubscriptionHeader() {
    clock_note(kCheatSubscriptionCtor, this);
    std::memcpy(mState, tpw_cheat_subscription_words, sizeof mState);
}
TPW_WEAK CSubscriptionHeader::~CSubscriptionHeader() {}

TPW_WEAK int32_t CMessageCentre::PostMessage(const CMsg& message) {
    auto& cheat = static_cast<const CMsgCheatMode&>(message);
    ClockTestEvent* e = clock_note(kCheatPost, this, &message, cheat.mCheating, 2);
    if (e != nullptr) {
        e->words[0] = message.mFlags;
        e->words[1] = static_cast<uint32_t>(message.GetType());
    }
    for (auto& header : mSubscriptions) {
        for (uint32_t i = 0; i < 4; i++) {
            uint32_t v;
            std::memcpy(&v, header.mState + i * 4, 4);
            v ^= tpw_cheat_post_mask;
            std::memcpy(header.mState + i * 4, &v, 4);
        }
    }
    return tpw_clock_returns[kCheatPost];
}

namespace SamsUtilities {
// Other suites observe UDebug through this hook (shim_TPMain.cpp sets it).
extern "C" void (*tpw_udebug_hook)(const char* format);
void (*tpw_udebug_hook)(const char* format) = nullptr;
TPW_WEAK void UDebug(const char* format, ...) {
    if (tpw_udebug_hook != nullptr) tpw_udebug_hook(format);
    if (tpw_batch3_on) tpw_batch3_debug(format);
    ClockTestEvent* e = clock_note(kCheatDebug, nullptr);
    if (e != nullptr) std::strncpy(e->text, format, sizeof e->text - 1);
}
}  // namespace SamsUtilities

#ifdef TPW_LEGACY_STRING_FAKES
TPW_WEAK TbError LbFile_Read(void* file, void* buffer, uint32_t length, uint32_t* transferred) {
    if (tpw_batch3_on) return tpw_batch3_read(file, buffer, length, transferred);
    int32_t i = tpw_cheat_io_i++;
    clock_note(kCheatRead, file, buffer, 0, length);
    if (tpw_cheat_io_write[i] != 0 && length != 0)
        *static_cast<uint8_t*>(buffer) = tpw_cheat_io_data[i];
    *transferred = tpw_cheat_io_count[i];
    return TbError(static_cast<int32_t>(tpw_cheat_io_result[i]));
}

TPW_WEAK TbError LbFile_Write(void* file, const void* buffer, uint32_t length, uint32_t* transferred) {
    if (tpw_batch3_on) return tpw_batch3_write(file, buffer, length, transferred);
    int32_t i = tpw_cheat_io_i++;
    clock_note(kCheatWrite, file, buffer, length == 0 ? 0 : *static_cast<const uint8_t*>(buffer), length);
    *transferred = tpw_cheat_io_count[i];
    return TbError(static_cast<int32_t>(tpw_cheat_io_result[i]));
}
#endif

// ---- OpenGL 1.1, SystemTask, UTimer::GetAbsolute, TGALoadAndConvert, gei_DrawPrimitive
// (tb_splash.cpp's callees). GL calls append (id, ints[4], float bits[3]) to tpw_gl_log:
// ids 1 PushAttrib, 2 PopAttrib, 3 Disable, 4 PixelStorei, 5 RasterPos3f, 6 PixelZoom,
// 7 DrawPixels (the pixel pointer goes to tpw_gl_ptr), 8 GetError. GetAbsolute returns tpw_time_base + n * tpw_time_step (n counts calls).
// TGALoadAndConvert answers call n from tpw_images[n] (width, height, ok, data offset in
// tpw_image_bytes); the pixels come from malloc and are logged in tpw_image_ptr.
extern "C" {
int32_t tpw_gl_n = 0;
uint32_t tpw_gl_log[128][8];
int32_t tpw_systemtask_calls = 0;
uint64_t tpw_time_base = 0;
uint64_t tpw_time_step = 1000;   // never 0 by default: busy-wait loops must end
int32_t tpw_time_calls = 0;
int32_t tpw_images[16][4];
uint8_t tpw_image_bytes[1 << 20];
int32_t tpw_image_n = 0;
void* tpw_image_ptr[16];
char tpw_image_name[16][128];
int32_t tpw_image_bpp[16];
int32_t tpw_image_count = 0;   // images configured
void tpw_splash_reset() {
    tpw_gl_n = 0;
    tpw_systemtask_calls = 0;
    tpw_time_calls = 0;
    tpw_image_n = 0;
}
int32_t tpw_image_index(const void* p) {
    for (int32_t i = 0; i < tpw_image_n && i < 16; i++)
        if (tpw_image_ptr[i] == p) return i;
    return -1;
}
}

static void tpw_gl(uint32_t id, uint32_t a = 0, uint32_t b = 0, uint32_t c = 0, uint32_t d = 0, float x = 0,
                   float y = 0, float z = 0) {
    if (tpw_gl_n >= 128) return;
    uint32_t* r = tpw_gl_log[tpw_gl_n++];
    r[0] = id;
    r[1] = a;
    r[2] = b;
    r[3] = c;
    r[4] = d;
    std::memcpy(&r[5], &x, 4);
    std::memcpy(&r[6], &y, 4);
    std::memcpy(&r[7], &z, 4);
}

TPW_WEAK void glPushAttrib(GLbitfield mask) { tpw_gl(1, mask); }
TPW_WEAK void glPopAttrib() { tpw_gl(2); }
TPW_WEAK void glDisable(GLenum cap) { tpw_gl(3, cap); }
TPW_WEAK void glPixelStorei(GLenum pname, GLint param) { tpw_gl(4, pname, static_cast<uint32_t>(param)); }
TPW_WEAK void glRasterPos3f(GLfloat x, GLfloat y, GLfloat z) { tpw_gl(5, 0, 0, 0, 0, x, y, z); }
TPW_WEAK void glPixelZoom(GLfloat x, GLfloat y) { tpw_gl(6, 0, 0, 0, 0, x, y); }
extern "C" {
uint64_t tpw_gl_ptr[128];   // DrawPixels: the pixel pointer of that log row
}
TPW_WEAK void glDrawPixels(GLsizei w, GLsizei h, GLenum format, GLenum type, const void* pixels) {
    tpw_gl(7, static_cast<uint32_t>(w), static_cast<uint32_t>(h), format, type);
    if (tpw_gl_n <= 128) tpw_gl_ptr[tpw_gl_n - 1] = reinterpret_cast<uintptr_t>(pixels);
}
TPW_WEAK GLenum glGetError() {
    tpw_gl(8);
    return 0;
}
// The widescreen splash's black sides (the v5L26 port fix in the splash).
TPW_WEAK void glClearColor(GLfloat r, GLfloat g, GLfloat b, GLfloat a) { tpw_gl(9, 0, 0, 0, 0, r, g, b + a); }
TPW_WEAK void glClear(GLbitfield mask) { tpw_gl(10, mask); }

TPW_WEAK void SystemTask() { tpw_systemtask_calls++; }

namespace SamsUtilities {
TPW_WEAK uint64_t UTimer::GetAbsolute() {
    return tpw_time_base + static_cast<uint64_t>(tpw_time_calls++) * tpw_time_step;
}
}  // namespace SamsUtilities

TPW_WEAK void* TGALoadAndConvert(char* name, int32_t* width, int32_t* height, int32_t flags, lARGBType* palette,
                                 int32_t bytesPerPixel) {
    int32_t i = tpw_image_n++;
    if (i >= 16) return nullptr;
    std::strncpy(tpw_image_name[i], name, 127);
    tpw_image_bpp[i] = bytesPerPixel;
    tpw_image_ptr[i] = nullptr;
    if (i >= tpw_image_count || tpw_images[i][2] == 0) return nullptr;
    int32_t w = tpw_images[i][0], h = tpw_images[i][1];
    size_t n = static_cast<size_t>(w) * static_cast<size_t>(h) * static_cast<size_t>(bytesPerPixel);
    void* p = std::malloc(n != 0 ? n : 1);
    std::memcpy(p, tpw_image_bytes + tpw_images[i][3], n);
    *width = w;
    *height = h;
    tpw_image_ptr[i] = p;
    return p;
}

TPW_WEAK CDrawPrimitive* gei_DrawPrimitive;   // tb_drawprimitive.cpp
TPW_WEAK CDrawPrimitive::~CDrawPrimitive() {}  // (remove with the real file: vtable)

// ---- bullfrog callees of the TbPixFmt.cpp range (LbLog.cpp, TbFPUPrecision.cpp): LbFile
// (LbSCBack.cpp range), LbString_strnzcpy (WinScrn.cpp range), LbTime (Draw.cpp range),
// PlatformFPUFlags (the port's hook for the FPU word). Calls go to tpw_bf_log as (id,
// text, number): 1 ToProjectFilename (name, size), 2 IsDirectory (name), 3 Delete (name),
// 4 GetTime, 5 GetDate. ToProjectFilename writes tpw_bf_project (cut to size - 1 bytes)
// and returns tpw_bf_project_result; strnzcpy does what the original does (0x10066200).
extern "C" {
struct TpwBfCall { int32_t id; char text[0x120]; int32_t number; };
TpwBfCall tpw_bf_log[16];
int32_t tpw_bf_log_n = 0;
char tpw_bf_project[0x120];
int32_t tpw_bf_project_result = 0;
int32_t tpw_bf_isdir = 0;
int32_t tpw_bf_delete_result = 0;
int32_t tpw_bf_time[4];
int32_t tpw_bf_date[4];
uint16_t tpw_fpu_flags = 0;
}
namespace {
void bf_note(int32_t id, const char* text, int32_t number) {
    if (tpw_bf_log_n >= 16) return;
    TpwBfCall& c = tpw_bf_log[tpw_bf_log_n++];
    c.id = id;
    std::memset(c.text, 0, sizeof c.text);
    if (text != nullptr) std::strncpy(c.text, text, sizeof c.text - 1);
    c.number = number;
}
}  // namespace

#ifdef TPW_LEGACY_STRING_FAKES
TPW_WEAK TbError LbFile_ToProjectFilename(char* out, uint32_t size, const char* name) {
    bf_note(1, name, static_cast<int32_t>(size));
    size_t n = std::strlen(tpw_bf_project);
    if (size != 0) {
        if (n > size - 1) n = size - 1;
        std::memcpy(out, tpw_bf_project, n);
        out[n] = '\0';
    }
    return TbError(tpw_bf_project_result);
}
#endif
#ifdef TPW_LEGACY_STRING_FAKES
TPW_WEAK int32_t LbFile_IsDirectory(const char* name) {
    bf_note(2, name, 0);
    return tpw_bf_isdir;
}
TPW_WEAK TbError LbFile_Delete(const char* name) {
    bf_note(3, name, 0);
    return TbError(tpw_bf_delete_result);
}
#endif
TPW_WEAK int32_t LbString_strnzcpy(char* dst, const char* src, uint32_t size) {
    uint32_t n = static_cast<uint32_t>(std::strlen(src));
    if (n < size - 1) {
        std::memcpy(dst, src, n + 1);
        return 1;
    }
    std::memcpy(dst, src, size - 1);
    dst[size - 1] = '\0';
    return 0;
}
TPW_WEAK void LbTime_GetTime(TbTime* time) {
    bf_note(4, nullptr, 0);
    time->mHour = tpw_bf_time[0];
    time->mMinute = tpw_bf_time[1];
    time->mSecond = tpw_bf_time[2];
    time->mMillisecond = tpw_bf_time[3];
}
TPW_WEAK void LbTime_GetDate(TbDate* date) {
    bf_note(5, nullptr, 0);
    date->mDay = tpw_bf_date[0];
    date->mMonth = tpw_bf_date[1];
    date->mYear = tpw_bf_date[2];
    date->mDayOfWeek = tpw_bf_date[3];
}
TPW_WEAK uint16_t PlatformFPUFlags() {
    return tpw_fpu_flags;
}

// ---- virtual members of translated classes that are themselves still to do (their
// vtables need the symbols; nothing in the tests calls them)
TPW_WEAK int32_t CParticle::Prepare() { return 0; }
TPW_WEAK int32_t CWaveletImp::Decompress(uint32_t, uint32_t, bool, const uint8_t*, uint8_t*, float, float, float) { return 0; }
TPW_WEAK int32_t CWaveletImp::Decompress(int32_t, int32_t, int32_t, int32_t, void*, void*, void*, float, float, float, float) { return 0; }
TPW_WEAK int32_t CWaveletImp::Decompress(int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, void*, void*, void*, float, float, float, float) { return 0; }
TPW_WEAK int32_t CWaveletImp::Decompress_v12(int32_t, int32_t, int32_t, void*, void*, void*, float, float, float, float) { return 0; }
TPW_WEAK int32_t CWaveletImp::Decompress_v13(int32_t, int32_t, int32_t, void*, void*, void*, float, float, float, float) { return 0; }

// ---- unit w62: what the translated CGame::DeInit and CInGame::Init call (untranslated); nothing in these tests calls them
TPW_WEAK CMouse* gMouse;
TPW_WEAK void CMouse::AssignMouseMode(CMouseMode*) {}
TPW_WEAK void CGame::ExitLevel() {}
TPW_WEAK void CInGame::Start() {}
TPW_WEAK void FuckOffUIProgressBar() {}
