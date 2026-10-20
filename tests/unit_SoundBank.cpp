// Fakes and test entry points for unit w72, the sound library's sample banks (libtpw_unit_SoundBank.dylib, driven by
// tests/test_SoundBank.py in the program "sound symbols shared"; see calllog.h):
//   src/TbSoundBank.cpp    TbBank, TbFileBank, TbMMFileBank, TbSoundBankInfo, CBankFactory, Bank::CBank, the swap helpers,
//                          CFileOperation::GetBankPath, TbSoundSampleInfo::SetBank, the double to unsigned long conversion
// Real: those, TbRefRelease and the bullfrog strings. Fakes, each recording what the original's callee would be given (the
// PowerPC side records the same from its stubs):
//   the file (bfl::FileSystem::CFile: copy constructor, destructor, Seek, Read, Close, MapFile) over the bytes of the case,
//   the nothrow forms of operator new and new[] (the library's allocator; the queue "Alloc" says which allocation fails,
//   the blocks are zeroed and labelled, so that their deletion shows) and the owners (a TbRefRelease that records
//   AddRef and Release), the platform's _USwapBlock32, SamsUtilities::UString (construction and destruction).
// The file's behaviour: Seek answers the new position (the queue "Seek": -1 makes it fail), Read copies what is left (the
// queue "Read": a value caps the count, -1 fails), MapFile answers the whole image (the queue "Map": 0 answers NULL).
//
// Labels: 1 the component (and the owner of the same address), 2 a second owner, 3 and 5 the two file objects the banks copy,
// 4 the bank info, 6 the file image (the bytes), 7 the real factory, 8 the fake factory; 100 and up are made during a case.
//
// The case drives everything with tpw_sb_op and reads the state back with tpw_sb_state, tpw_sb_header and the other getters.
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>
#include <vector>

#include "TbDynamicStringTemplate.h"
#include "TbSoundBank.h"
#include "bfl_FileSystem.h"
#include "calllog.h"
#include "platform/samsutils.h"

#define TPW_ACCESS_ONLY
#define TPW_RECORDED_ACCESS_IO
#include "fake_file_slots.h"

using namespace calllog;
using bfl::FileSystem::CAccessItem;
using bfl::FileSystem::CFile;

// The string constructors the real strings need and nothing here calls.
template <>
void TbDynamicStringTemplate<char>::Format(const char*, ...) {}
template <>
void TbDynamicStringTemplate<char16_t>::Format(const char16_t*, ...) {}
template <>
void TbStaticStringTemplate<char>::Format(const char*, ...) {}

namespace {

// ---- the file ----------------------------------------------------------------------------------------------------------

struct FileWorld {
    std::vector<uint8_t> bytes;
    int64_t position = 0;
};
FileWorld gFile;

// Behind every copied file handle: the reads of the case's bytes.
class TestAccess : public CAccessItem {
public:
    int32_t Read(void* dest, uint32_t size) override {
        ev("CFile::Read", {size, lab(dest)});
        const int64_t mode = ret("Read", -2);
        if (mode == -1) return -1;
        uint32_t wanted = size;
        if (mode >= 0 && static_cast<uint64_t>(mode) < wanted) wanted = static_cast<uint32_t>(mode);
        const int64_t left = static_cast<int64_t>(gFile.bytes.size()) - gFile.position;
        const uint32_t count = left <= 0 ? 0 : (static_cast<int64_t>(wanted) < left ? wanted : static_cast<uint32_t>(left));
        if (dest != nullptr && count != 0) std::memcpy(dest, gFile.bytes.data() + gFile.position, count);
        gFile.position += count;
        return static_cast<int32_t>(count);
    }
};
TestAccess gAccess;

// ---- allocation --------------------------------------------------------------------------------------------------------

// The nothrow new and new[] of the case: the queue "Alloc" (1 succeeds, 0 fails; the last value repeats) and a limit of 64 MB.
// The recorded size of the two object sizes the bank code allocates is the Mac's (0x20 for a bank, 0xc for a CBank).
std::size_t RecordedSize(std::size_t size);

void* FakeAllocate(std::size_t size, bool array) {
    std::size_t recorded = array ? size : RecordedSize(size);
    const bool succeed = ret("Alloc", 1) != 0 && size < (64u << 20);
    if (!succeed) {
        ev(array ? "new[]" : "new", {static_cast<int64_t>(recorded), 0});
        return nullptr;
    }
    void* block = std::calloc(1, size != 0 ? size : 1);
    ev(array ? "new[]" : "new", {static_cast<int64_t>(recorded), newlab(block)});
    return block;
}

}  // namespace

