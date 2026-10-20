// sams_core.cpp: the parts of Sam's Utilities with no window or file behind them: strings,
// rectangles, memory, timers.
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <new>
#include <thread>

#include "platform.h"

namespace SamsUtilities {

// ---- UString ------------------------------------------------------------------------
// A default-constructed string points at one shared empty buffer; any other owns its text.
static char gEmptyText[1] = {'\0'};

static char* CopyText(const char* text, uint32_t length) {
    char* copy = static_cast<char*>(std::malloc(length + 1));
    if (length != 0) std::memcpy(copy, text, length);
    copy[length] = '\0';
    return copy;
}

UString::UString() : mLength(0), mText(gEmptyText) {}

UString::UString(const char* text) {
    mLength = text != nullptr ? static_cast<uint32_t>(std::strlen(text)) : 0;
    mText = mLength != 0 ? CopyText(text, mLength) : gEmptyText;
}

UString::UString(const char* text, uint32_t length) {
    mLength = text != nullptr ? length : 0;
    mText = mLength != 0 ? CopyText(text, mLength) : gEmptyText;
}

UString::~UString() {
    if (mText != gEmptyText) std::free(mText);
    mText = gEmptyText;
    mLength = 0;
}

const UString UString::kEmptyString;

void UString::c2pcpy(unsigned char* dest, const char* source) {
    size_t length = std::strlen(source);
    if (length > 255) length = 255;
    dest[0] = static_cast<unsigned char>(length);
    std::memcpy(dest + 1, source, length);
}

UResourceId::UResourceId(const UString& type, int32_t id) : mType(type.mText, type.mLength), mId(id) {}

// ---- Rectangles and points ----------------------------------------------------------
URectangle::~URectangle() {}

URectangle::URectangle(int32_t left, int32_t top, int32_t right, int32_t bottom) : mRect{left, top, right, bottom} {
    if (mRect.right < mRect.left) std::swap(mRect.left, mRect.right);
    if (mRect.bottom < mRect.top) std::swap(mRect.top, mRect.bottom);
}

// Copies the QuickDraw rectangle and orders its edges (the original's Rationalize).
URectangleMac::URectangleMac(const Rect& rect) {
    mRect.left = rect.left;
    mRect.top = rect.top;
    mRect.right = rect.right;
    mRect.bottom = rect.bottom;
    if (mRect.right < mRect.left) std::swap(mRect.left, mRect.right);
    if (mRect.bottom < mRect.top) std::swap(mRect.top, mRect.bottom);
}

void URectangleMac::ToGlobal(GrafPort*) {
    int32_t x, y;
    tpw::WindowOrigin(x, y);
    mRect.left += x;
    mRect.right += x;
    mRect.top += y;
    mRect.bottom += y;
}

void URectangleMac::ToLocal(GrafPort*) {
    int32_t x, y;
    tpw::WindowOrigin(x, y);
    mRect.left -= x;
    mRect.right -= x;
    mRect.top -= y;
    mRect.bottom -= y;
}

// (The shim keeps these in a 1 KB ring of scratch memory too.)
URectangleMac::operator Rect*() {
    static Rect ring[128];
    static unsigned next = 0;
    Rect* rect = &ring[next++ % 128];
    rect->left = static_cast<int16_t>(mRect.left);
    rect->right = static_cast<int16_t>(mRect.right);
    rect->top = static_cast<int16_t>(mRect.top);
    rect->bottom = static_cast<int16_t>(mRect.bottom);
    return rect;
}

void UPointMac::ToLocal(GrafPort*) {
    int32_t originX, originY;
    tpw::WindowOrigin(originX, originY);
    x -= originX;
    y -= originY;
}

// ---- Events -------------------------------------------------------------------------
// Every UEvent the game is handed is a tpw::Event made in gamework.cpp.
UPoint UEvent::GetPosition() const {
    const auto* event = static_cast<const tpw::Event*>(this);
    return {event->globalX, event->globalY};
}

uint16_t UEvent::GetKeyModifiers() const { return static_cast<const tpw::Event*>(this)->modifiers; }

// ---- Memory -------------------------------------------------------------------------
UAllocator* gMemory = nullptr;

UAllocator::UAllocator() : mOpaque{} {}

// The temporary allocator (sams utils shared): short-lived buffers from the host's heap. Free also clears the caller's
// pointer, as the Mac's does.
UAllocatorTemporary* gMemoryTemporary = nullptr;

UAllocatorTemporary::UAllocatorTemporary() {}
UAllocatorTemporary::~UAllocatorTemporary() {}
uint8_t* UAllocatorTemporary::Allocate(uint32_t size) { return new (std::nothrow) uint8_t[size]; }
void UAllocatorTemporary::Free(uint8_t*& block) {
    delete[] block;
    block = nullptr;
}

// The platform allocator's byte clear (sams utils 0x100009a0).
void UAllocator::Clear(void* dest, uint32_t size) { std::memset(dest, 0, size); }

// `count` elements of `elementSize` bytes, aligned to `alignment`, zeroed. The original
// (sams utils 0x100113f8) asks its allocator for alignment + count * elementSize bytes and
// rounds the pointer up; only mData is read by the game.
UMemoryAlignedBase::UMemoryAlignedBase(uint32_t count, uint32_t alignment, uint32_t elementSize, UAllocator*) {
    if (alignment < sizeof(void*)) alignment = sizeof(void*);
    const size_t size = static_cast<size_t>(count) * elementSize;
    void* block = nullptr;
    if (posix_memalign(&block, alignment, size != 0 ? size : 1) != 0) block = nullptr;
    if (block != nullptr) std::memset(block, 0, size);
    mF00 = count;
    mData = block;
    mF08 = alignment;
    mF0C = elementSize;
}

UMemoryAlignedBase::~UMemoryAlignedBase() {
    std::free(mData);
    mData = nullptr;
}

// Byte-swaps `count` 32-bit words: the Mac's conversion of little-endian file data. A
// little-endian host already has them the right way round.
void _USwapBlock32(uint32_t* words, int32_t count) {
    const uint16_t probe = 1;
    if (*reinterpret_cast<const uint8_t*>(&probe) == 1) return;
    for (int32_t i = 0; i < count; ++i) {
        const uint32_t v = words[i];
        words[i] = (v >> 24) | ((v >> 8) & 0xff00u) | ((v << 8) & 0xff0000u) | (v << 24);
    }
}

uint32_t* _USwapCopy32(const uint32_t* words, int32_t count) {
    uint32_t* copy = new uint32_t[count > 0 ? count : 1];
    for (int32_t i = 0; i < count; ++i) copy[i] = words[i];
    _USwapBlock32(copy, count);
    return copy;
}

void _USwapDelete(void* copy) { delete[] static_cast<uint32_t*>(copy); }

// A block for _USwapDelete to free (it frees with delete[] of words, so this allocates words).
void* _USwapAllocate(uint32_t bytes) { return new uint32_t[bytes / 4 + 1]; }

void _USwapBlock16(uint16_t* words, int32_t count) {
    const uint16_t probe = 1;
    if (*reinterpret_cast<const uint8_t*>(&probe) == 1) return;
    for (int32_t i = 0; i < count; ++i) words[i] = static_cast<uint16_t>(words[i] >> 8 | words[i] << 8);
}

// ---- Time ---------------------------------------------------------------------------
static std::chrono::steady_clock::time_point StartTime() {
    static const auto start = std::chrono::steady_clock::now();
    return start;
}

// Microseconds since the program started (the Mac's since boot; the game uses differences,
// and one value to seed rand).
uint64_t UTimer::GetAbsolute() {
    const auto start = StartTime();   // first: the first call must not read the clock before the start is taken
    const auto elapsed = std::chrono::steady_clock::now() - start;
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count());
}