void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    return FakeAllocate(size, false);
}

void* operator new[](std::size_t size, const std::nothrow_t&) noexcept {
    return FakeAllocate(size, true);
}

// ---- the file handle ---------------------------------------------------------------------------------------------------

CFile::CFile() : mImplementation(&gAccess) {}

CFile::CFile(const CFile& other) : mImplementation(&gAccess) {
    ev("CFile::CFile", {newlab(this), lab(&other)});
}

CFile::~CFile() {
    ev("CFile::~CFile", {lab(this)});
    forget(this);
}

void CFile::Close() {
    ev("CFile::Close", {lab(this)});
}

int32_t CFile::Seek(uint32_t offset, uint32_t whence) {
    ev("CFile::Seek", {lab(this), offset, whence});
    if (ret("Seek", 0) == -1) return -1;
    int64_t target;
    switch (whence) {
        case 0: target = offset; break;
        case 1: target = gFile.position + static_cast<int32_t>(offset); break;
        case 2: target = static_cast<int64_t>(gFile.bytes.size()) + static_cast<int32_t>(offset); break;
        default: return -1;
    }
    if (target < 0) return -1;
    gFile.position = target;
    return static_cast<int32_t>(target);
}

int32_t CFile::GetSize() {
    return static_cast<int32_t>(gFile.bytes.size());
}

int32_t CFile::GetPos() {
    return static_cast<int32_t>(gFile.position);
}

void* CFile::MapFile() {
    ev("CFile::MapFile", {lab(this)});
    if (ret("Map", 1) == 0 || gFile.bytes.empty()) return nullptr;
    return gFile.bytes.data();
}

TbError CFile::Flush() {
    return TbError(0);
}

int32_t CFile::GetMacFileReference() {
    return -1;
}

CAccessItem::~CAccessItem() {}

int32_t CAccessItem::Read(void*, uint32_t) {
    return -1;
}

int32_t CAccessItem::Write(const void*, uint32_t) {
    return -1;
}

// ---- the platform ------------------------------------------------------------------------------------------------------

// The platform's swap does nothing on a little-endian host (the emulator's stub swaps: it reads the file's words as big-endian).
void SamsUtilities::_USwapBlock32(uint32_t* words, int32_t count) {
    ev("_USwapBlock32", {lab(words), count});
}

SamsUtilities::UString::UString(const char* text) {
    ev("UString::UString", {newlab(this)}, {text});
}

SamsUtilities::UString::~UString() {
    ev("UString::~UString", {lab(this)});
    forget(this);
}