// A free-running counter (the PowerPC time base); nanoseconds here.
uint64_t UTimer::GetAbsoluteCycles() {
    const auto start = StartTime();
    const auto elapsed = std::chrono::steady_clock::now() - start;
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count());
}

void UTimer::SetRate(const UMicroseconds& rate, bool start) {
    mRate = rate.mValue;
    mRunning = start;
    mNextTick = static_cast<int64_t>(GetAbsolute()) + mRate;
}

void UThread::Yield() { std::this_thread::yield(); }

// ---- USoundChannel -------------------------------------------------------------------
// The class is declared in include/TbAudioSystem.h (the sound start-up calls SetSafeMode when Option and Command are held);
// the declaration is repeated here exactly as there, because that header and the platform headers are not included together.
// Safe mode made the Sound Sprockets channels play in a degraded mode; the host module has one way of playing, so the flag is
// only kept (tpw::SoundSafeMode).
class USoundChannel {
public:
    static void SetSafeMode(uint8_t on);
};

static bool gSoundSafeMode = false;

void USoundChannel::SetSafeMode(uint8_t on) {
    gSoundSafeMode = on != 0;
    tpw::Log("sound: safe mode %s (no effect on the host)", gSoundSafeMode ? "on" : "off");
}

}  // namespace SamsUtilities

bool tpw::SoundSafeMode() { return SamsUtilities::gSoundSafeMode; }

// ---- Toolbox calls with no state ------------------------------------------------------
void BlockMoveData(const void* src, void* dest, int32_t count) {
    if (count > 0) std::memmove(dest, src, static_cast<size_t>(count));
}

void SetRect(Rect* rect, int16_t left, int16_t top, int16_t right, int16_t bottom) {
    rect->left = left;
    rect->top = top;
    rect->right = right;
    rect->bottom = bottom;
}

unsigned char* c2pstr(char* text) {
    size_t length = std::strlen(text);
    if (length > 255) length = 255;
    std::memmove(text + 1, text, length);
    text[0] = static_cast<char>(length);
    return reinterpret_cast<unsigned char*>(text);
}

// Two Pascal strings compared. The game only compares ASCII names ignoring case; the
// Mac's diacritical folding is not reproduced.
bool EqualString(const unsigned char* a, const unsigned char* b, bool caseSensitive, bool) {
    if (a[0] != b[0]) return false;
    for (unsigned i = 1; i <= a[0]; ++i) {
        unsigned char x = a[i], y = b[i];
        if (!caseSensitive) {
            if (x >= 'a' && x <= 'z') x = static_cast<unsigned char>(x - 0x20);
            if (y >= 'a' && y <= 'z') y = static_cast<unsigned char>(y - 0x20);
        }
        if (x != y) return false;
    }
    return true;
}

// Gestalt: the machine the game sizes its defaults for (TPSettings.cpp: memory in bytes
// under 'ram ', the processor clock in Hz under 'pclk'). Both are 32-bit on the Mac, so a
// modern machine is reported as the largest values that fit: the top detail tier.
int16_t Gestalt(uint32_t selector, int32_t* response) {
    switch (selector) {
    case 0x72616d20:   // 'ram '
        *response = 0x40000000;   // 1 GB
        return 0;
    case 0x70636c6b:   // 'pclk'
        *response = 1000000000;   // 1 GHz
        return 0;
    default:
        tpw::Log("Gestalt: unknown selector 0x%08x", selector);
        return -5551;   // gestaltUndefSelectorErr
    }
}

// The x87 status word the PC build read with assembler (include/platform/fpu.h).
uint16_t PlatformFPUFlags() { return 0; }