namespace {

// ---- the owners --------------------------------------------------------------------------------------------------------

// What a bank holds a reference on: records AddRef and Release.
class FakeOwner : public TbRefRelease {
public:
    int32_t Release() override {
        ev("Owner.Release", {lab(this)});
        return 0;
    }
    int32_t AddRef() override {
        ev("Owner.AddRef", {lab(this)});
        return 0;
    }
};

class FakeComponent : public CAudioComponent {
public:
    // CAudioComponent (include/TbSoundCore.h, unit w79b) is abstract: its access slots are never called here.
    TbError GetSystemInfo(TbSoundSystemInfo&) override { return TbError(0); }
    TbError GetSampleInfo(void*, TbSoundSampleInfo&) override { return TbError(0); }
    void* NewSample(void*, TbSoundSampleInfo&) override { return nullptr; }
    void* NewControl(void*, TbSoundSampleInfo&) override { return nullptr; }
    TbError SystemCommand(const SystemCommand::Command&) override { return TbError(0); }
    TbSoundSystemModule* GetSystemModule() const override { return nullptr; }
    TbSoundSampleStore* GetSampleStore() const override { return nullptr; }
    int32_t Release() override {
        ev("Owner.Release", {lab(this)});
        return 0;
    }
    int32_t AddRef() override {
        ev("Owner.AddRef", {lab(this)});
        return 0;
    }
};

// ---- a bank whose RetrieveSample is the case's ---------------------------------------------------------------------------

// The queue "Retrieve" gives the error of each RetrieveSample (an error leaves the header out unset).
class FakeBank : public TbBank {
public:
    explicit FakeBank(TbRefRelease* owner) : TbBank(owner) {}
    TbError RegisterBank(const char*, uint32_t) override {
        ev("Fake.RegisterBank", {lab(this)});
        return TbError(0);
    }
    TbError RetrieveSample(TbSampleHeader** out, uint32_t index) override {
        ev("Fake.RetrieveSample", {lab(this), index});
        const int32_t error = static_cast<int32_t>(ret("Retrieve", 0));
        if (error >= 0) *out = &header;
        return TbError(error);
    }
    bool IsType(uint32_t) const override { return false; }
    void Deregister() override { ev("Fake.Deregister", {lab(this)}); }
    TbSampleHeader header = {};
};

// ---- the world of a case -------------------------------------------------------------------------------------------------

constexpr int kSlots = 4;
enum Kind { kNone = 0, kMM = 1, kFile = 2, kFake = 3 };

TbBank* gBank[kSlots];
int gKind[kSlots];
Bank::CBank* gCBank[2];
CFile gFileObject[2];
FakeOwner gOwner;
FakeComponent gComponent;
alignas(16) unsigned char gStorageBuffer[64];
CBankFactory* gFactory[2];
CRefManager* gRef;
TbSoundBankInfo gInfo(0, "");
std::vector<char> gPath;
int64_t gAux[8];
TbSampleHeader* const kSentinel = reinterpret_cast<TbSampleHeader*>(static_cast<uintptr_t>(0xdeadbeef));

class FakeFactory : public CBankFactory {
public:
    TbError ConstructBank(TbBank*& out, const TbSoundBankInfo& info, CAudioComponent* component) override {
        ev("Factory.ConstructBank", {lab(this), lab(&info), lab(component)});
        const int64_t slot = ret("CBSlot", -1);
        out = slot >= 0 && slot < kSlots ? gBank[slot] : nullptr;
        return TbError(static_cast<int32_t>(ret("CBErr", 0)));
    }
};

// The Mac's size of the objects the bank code allocates one at a time: 0x20 for a bank (the fake bank is built like one), 0xc
// for a CBank and 4 for a factory.
std::size_t RecordedSize(std::size_t size) {
    if (size == sizeof(TbMMFileBank) || size == sizeof(FakeBank)) return 0x20;
    if (size == sizeof(Bank::CBank)) return 0xc;
    if (size == sizeof(CBankFactory) || size == sizeof(CRefManager)) return 4;
    return size;
}

TbRefRelease* OwnerOf(int64_t which) {
    if (which == 1) return &gOwner;
    if (which == 2) return &gComponent;
    return nullptr;
}

// The pointer a RetrieveSample answered, in a form both sides can compare: -2 unset, -3 NULL, else the index in the header
// array (TbMMFileBank) or the offset in the file image (TbFileBank).
int64_t Normalise(int slot, TbSampleHeader* header) {
    if (header == kSentinel) return -2;
    if (header == nullptr) return -3;
    if (gKind[slot] == kMM) return header - static_cast<TbMMFileBank*>(gBank[slot])->mHeaders;
    if (gKind[slot] == kFile) return reinterpret_cast<uint8_t*>(header) - gBank[slot]->mData;
    return header == &static_cast<FakeBank*>(gBank[slot])->header ? 0 : -4;
}

// A bank that was released down to nothing (its label is gone with the delete) is no longer there.
bool Alive(int slot) {
    return gBank[slot] != nullptr && lab(gBank[slot]) > 0;
}

bool CBankAlive(int slot) {
    return gCBank[slot] != nullptr && lab(gCBank[slot]) > 0;
}

int64_t Err(const TbError& error) {
    return static_cast<int32_t>(error);
}

void Reset() {
    for (auto& bank : gBank) bank = nullptr;
    for (auto& kind : gKind) kind = kNone;
    for (auto& cbank : gCBank) cbank = nullptr;
    for (auto& factory : gFactory) factory = nullptr;
    gRef = nullptr;
    for (auto& value : gAux) value = 0;
}

}  // namespace

enum Op {
    kNewMM = 0, kNewFile, kNewFake, kRegister, kRetrieve, kLength, kPosition, kName, kHandle, kIsType, kStreaming, kNumber,
    kInitialise, kDeregister, kDestroy, kPoke, kNewFactory, kConstruct, kGenerate, kCBankNew, kCBankFile, kCBankRetrieve,
    kCBankDestroy, kRefNew, kRefDel
};

extern "C" {

// A case: the file's bytes, the fixed labels, the queues.
void tpw_sb_begin(const uint8_t* bytes, int32_t length) {
    Reset();
    gFile.bytes.assign(bytes, bytes + length);
    gFile.position = 0;
}

// Labels and recording start after the queues are in place (the Python side calls tpw_cl_reset and the queue setters first).
void tpw_sb_labels() {
    tpw_cl_label(1, &gComponent);
    tpw_cl_label(2, &gOwner);
    tpw_cl_label(3, &gFileObject[0]);
    tpw_cl_label(5, &gFileObject[1]);
    tpw_cl_label(4, &gInfo);
    tpw_cl_label(6, gFile.bytes.data());
}

int64_t tpw_sb_header_size() {
    return sizeof(TbSampleHeader);
}

// One operation: a = the bank slot, b, c and d depend on the operation.
int64_t tpw_sb_op(int32_t op, int64_t a, int64_t b, int64_t c, int64_t d) {
    for (auto& value : gAux) value = 0;
    const int slot = static_cast<int>(a);
    // an operation on a bank the case could not make (its allocation failed) is skipped
    if (op >= kRegister && op <= kDeregister && !Alive(slot)) return -99;
    if (op == kPoke && !Alive(slot)) return -99;
    if ((op == kCBankFile || op == kCBankRetrieve) && !CBankAlive(slot)) return -99;
    if ((op == kConstruct || op == kGenerate) && gFactory[slot] == nullptr) return -99;
    switch (op) {
        case kNewMM:
        case kNewFile: {
            // b: the owner (0 none, 1 the owner, 2 the component), c: the file object
            TbRefRelease* owner = OwnerOf(b);
            if (op == kNewMM) {
                gBank[slot] = new (std::nothrow) TbMMFileBank(owner, &gFileObject[c]);
            } else {
                gBank[slot] = new (std::nothrow) TbFileBank(owner, &gFileObject[c]);
            }
            gKind[slot] = op == kNewMM ? kMM : kFile;
            return gBank[slot] != nullptr;
        }
        case kNewFake:
            gBank[slot] = new (std::nothrow) FakeBank(OwnerOf(b));
            gKind[slot] = kFake;
            return gBank[slot] != nullptr;
        case kRegister:
            return Err(gBank[slot]->RegisterBank("path", static_cast<uint32_t>(b)));
        case kRetrieve: {
            TbSampleHeader* header = kSentinel;
            const int64_t error = Err(gBank[slot]->RetrieveSample(&header, static_cast<uint32_t>(b)));
            gAux[0] = Normalise(slot, header);
            return error;
        }
        case kLength:
            return gBank[slot]->GetSampleLength(static_cast<uint32_t>(b));
        case kPosition:
            return gBank[slot]->GetSamplePosition(static_cast<uint32_t>(b));
        case kName:
            return gBank[slot]->GetSampleName(static_cast<uint32_t>(b)) != nullptr;
        case kHandle: {
            CFile* handle = gBank[slot]->GetHandle();
            if (handle == nullptr) return 0;
            if (gKind[slot] == kMM) return handle == &static_cast<TbMMFileBank*>(gBank[slot])->mFile ? 1 : 2;
            return 2;
        }
        case kIsType:
            return gBank[slot]->IsType(static_cast<uint32_t>(b));
        case kStreaming:
            return gBank[slot]->IsStreaming();
        case kNumber:
            return gBank[slot]->GetNumberOfSamples();
        case kInitialise: {
            TbRefRelease* out = nullptr;
            const int64_t error = Err(gBank[slot]->Initialise(&out));
            gAux[0] = out == gBank[slot] ? 1 : out == nullptr ? 0 : 2;
            return error;
        }
        case kDeregister:
            gBank[slot]->Deregister();
            return 0;
        case kDestroy: {
            // b: the Mac's flag (positive deletes the bank too); the slot is gone afterwards
            TbBank* bank = Alive(slot) ? gBank[slot] : nullptr;
            if (bank != nullptr && static_cast<int16_t>(b) > 0) {
                delete bank;
            } else if (bank != nullptr) {
                bank->~TbBank();
            }
            gBank[slot] = nullptr;
            gKind[slot] = kNone;
            return 0;
        }
        case kPoke: {
            // b: what (0 count, 1 size, 2 rate, 3 bits, 4 flags, 5 field 8, 6 field c), c: the value; on a fake bank's header
            FakeBank* fake = static_cast<FakeBank*>(gBank[slot]);
            switch (b) {
                case 0: fake->mCount = static_cast<uint32_t>(c); break;
                case 1: fake->header.mSize = static_cast<int32_t>(c); break;
                case 2: fake->header.mRate = static_cast<uint16_t>(c); break;
                case 3: fake->header.mBits = static_cast<uint8_t>(c); break;
                case 4: fake->header.mFlags = static_cast<uint8_t>(c); break;
                case 5: fake->header.mField8 = static_cast<int32_t>(c); break;
                default: fake->header.mFieldC = static_cast<uint32_t>(c); break;
            }
            return 0;
        }
        case kNewFactory: {
            // a: 0 the real factory, 1 the fake
            if (a == 0) gFactory[a] = new (std::nothrow) CBankFactory();
            else gFactory[a] = new (std::nothrow) FakeFactory();
            return gFactory[a] != nullptr;
        }
        case kConstruct: {
            // a: the factory, b: the slot for the bank, c: the owner, d: the file object
            gInfo = TbSoundBankInfo(7, "bank", &gFileObject[d]);
            gInfo.mSize = 1234;
            TbBank* made = reinterpret_cast<TbBank*>(kSentinel);
            const int64_t error = Err(gFactory[a]->ConstructBank(made, gInfo, c == 1 ? &gComponent : nullptr));
            gAux[0] = made == reinterpret_cast<TbBank*>(kSentinel) ? -2 : made == nullptr ? -3 : 1;
            if (made != nullptr && made != reinterpret_cast<TbBank*>(kSentinel)) {
                gBank[b] = made;
                gKind[b] = kMM;
            }
            return error;
        }
        case kGenerate: {
            // a: the factory, b: the CBank slot, c: the component (0 none, 1 the component), d: the file object
            gPath.assign({'p', 'a', 't', 'h', '.', 's', 'd', 't', 0});
            gInfo = TbSoundBankInfo(7, gPath.data(), &gFileObject[d]);
            gInfo.mSize = 77;
            Bank::CBank* made = gFactory[a]->GenerateBank(gInfo, c == 1 ? &gComponent : nullptr);
            gCBank[b] = made;
            if (made != nullptr) {
                gAux[0] = lab(made);
            }
            return made != nullptr;
        }
        case kCBankNew: {
            // a: the CBank slot, b: the bank slot
            gCBank[a] = new (std::nothrow) Bank::CBank(b >= 0 ? gBank[b] : nullptr);
            return gCBank[a] != nullptr;
        }
        case kCBankFile: {
            CFile* file = static_cast<CFile*>(*gCBank[a]);
            if (file == nullptr) return 0;
            for (int s = 0; s < kSlots; ++s) {
                if (gKind[s] == kMM && file == &static_cast<TbMMFileBank*>(gBank[s])->mFile) return 1;
            }
            return 2;
        }
        case kCBankRetrieve: {
            // a: the CBank slot, b: the sample index, c: the bank slot (for the normalisation)
            TbSampleHeader* header = kSentinel;
            const int64_t result = gCBank[a]->RetrieveSample(&header, static_cast<uint32_t>(b));
            gAux[0] = c >= 0 && gBank[c] != nullptr ? Normalise(static_cast<int>(c), header)
                                                    : header == kSentinel ? -2 : header == nullptr ? -3 : -4;
            return result;
        }
        case kRefNew:
            // a: the owner (0 none, 1 the owner, 2 the component)
            gRef = new (std::nothrow) CRefManager(OwnerOf(a));
            return gRef != nullptr;
        case kRefDel:
            // a: the Mac's flag
            if (static_cast<int16_t>(a) > 0) {
                delete gRef;
            } else {
                gRef->~CRefManager();
            }
            gRef = nullptr;
            return 0;
        case kCBankDestroy: {
            Bank::CBank* cbank = CBankAlive(static_cast<int>(a)) ? gCBank[a] : nullptr;
            if (cbank != nullptr && static_cast<int16_t>(b) > 0) {
                delete cbank;
            } else if (cbank != nullptr) {
                cbank->~CBank();
            }
            gCBank[a] = nullptr;
            return 0;
        }
    }
    return 0;
}

int64_t tpw_sb_aux(int32_t index) {
    return gAux[index];
}

// The state of bank `slot`: exists, kind, reference count, count, label of the data word, label of the headers word, the
// reference count of the owner's AddRef is not counted here (the owners are fakes).
void tpw_sb_state(int32_t slot, int64_t* out) {
    TbBank* bank = gBank[slot];
    out[0] = Alive(slot);
    if (!Alive(slot)) {
        for (int i = 1; i < 6; ++i) out[i] = 0;
        return;
    }
    out[1] = gKind[slot];
    out[2] = bank->mRefCount;
    out[3] = bank->mCount;
    out[4] = lab(bank->mData);
    if (gKind[slot] == kMM) out[5] = lab(static_cast<TbMMFileBank*>(bank)->mHeaders);
    else if (gKind[slot] == kFile) out[5] = lab(static_cast<TbFileBank*>(bank)->mCache);
    else out[5] = 0;
}

int64_t tpw_sb_offset(int32_t slot, int32_t index) {
    return reinterpret_cast<const uint32_t*>(gBank[slot]->mData)[index];
}

// The header i of a TbMMFileBank: size, rate, bits, flags, field 8, field c, the label of the data block.
void tpw_sb_header(int32_t slot, int32_t index, int64_t* out) {
    const TbSampleHeader& header = static_cast<TbMMFileBank*>(gBank[slot])->mHeaders[index];
    out[0] = header.mSize;
    out[1] = header.mRate;
    out[2] = header.mBits;
    out[3] = header.mFlags;
    out[4] = header.mField8;
    out[5] = header.mFieldC;
    out[6] = lab(header.mData);
}

void tpw_sb_hdata(int32_t slot, int32_t index, uint8_t* out, int32_t length) {
    std::memcpy(out, static_cast<TbMMFileBank*>(gBank[slot])->mHeaders[index].mData, length);
}

// The cache of a TbFileBank: the offset of the resolved header in the image, or -1.
int64_t tpw_sb_cache(int32_t slot, int32_t index) {
    TbSampleHeader* header = static_cast<TbFileBank*>(gBank[slot])->mCache[index];
    return header == nullptr ? -1 : reinterpret_cast<uint8_t*>(header) - gBank[slot]->mData;
}

int32_t tpw_sb_image(uint8_t* out, int32_t length) {
    const int32_t count = static_cast<int32_t>(gFile.bytes.size()) < length ? static_cast<int32_t>(gFile.bytes.size()) : length;
    if (count > 0) std::memcpy(out, gFile.bytes.data(), count);
    return count;
}

// The state of CBank `slot`: exists, reference count, the bank slot it holds (-1 none).
void tpw_sb_cbank(int32_t slot, int64_t* out) {
    Bank::CBank* cbank = CBankAlive(slot) ? gCBank[slot] : nullptr;
    out[0] = cbank != nullptr;
    out[1] = cbank != nullptr ? cbank->mRefCount : 0;
    out[2] = -1;
    if (cbank != nullptr)
        for (int s = 0; s < kSlots; ++s)
            if (Alive(s) && gBank[s] == cbank->mBank) out[2] = s;
}

// ---- small functions -----------------------------------------------------------------------------------------------------

uint32_t tpw_sb_swap32(int32_t which, uint32_t value) {
    switch (which) {
        case 0: return SamsUtilities::USwap32(value);
        case 1: return static_cast<uint32_t>(SamsUtilities::USwap32(static_cast<int32_t>(value)));
        default: return SamsUtilities::USwap32(static_cast<uint32_t>(value));
    }
}

uint32_t tpw_sb_swap16(uint32_t value) {
    return SamsUtilities::USwap16(static_cast<uint16_t>(value));
}

void tpw_sb_swap_inplace(uint32_t* word) {
    SamsUtilities::USwapInplace32(*word);
}

void tpw_sb_swap_block(uint32_t* words, int32_t count) {
    SamsUtilities::USwapBlock32(words, count);
}

uint32_t tpw_sb_cvt(double value) {
    return CvtDblUl(value);
}

// The info: kind 0 default, 1 (type, path), 2 (type, path, size), 3 (type, path, file); the fields were pre-filled with
// type 0xAAAAAAAA, size 0xBBBBBBBB, a path "pre" and the file object 1 (label 5). out: type, size, the path text as a
// number (see below), the file's label.
void tpw_sb_info(int32_t kind, uint32_t type, const char* path, uint32_t size, int32_t fileLabel, int64_t* out, char* pathOut) {
    alignas(TbSoundBankInfo) unsigned char storage[sizeof(TbSoundBankInfo)];
    TbSoundBankInfo* info = reinterpret_cast<TbSoundBankInfo*>(storage);
    info->mType = 0xAAAAAAAAu;
    info->mSize = 0xBBBBBBBBu;
    info->mPath = "pre";
    info->mFile = &gFileObject[1];
    CFile* file = fileLabel == 3 ? &gFileObject[0] : fileLabel == 5 ? &gFileObject[1] : nullptr;
    switch (kind) {
        case 0: new (storage) TbSoundBankInfo(); break;
        case 1: new (storage) TbSoundBankInfo(type, path); break;
        case 2: new (storage) TbSoundBankInfo(type, path, size); break;
        default: new (storage) TbSoundBankInfo(type, path, file); break;
    }
    out[0] = info->GetPath() == nullptr ? 0 : 1;
    out[1] = info->mType;
    out[2] = info->GetSize();
    out[3] = info->GetFile() == &gFileObject[0] ? 3 : info->GetFile() == &gFileObject[1] ? 5 : info->GetFile() == nullptr ? 0 : -1;
    std::strncpy(pathOut, info->GetPath() != nullptr ? info->GetPath() : "(null)", 31);
    pathOut[31] = 0;
    // GetFile, GetSize and GetPath answer the fields
    out[4] = info->GetFile() == info->mFile && info->GetSize() == info->mSize && info->GetPath() == info->mPath;
}

// TbSoundSampleInfo::SetBank over a pre-filled record: out: the flags, 0 NULL / 1 the bank / 2 the old bank.
void tpw_sb_setbank(uint32_t flags, int32_t nonNull, int64_t* out) {
    Bank::CBank* const token = reinterpret_cast<Bank::CBank*>(static_cast<uintptr_t>(0x5678));
    TbSoundSampleInfo info = {};
    info.mFlags = flags;
    info.mBank = reinterpret_cast<Bank::CBank*>(static_cast<uintptr_t>(0x1234));
    info.SetBank(nonNull != 0 ? token : nullptr);
    out[0] = info.mFlags;
    out[1] = info.mBank == nullptr ? 0 : info.mBank == token ? 1 : 2;
}

// CFileOperation::GetBankPath over a real string: answers the result and writes the string's text.
int32_t tpw_sb_bankpath(const char* initial, char* out, int32_t capacity) {
    TbDynamicStringTemplate<char> path("unused");
    TbDynamicStringTemplate<char> name(initial);
    const int32_t result = CFileOperation::GetBankPath(*reinterpret_cast<bfl::FileSystem::CFileStorage*>(gStorageBuffer), path, name, 5);
    std::strncpy(out, name.mData != nullptr ? name.mData : "", capacity - 1);
    out[capacity - 1] = 0;
    return result;
}

// The watcher's constructor has no effect; construct one over a UString.
int32_t tpw_sb_watcher() {
    SamsUtilities::UString name("x");
    SamsUtilities::UMemoryWatcher watcher(name);
    return 1;
}

}  // extern "C"
