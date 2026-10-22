// Host tests of the asset launcher's container readers (assets/): SHA-256 known answers, a zip built in memory (stored,
// deflate, data descriptor, zip64), a folder, the US Mac disc's zip, three PC disc images and the Gold disc's zip.
// The inputs are the player's own files, read in place and never copied. A test whose input is absent is skipped with a
// message. Expected hashes of the PC images come from JSON lists of path, size and SHA-256 made by hashing the images with a script (TPW_TEST_EXPECT).
//
// usage: tpw_assets_tests [--quick]      --quick hashes only the first 25 files of each image
// environment: TPW_GDI_ROOT (the ROOT folder), TPW_TEST_TRANSFER (the disc archive folder), TPW_TEST_EXPECT (the JSON folder),
// TPW_TEST_SCRATCH (a folder with room for the Mac disc's 670 MB image, which is unpacked there and deleted)
#include <zlib.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>

#include "../macroman.h"
#include "../sniff.h"
#include "../source.h"

using namespace tpw::assets;

namespace {

int g_fail = 0, g_pass = 0, g_skip = 0;
bool g_quick = false;

#define CHECK(cond)                                                                    \
    do {                                                                               \
        if (cond) {                                                                    \
            g_pass++;                                                                  \
        } else {                                                                       \
            g_fail++;                                                                  \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                \
        }                                                                              \
    } while (0)

#define CHECK_EQ_STR(a, b)                                                                                   \
    do {                                                                                                     \
        std::string va = (a), vb = (b);                                                                      \
        if (va == vb) {                                                                                      \
            g_pass++;                                                                                        \
        } else {                                                                                             \
            g_fail++;                                                                                        \
            std::printf("FAIL %s:%d: %s: \"%s\" != \"%s\"\n", __FILE__, __LINE__, #a, va.c_str(), vb.c_str()); \
        }                                                                                                    \
    } while (0)

void Skip(const std::string& what) {
    g_skip++;
    std::printf("SKIP %s\n", what.c_str());
}

std::string Env(const char* name, const std::string& fallback) {
    const char* v = std::getenv(name);
    return v && *v ? v : fallback;
}

bool Exists(const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    std::fclose(f);
    return true;
}

const Entry* Find(const Source& s, const std::string& path, EntryKind kind = EntryKind::Data) {
    for (const Entry& e : s.Entries()) {
        if (e.path == path && e.kind == kind) return &e;
    }
    return nullptr;
}

std::string HashEntry(Source& s, const Entry& e) {
    std::string err;
    auto st = s.Open(e, err);
    if (!st) return "open failed: " + err;
    Sha256Digest d;
    if (!HashStream(*st, d, err)) return "read failed: " + err;
    return d.Hex();
}

// ---- SHA-256

void TestSha256() {
    CHECK_EQ_STR(Sha256::Of("", 0).Hex(), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    CHECK_EQ_STR(Sha256::Of("abc", 3).Hex(), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    const char* two = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    CHECK_EQ_STR(Sha256::Of(two, std::strlen(two)).Hex(), "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    // One million 'a', in chunks of an awkward size.
    Sha256 s;
    std::string chunk(997, 'a');
    size_t left = 1000000;
    while (left > 0) {
        size_t n = left < chunk.size() ? left : chunk.size();
        s.Update(chunk.data(), n);
        left -= n;
    }
    CHECK_EQ_STR(s.Final().Hex(), "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
    // The object is reusable after Final, and chunking never changes the answer around the padding boundaries.
    for (size_t len : {55u, 56u, 57u, 63u, 64u, 65u, 119u, 120u, 128u}) {
        std::string msg(len, 'x');
        for (size_t i = 0; i < len; i++) msg[i] = char('a' + i % 26);
        std::string one = Sha256::Of(msg.data(), len).Hex();
        s.Update(msg.data(), len / 3);
        s.Update(msg.data() + len / 3, len - len / 3);
        CHECK_EQ_STR(s.Final().Hex(), one);
    }
}

// ---- A zip built in memory

void Put16(std::vector<uint8_t>& v, uint32_t x) { v.push_back(uint8_t(x)); v.push_back(uint8_t(x >> 8)); }
void Put32(std::vector<uint8_t>& v, uint32_t x) { Put16(v, x & 0xFFFF); Put16(v, x >> 16); }
void Put64(std::vector<uint8_t>& v, uint64_t x) { Put32(v, uint32_t(x)); Put32(v, uint32_t(x >> 32)); }
void PutStr(std::vector<uint8_t>& v, const std::string& s) { v.insert(v.end(), s.begin(), s.end()); }

struct ZipItem {
    std::string name;
    std::string data;
    bool deflate;
    bool descriptor;
    bool zip64;
};

std::vector<uint8_t> RawDeflate(const std::string& in) {
    z_stream z{};
    deflateInit2(&z, 6, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY);
    std::vector<uint8_t> out(deflateBound(&z, uLong(in.size())) + 16);
    z.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(in.data()));
    z.avail_in = uInt(in.size());
    z.next_out = out.data();
    z.avail_out = uInt(out.size());
    deflate(&z, Z_FINISH);
    out.resize(z.total_out);
    deflateEnd(&z);
    return out;
}

std::vector<uint8_t> BuildZip(const std::vector<ZipItem>& items, bool zip64End, size_t prefix = 0) {
    std::vector<uint8_t> out(prefix, 0x55);
    struct Done { uint64_t offset, csize, usize; uint32_t crc; uint16_t method, flags; bool zip64; };
    std::vector<Done> done;
    for (const ZipItem& it : items) {
        std::vector<uint8_t> body;
        if (it.deflate) body = RawDeflate(it.data); else body.assign(it.data.begin(), it.data.end());
        uint32_t crc = uint32_t(crc32(crc32(0, nullptr, 0), reinterpret_cast<const Bytef*>(it.data.data()), uInt(it.data.size())));
        Done d{out.size() - prefix, body.size(), it.data.size(), crc, uint16_t(it.deflate ? 8 : 0),
               uint16_t(it.descriptor ? 8 : 0), it.zip64};
        Put32(out, 0x04034b50);
        Put16(out, 20);
        Put16(out, d.flags);
        Put16(out, d.method);
        Put32(out, 0);
        Put32(out, it.descriptor ? 0 : crc);
        Put32(out, it.descriptor ? 0 : uint32_t(body.size()));
        Put32(out, it.descriptor ? 0 : uint32_t(it.data.size()));
        Put16(out, uint32_t(it.name.size()));
        Put16(out, 0);
        PutStr(out, it.name);
        out.insert(out.end(), body.begin(), body.end());
        if (it.descriptor) {
            Put32(out, 0x08074b50);
            Put32(out, crc);
            Put32(out, uint32_t(body.size()));
            Put32(out, uint32_t(it.data.size()));
        }
        done.push_back(d);
    }
    uint64_t cdStart = out.size() - prefix;
    for (size_t i = 0; i < items.size(); i++) {
        const Done& d = done[i];
        std::vector<uint8_t> extra;
        if (d.zip64) {
            Put16(extra, 1);
            Put16(extra, 24);
            Put64(extra, d.usize);
            Put64(extra, d.csize);
            Put64(extra, d.offset);
        }
        Put32(out, 0x02014b50);
        Put16(out, 45);
        Put16(out, 45);
        Put16(out, d.flags);
        Put16(out, d.method);
        Put32(out, 0);
        Put32(out, d.crc);
        Put32(out, d.zip64 ? 0xFFFFFFFFu : uint32_t(d.csize));
        Put32(out, d.zip64 ? 0xFFFFFFFFu : uint32_t(d.usize));
        Put16(out, uint32_t(items[i].name.size()));
        Put16(out, uint32_t(extra.size()));
        Put16(out, 0);
        Put16(out, 0);
        Put16(out, 0);
        Put32(out, 0);
        Put32(out, d.zip64 ? 0xFFFFFFFFu : uint32_t(d.offset));
        PutStr(out, items[i].name);
        out.insert(out.end(), extra.begin(), extra.end());
    }
    uint64_t cdSize = out.size() - prefix - cdStart;
    uint64_t z64At = out.size() - prefix;
    if (zip64End) {
        Put32(out, 0x06064b50);
        Put64(out, 44);
        Put16(out, 45);
        Put16(out, 45);
        Put32(out, 0);
        Put32(out, 0);
        Put64(out, items.size());
        Put64(out, items.size());
        Put64(out, cdSize);
        Put64(out, cdStart);
        Put32(out, 0x07064b50);
        Put32(out, 0);
        Put64(out, z64At);
        Put32(out, 1);
    }
    Put32(out, 0x06054b50);
    Put16(out, 0);
    Put16(out, 0);
    Put16(out, zip64End ? 0xFFFF : uint32_t(items.size()));
    Put16(out, zip64End ? 0xFFFF : uint32_t(items.size()));
    Put32(out, zip64End ? 0xFFFFFFFFu : uint32_t(cdSize));
    Put32(out, zip64End ? 0xFFFFFFFFu : uint32_t(cdStart));
    Put16(out, 3);
    PutStr(out, "end");
    return out;
}

void TestZipInMemory() {
    std::string text;
    for (int i = 0; i < 20000; i++) text += "line " + std::to_string(i) + " of the sample text\n";
    std::string small = "tiny";
    std::vector<ZipItem> items = {
        {"a/stored.txt", small, false, false, false},
        {"a/deflated.txt", text, true, false, false},
        {"b\\descriptor.txt", text, true, true, false},
        {"b/big64.txt", text, true, false, true},
        {"empty.txt", "", false, false, false},
        {"dir/", "", false, false, false},
        {"../escape.txt", small, false, false, false},
    };
    for (int variant = 0; variant < 3; variant++) {
        bool z64 = variant == 1;
        size_t prefix = variant == 2 ? 777 : 0;
        std::string err;
        auto src = OpenZip(MakeMemoryBlob(BuildZip(items, z64, prefix)), err);
        CHECK(src != nullptr);
        if (!src) {
            std::printf("  %s\n", err.c_str());
            continue;
        }
        CHECK(src->Entries().size() == 5);  // the directory and the entry that climbs out are not listed
        const Entry* a = Find(*src, "a/stored.txt");
        const Entry* b = Find(*src, "a/deflated.txt");
        const Entry* c = Find(*src, "b/descriptor.txt");
        const Entry* d = Find(*src, "b/big64.txt");
        const Entry* e = Find(*src, "empty.txt");
        CHECK(a && b && c && d && e);
        if (!(a && b && c && d && e)) continue;
        CHECK(a->size == small.size() && b->size == text.size());
        CHECK_EQ_STR(HashEntry(*src, *a), Sha256::Of(small.data(), small.size()).Hex());
        std::string want = Sha256::Of(text.data(), text.size()).Hex();
        CHECK_EQ_STR(HashEntry(*src, *b), want);
        CHECK_EQ_STR(HashEntry(*src, *c), want);
        CHECK_EQ_STR(HashEntry(*src, *d), want);
        CHECK_EQ_STR(HashEntry(*src, *e), Sha256::Of("", 0).Hex());
        // An entry as a Blob (a zip inside a zip would be read this way): random access, forward and back.
        std::shared_ptr<Source> shared(std::move(src));
        auto blob = OpenEntryBlob(shared, *b, err);
        CHECK(blob != nullptr);
        if (blob) {
            char buf[16];
            CHECK(blob->ReadAt(100, buf, 16, err) == 16 && std::memcmp(buf, text.data() + 100, 16) == 0);
            CHECK(blob->ReadAt(5, buf, 16, err) == 16 && std::memcmp(buf, text.data() + 5, 16) == 0);
            CHECK(blob->ReadAt(text.size() - 4, buf, 16, err) == 4);
            CHECK(blob->ReadAt(text.size(), buf, 16, err) == 0);
        }
    }
    // A damaged entry is reported, not returned as good data.
    std::vector<uint8_t> bad = BuildZip({{"x.txt", text, false, false, false}}, false);
    bad[30 + 5 + 100] ^= 1;
    std::string err;
    auto src = OpenZip(MakeMemoryBlob(bad), err);
    CHECK(src != nullptr);
    if (src) CHECK(HashEntry(*src, src->Entries()[0]).rfind("read failed", 0) == 0);
    // Not a zip.
    CHECK(OpenZip(MakeMemoryBlob(std::vector<uint8_t>(100, 7)), err) == nullptr);
}

// ---- Folder

void TestFolder(const std::string& root) {
    std::string dir = root + "/extracted/disc-hfs/Theme Park Data";
    std::string err;
    auto src = OpenFolder(dir, err);
    if (!src) {
        Skip("folder source: " + dir + " (" + err + ")");
        return;
    }
    size_t data = 0, rsrc = 0;
    for (const Entry& e : src->Entries()) (e.kind == EntryKind::Data ? data : rsrc)++;
    std::printf("folder: %zu data entries, %zu resource forks\n", data, rsrc);
    CHECK(data == 1546 && rsrc == 49);
    CHECK(Find(*src, "ReadMe.rsrc") == nullptr);  // the side file is the fork of ReadMe
    struct Want { const char* path; EntryKind kind; uint64_t size; const char* sha; };
    const Want wants[] = {
        {"ReadMe", EntryKind::Data, 9973, "0204b9ada39dafe80c397466b94a3e7161a4ae94642ad2225cd928bf3d13a4f8"},
        {"ReadMe", EntryKind::Rsrc, 332, "46b09b7447908eb73b12b2e3896a4b8889fe8f6967151251300066a4cb4da499"},
        {"SimTheme Park", EntryKind::Data, 2274758, "04809cd4ccee5433c7fb0b7c93d32f6a7aa629c1849181c0b7906415e5e295f5"},
        {"SimTheme Park", EntryKind::Rsrc, 130609, "000601114927848b15b901bea71ba557b5a86408960191c00ee1f554a3269e89"},
    };
    for (const Want& w : wants) {
        const Entry* e = Find(*src, w.path, w.kind);
        CHECK(e != nullptr);
        if (!e) continue;
        CHECK(e->size == w.size);
        CHECK_EQ_STR(HashEntry(*src, *e), w.sha);
    }
    CHECK(OpenFolder(dir + "/does-not-exist", err) == nullptr);
}

// ---- Minimal reader of the design study's JSON lists: {"path": [size, "sha256"], ...}

struct Expect {
    uint64_t size;
    std::string sha;
};

void AppendUtf8(std::string& s, uint32_t c) {
    if (c < 0x80) s += char(c);
    else if (c < 0x800) { s += char(0xC0 | (c >> 6)); s += char(0x80 | (c & 63)); }
    else if (c < 0x10000) { s += char(0xE0 | (c >> 12)); s += char(0x80 | ((c >> 6) & 63)); s += char(0x80 | (c & 63)); }
    else { s += char(0xF0 | (c >> 18)); s += char(0x80 | ((c >> 12) & 63)); s += char(0x80 | ((c >> 6) & 63)); s += char(0x80 | (c & 63)); }
}

bool LoadExpect(const std::string& path, std::map<std::string, Expect>& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::stringstream ss;
    ss << f.rdbuf();
    std::string t = ss.str();
    size_t i = 0;
    auto ws = [&]() { while (i < t.size() && (t[i] == ' ' || t[i] == '\n' || t[i] == '\r' || t[i] == '\t')) i++; };
    auto str = [&](std::string& s) {
        s.clear();
        if (i >= t.size() || t[i] != '"') return false;
        i++;
        while (i < t.size() && t[i] != '"') {
            if (t[i] == '\\' && i + 1 < t.size()) {
                char c = t[i + 1];
                i += 2;
                if (c == 'u' && i + 4 <= t.size()) {
                    uint32_t u = uint32_t(std::strtoul(t.substr(i, 4).c_str(), nullptr, 16));
                    i += 4;
                    if (u >= 0xD800 && u < 0xDC00 && i + 6 <= t.size() && t[i] == '\\' && t[i + 1] == 'u') {
                        uint32_t lo = uint32_t(std::strtoul(t.substr(i + 2, 4).c_str(), nullptr, 16));
                        i += 6;
                        u = 0x10000 + ((u - 0xD800) << 10) + (lo - 0xDC00);
                    }
                    AppendUtf8(s, u);
                } else {
                    s += c == 'n' ? '\n' : c == 't' ? '\t' : c;
                }
            } else {
                s += t[i++];
            }
        }
        i++;
        return true;
    };
    ws();
    if (i >= t.size() || t[i++] != '{') return false;
    for (;;) {
        ws();
        if (i < t.size() && t[i] == '}') return true;
        std::string key, sha;
        if (!str(key)) return false;
        ws();
        if (t[i++] != ':') return false;
        ws();
        if (t[i++] != '[') return false;
        ws();
        char* endp = nullptr;
        uint64_t size = std::strtoull(t.c_str() + i, &endp, 10);
        i = size_t(endp - t.c_str());
        ws();
        if (t[i++] != ',') return false;
        ws();
        if (!str(sha)) return false;
        ws();
        if (t[i++] != ']') return false;
        out[key] = Expect{size, sha};
        ws();
        if (i < t.size() && t[i] == ',') i++;
    }
}

// Compares every file of a source with a list: same files, sizes and hashes. Returns the counts for the log.
void CompareWithList(Source& src, const std::map<std::string, Expect>& want, const char* label) {
    size_t limit = g_quick ? 25 : size_t(-1);
    CHECK(src.Entries().size() == want.size());
    size_t checked = 0, bad = 0, missing = 0;
    uint64_t bytes = 0;
    for (const Entry& e : src.Entries()) {
        auto it = want.find(e.path);
        if (it == want.end()) {
            missing++;
            std::printf("  not in the list: %s\n", e.path.c_str());
            continue;
        }
        if (checked >= limit) continue;
        checked++;
        bytes += e.size;
        if (e.size != it->second.size || HashEntry(src, e) != it->second.sha) {
            bad++;
            if (bad < 10) std::printf("  differs: %s\n", e.path.c_str());
        }
    }
    std::printf("%s: %zu files listed, %zu hashed (%llu bytes), %zu differ, %zu not in the list\n", label,
                src.Entries().size(), checked, (unsigned long long)bytes, bad, missing);
    CHECK(bad == 0 && missing == 0);
}

void TestPcImage(const std::string& transfer, const std::string& expectDir, const char* dirName, const char* file,
                 const char* json, size_t files) {
    std::string path = transfer + "/1 ThemeParkWorld PC UK-European CD Bullfrog EA/" + dirName + "/" + file;
    std::string err;
    std::map<std::string, Expect> want;
    if (!Exists(path) || !LoadExpect(expectDir + "/" + json, want)) {
        Skip(std::string("iso source: ") + file + " or " + json + " is absent");
        return;
    }
    CHECK(want.size() == files);
    auto blob = OpenFileBlob(path, err);
    CHECK(blob != nullptr);
    if (!blob) return;
    auto src = OpenIso9660(blob, err);
    CHECK(src != nullptr);
    if (!src) {
        std::printf("  %s\n", err.c_str());
        return;
    }
    CHECK(std::string(src->Kind()) == "iso9660");
    CompareWithList(*src, want, file);
}

void TestPcImages(const std::string& transfer, const std::string& expectDir) {
    TestPcImage(transfer, expectDir, "ThemeParkWorld_PC_EA-Classics-CD-Image_2000-11_Bullfrog-EA_Win9x_redump-66484",
                "ThemeParkWorld.img", "p66484.json", 2839);
    TestPcImage(transfer, expectDir, "ThemeParkWorld_PC_European-CD-Image-EN-IT-NL-PL_1999-10_Bullfrog-EA_Win9x_redump-66100",
                "TPWORLD.mdf", "p66100.json", 2651);
    TestPcImage(transfer, expectDir, "ThemeParkWorld_PC_German-Play-the-Games-Vol4-CD-Image_2000-06_Bullfrog-EA_Win9x_redump-9465",
                "TPWORLD.bin", "p9465.json", 2817);
}

// ---- The US Mac disc's zip, and the Gold disc's zip holding a raw image

void TestMacZip(const std::string& transfer) {
    std::string path = transfer + "/2 SimThemePark Mac US CD v2.0.1 Feral Aspyr/"
                       "SimThemePark_Mac_US-CD-Image_v2.0.1_2000_Feral-Aspyr_PPC-Classic.toast.zip";
    std::string err;
    if (!Exists(path)) {
        Skip("zip source: the US Mac disc's zip is absent");
        return;
    }
    auto blob = OpenFileBlob(path, err);
    auto src = blob ? OpenZip(blob, err) : nullptr;
    CHECK(src != nullptr);
    if (!src) return;
    std::printf("mac zip:");
    for (const Entry& e : src->Entries()) std::printf(" [%s %llu]", e.path.c_str(), (unsigned long long)e.size);
    std::printf("\n");
    CHECK(src->Entries().size() == 2);
    const Entry* toast = Find(*src, "Theme Park.toast");
    const Entry* side = Find(*src, "__MACOSX/._Theme Park.toast");
    CHECK(toast && side);
    if (!(toast && side)) return;
    CHECK(toast->size == 670476288 && side->size == 82);
    CHECK_EQ_STR(HashEntry(*src, *side), "527e59a255929a8837cb120dc118923ca58bd30dcadc909811289c9f4b446202");
    if (g_quick) return;
    // 670 MB inflated and checked against its CRC-32, a chunk at a time.
    CHECK_EQ_STR(HashEntry(*src, *toast), "75cc5c39614cf17d20139382419727e9b471701c7a21e2036ecda9ab00f3e359");
}

void TestGold(const std::string& transfer, const std::string& expectDir) {
    std::string path = transfer + "/4 SimThemePark PC Gold Edition Bonus Rides and Attractions CD/"
                       "SimThemePark_PC_US-Gold-Edition-Bonus-Rides-and-Attractions-CD-Image_2000-07_Bullfrog-EA_Win9x.zip";
    std::string err;
    std::map<std::string, Expect> want;
    if (!Exists(path) || !LoadExpect(expectDir + "/gold.json", want)) {
        Skip("gold zip: the zip or gold.json is absent");
        return;
    }
    auto blob = OpenFileBlob(path, err);
    std::shared_ptr<Source> zip(blob ? OpenZip(blob, err) : nullptr);
    CHECK(zip != nullptr);
    if (!zip) return;
    CHECK(zip->Entries().size() == 2);
    const Entry* image = nullptr;
    for (const Entry& e : zip->Entries()) {
        if (e.path.size() > 4 && e.path.compare(e.path.size() - 4, 4, ".bin") == 0) image = &e;
    }
    CHECK(image != nullptr);
    if (!image) return;
    auto inner = OpenEntryBlob(zip, *image, err);
    CHECK(inner != nullptr);
    auto iso = inner ? OpenIso9660(inner, err) : nullptr;
    CHECK(iso != nullptr);
    if (!iso) {
        std::printf("  %s\n", err.c_str());
        return;
    }
    CHECK(want.size() == 5);
    CompareWithList(*iso, want, "gold image in its zip");
}


// ---- Mac Roman, MacBinary, a cabinet and an HFS volume built in memory

void TestMacRoman() {
    CHECK_EQ_STR(MacRomanToUtf8("abc", 3, true), "abc");
    CHECK_EQ_STR(MacRomanToUtf8("a/b", 3, true), "a:b");
    CHECK_EQ_STR(MacRomanToUtf8("a/b", 3, false), "a/b");
    CHECK_EQ_STR(MacRomanToUtf8("B\x8A", 2, true), "B\xC3\xA4");              // a diaeresis
    CHECK_EQ_STR(MacRomanToUtf8("\xDB", 1, true), "\xE2\x82\xAC");            // the Euro sign, as Python's mac_roman
    CHECK_EQ_STR(MacRomanToUtf8("\xF0", 1, true), "\xEF\xA3\xBF");            // the Apple logo's private-use code
    CHECK_EQ_STR(MacRomanToUtf8("Icon\r", 5, true), "Icon\r");                // the control character stays
}

void Put16B(std::vector<uint8_t>& v, uint32_t x) { v.push_back(uint8_t(x >> 8)); v.push_back(uint8_t(x)); }
void Put32B(std::vector<uint8_t>& v, uint32_t x) { Put16B(v, x >> 16); Put16B(v, x & 0xFFFF); }

uint16_t TestCrc16(const uint8_t* p, size_t n) {
    uint16_t crc = 0;
    for (size_t i = 0; i < n; i++) {
        crc = uint16_t(crc ^ (uint16_t(p[i]) << 8));
        for (int b = 0; b < 8; b++) crc = (crc & 0x8000) ? uint16_t((crc << 1) ^ 0x1021) : uint16_t(crc << 1);
    }
    return crc;
}

std::string Pattern(size_t n, unsigned seed) {
    std::string s(n, 0);
    uint32_t x = seed * 2654435761u + 1;
    for (size_t i = 0; i < n; i++) {
        x = x * 1664525u + 1013904223u;
        s[i] = char(x >> 24);
    }
    return s;
}

std::vector<uint8_t> BuildMacBinary(const std::string& name, const std::string& data, const std::string& rsrc, bool crc) {
    std::vector<uint8_t> h(128, 0);
    h[1] = uint8_t(name.size());
    std::memcpy(&h[2], name.data(), name.size());
    std::memcpy(&h[65], "TEXT", 4);
    std::memcpy(&h[69], "ttxt", 4);
    for (int i = 0; i < 4; i++) {
        h[83 + i] = uint8_t(data.size() >> (24 - 8 * i));
        h[87 + i] = uint8_t(rsrc.size() >> (24 - 8 * i));
    }
    if (crc) {
        h[122] = 129;
        h[123] = 129;
        uint16_t c = TestCrc16(h.data(), 124);
        h[124] = uint8_t(c >> 8);
        h[125] = uint8_t(c);
    }
    std::vector<uint8_t> out = h;
    out.insert(out.end(), data.begin(), data.end());
    out.resize((out.size() + 127) & ~size_t(127), 0);
    out.insert(out.end(), rsrc.begin(), rsrc.end());
    out.resize((out.size() + 127) & ~size_t(127), 0);
    return out;
}

void TestMacBinary() {
    std::string data = Pattern(1000, 1), rsrc = Pattern(300, 2);
    std::string err;
    for (int variant = 0; variant < 3; variant++) {
        // 0: MacBinary II (checksum), 1: MacBinary I (no checksum), 2: no resource fork
        std::vector<uint8_t> file = BuildMacBinary("Read\x8A Me/1", data, variant == 2 ? "" : rsrc, variant != 1);
        auto src = OpenMacBinary(MakeMemoryBlob(file), err);
        CHECK(src != nullptr);
        if (!src) continue;
        CHECK(std::string(src->Kind()) == "macbinary");
        CHECK(src->Entries().size() == (variant == 2 ? 1u : 2u));
        const Entry* d = Find(*src, "Read\xC3\xA4 Me:1");
        CHECK(d && d->size == data.size());
        if (d) CHECK_EQ_STR(HashEntry(*src, *d), Sha256::Of(data.data(), data.size()).Hex());
        const Entry* r = Find(*src, "Read\xC3\xA4 Me:1", EntryKind::Rsrc);
        CHECK((r != nullptr) == (variant != 2));
        if (r) CHECK_EQ_STR(HashEntry(*src, *r), Sha256::Of(rsrc.data(), rsrc.size()).Hex());
    }
    // A damaged checksum, a header on garbage, a file cut short, and an ordinary file are refused.
    std::vector<uint8_t> file = BuildMacBinary("name", data, rsrc, true);
    file[10] ^= 1;
    CHECK(OpenMacBinary(MakeMemoryBlob(file), err) == nullptr);
    file = BuildMacBinary("name", data, rsrc, true);
    file.resize(300);
    CHECK(OpenMacBinary(MakeMemoryBlob(file), err) == nullptr);
    CHECK(OpenMacBinary(MakeMemoryBlob(std::vector<uint8_t>(200, 0)), err) == nullptr);
    CHECK(OpenMacBinary(MakeMemoryBlob(std::vector<uint8_t>(500, 'x')), err) == nullptr);
}

uint32_t TestCabSum(const uint8_t* p, size_t n, uint32_t seed) {
    uint32_t s = seed;
    size_t i = 0;
    for (; i + 4 <= n; i += 4) s ^= uint32_t(p[i]) | (uint32_t(p[i + 1]) << 8) | (uint32_t(p[i + 2]) << 16) | (uint32_t(p[i + 3]) << 24);
    uint32_t last = 0;
    // The leftover bytes, first one highest, as in Microsoft's and libmspack's code.
    for (size_t k = 0; i + k < n; k++) last = (last << 8) | p[i + k];
    return s ^ last;
}

struct CabItem {
    std::string name;
    std::string data;
};

// One folder; MSZIP blocks of 10000 bytes, each deflated with the previous block as its dictionary.
std::vector<uint8_t> BuildCab(const std::vector<CabItem>& items, bool mszip, bool checksums) {
    std::string all;
    for (const CabItem& it : items) all += it.data;
    std::vector<std::vector<uint8_t>> blocks;
    std::string prev;
    size_t nBlocks = 0;
    for (size_t at = 0; at < all.size(); at += 10000, nBlocks++) {
        std::string raw = all.substr(at, 10000);
        std::vector<uint8_t> payload;
        if (mszip) {
            z_stream z{};
            deflateInit2(&z, 6, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY);
            if (!prev.empty()) deflateSetDictionary(&z, reinterpret_cast<const Bytef*>(prev.data()), uInt(prev.size()));
            std::vector<uint8_t> out(deflateBound(&z, uLong(raw.size())) + 16);
            z.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(raw.data()));
            z.avail_in = uInt(raw.size());
            z.next_out = out.data();
            z.avail_out = uInt(out.size());
            deflate(&z, Z_FINISH);
            out.resize(z.total_out);
            deflateEnd(&z);
            payload = {'C', 'K'};
            payload.insert(payload.end(), out.begin(), out.end());
        } else {
            payload.assign(raw.begin(), raw.end());
        }
        std::vector<uint8_t> b;
        std::vector<uint8_t> sizes;
        Put16(sizes, uint32_t(payload.size()));
        Put16(sizes, uint32_t(raw.size()));
        Put32(b, checksums ? TestCabSum(payload.data(), payload.size(), TestCabSum(sizes.data(), 4, 0)) : 0);
        b.insert(b.end(), sizes.begin(), sizes.end());
        b.insert(b.end(), payload.begin(), payload.end());
        blocks.push_back(std::move(b));
        prev = raw;
    }
    std::vector<uint8_t> files;
    uint32_t off = 0;
    for (const CabItem& it : items) {
        Put32(files, uint32_t(it.data.size()));
        Put32(files, off);
        Put16(files, 0);
        Put16(files, 0x2821);
        Put16(files, 0x6000);
        Put16(files, 0x20);
        PutStr(files, it.name);
        files.push_back(0);
        off += uint32_t(it.data.size());
    }
    uint32_t coffFiles = 36 + 8;
    uint32_t coffData = coffFiles + uint32_t(files.size());
    uint32_t dataBytes = 0;
    for (auto& b : blocks) dataBytes += uint32_t(b.size());
    std::vector<uint8_t> cab;
    PutStr(cab, "MSCF");
    Put32(cab, 0);
    Put32(cab, coffData + dataBytes);
    Put32(cab, 0);
    Put32(cab, coffFiles);
    Put32(cab, 0);
    cab.push_back(3);  // minor
    cab.push_back(1);  // major
    Put16(cab, 1);
    Put16(cab, uint32_t(items.size()));
    Put16(cab, 0);
    Put16(cab, 1234);
    Put16(cab, 0);
    Put32(cab, coffData);
    Put16(cab, uint32_t(nBlocks));
    Put16(cab, mszip ? 1 : 0);
    cab.insert(cab.end(), files.begin(), files.end());
    for (auto& b : blocks) cab.insert(cab.end(), b.begin(), b.end());
    return cab;
}

void TestCabInMemory() {
    std::string text;
    for (int i = 0; i < 2500; i++) text += "the same sentence again and again, number " + std::to_string(i % 40) + "\n";
    std::vector<CabItem> items = {{"a.txt", text}, {"dir\\b.bin", Pattern(17000, 5)}, {"empty", ""}, {"c.txt", text.substr(0, 9999)}};
    std::string err;
    for (int variant = 0; variant < 4; variant++) {
        bool mszip = variant & 1, sums = variant & 2;
        std::vector<uint8_t> cab = BuildCab(items, mszip, sums);
        auto src = OpenCab(MakeMemoryBlob(cab), 0, err);
        CHECK(src != nullptr);
        if (!src) {
            std::printf("  %s\n", err.c_str());
            continue;
        }
        CHECK(std::string(src->Kind()) == "cab");
        CHECK(src->Entries().size() == 4);
        for (const CabItem& it : items) {
            std::string path = it.name;
            for (char& c : path) if (c == '\\') c = '/';
            const Entry* e = Find(*src, path);
            CHECK(e != nullptr);
            if (!e) continue;
            CHECK(e->size == it.data.size());
            CHECK_EQ_STR(HashEntry(*src, *e), Sha256::Of(it.data.data(), it.data.size()).Hex());
        }
        if (variant == 3) {  // a damaged block is reported, not returned as good data
            std::vector<uint8_t> bad = cab;
            bad[bad.size() - 50] ^= 1;
            auto s2 = OpenCab(MakeMemoryBlob(bad), 0, err);
            CHECK(s2 != nullptr);
            bool failed = false;
            if (s2) for (const Entry& e : s2->Entries()) failed = failed || HashEntry(*s2, e).rfind("read failed", 0) == 0;
            CHECK(failed);
        }
    }
    // Inside an installer: found by its signature, behind bytes that hold a false "MSCF".
    std::vector<uint8_t> cab = BuildCab(items, true, true);
    std::string stub = "MZ" + Pattern(5000, 9) + "MSCF" + Pattern(40, 3) + Pattern(2000, 4);
    std::vector<uint8_t> exe(stub.begin(), stub.end());
    exe.insert(exe.end(), cab.begin(), cab.end());
    auto blob = MakeMemoryBlob(exe);
    int64_t at = FindCabinet(*blob, 1 << 20, err);
    CHECK(at == int64_t(stub.size()));
    auto src = OpenCab(blob, uint64_t(at), err);
    CHECK(src != nullptr && src->Entries().size() == 4);
    SniffOptions opts;
    auto sniffed = OpenSniffed(blob, opts, 0, err);
    CHECK(sniffed != nullptr && std::string(sniffed->Kind()) == "cab");
    CHECK(FindCabinet(*MakeMemoryBlob(std::vector<uint8_t>(5000, 'x')), 1 << 20, err) == -1);
    std::vector<uint8_t> noCab(exe.begin(), exe.begin() + stub.size());
    CHECK(OpenSniffed(MakeMemoryBlob(noCab), opts, 0, err) == nullptr);
}

// An HFS volume: 512-byte allocation blocks from byte 2048; the extents file in blocks 0-1, the catalog in 2-4, files at
// 10 and later. File "A" has five blocks (three extents in its record, the rest in the overflow tree) and a resource fork.
std::string BlockByte(uint32_t block, size_t n) {
    std::string s(n, 0);
    for (size_t i = 0; i < n; i++) s[i] = char((block * 7 + i) % 251);
    return s;
}

std::vector<uint8_t> CatKey(uint32_t parent, const std::string& name) {
    std::vector<uint8_t> k;
    k.push_back(uint8_t(6 + name.size()));
    k.push_back(0);
    Put32B(k, parent);
    k.push_back(uint8_t(name.size()));
    PutStr(k, name);
    if (k.size() & 1) k.push_back(0);
    return k;
}

std::vector<uint8_t> Node(uint8_t kind, uint8_t height, const std::vector<std::vector<uint8_t>>& recs, uint32_t fLink = 0) {
    std::vector<uint8_t> n(512, 0);
    n[3] = uint8_t(fLink);
    n[8] = kind;
    n[9] = height;
    n[10] = uint8_t(recs.size() >> 8);
    n[11] = uint8_t(recs.size());
    size_t at = 14;
    for (size_t i = 0; i <= recs.size(); i++) {
        n[512 - 2 * (i + 1)] = uint8_t(at >> 8);
        n[512 - 2 * (i + 1) + 1] = uint8_t(at);
        if (i < recs.size()) {
            std::memcpy(&n[at], recs[i].data(), recs[i].size());
            at += recs[i].size();
        }
    }
    return n;
}

std::vector<uint8_t> HeaderNode(uint32_t records, uint32_t lastLeaf = 1) {
    std::vector<std::vector<uint8_t>> recs(3);
    recs[0].assign(106, 0);
    recs[1].assign(128, 0);
    recs[2].assign(256, 0);
    std::vector<uint8_t>& h = recs[0];
    h[1] = 1;                       // depth
    h[5] = 1;                       // root node 1
    h[9] = uint8_t(records);
    h[13] = 1;                      // first leaf
    h[17] = uint8_t(lastLeaf);      // last leaf
    h[18] = 2;                      // node size 512
    return Node(1, 0, recs);
}

std::vector<uint8_t> Rec(std::vector<uint8_t> key, std::vector<uint8_t> data) {
    key.insert(key.end(), data.begin(), data.end());
    return key;
}

void SetExt(std::vector<uint8_t>& r, size_t at, std::initializer_list<std::pair<int, int>> exts) {
    for (auto& e : exts) {
        r[at] = uint8_t(e.first >> 8);
        r[at + 1] = uint8_t(e.first);
        r[at + 2] = uint8_t(e.second >> 8);
        r[at + 3] = uint8_t(e.second);
        at += 4;
    }
}

std::vector<uint8_t> FileRecord(uint32_t id, uint32_t dataLen, uint32_t rsrcLen) {
    std::vector<uint8_t> r(102, 0);
    r[0] = 2;
    for (int i = 0; i < 4; i++) {
        r[20 + i] = uint8_t(id >> (24 - 8 * i));
        r[26 + i] = uint8_t(dataLen >> (24 - 8 * i));
        r[36 + i] = uint8_t(rsrcLen >> (24 - 8 * i));
    }
    return r;
}

std::vector<uint8_t> BuildHfs(bool withMap) {
    std::vector<uint8_t> vol(27 * 512, 0);
    auto put = [&](size_t at, const std::vector<uint8_t>& b) { std::memcpy(&vol[at], b.data(), b.size()); };
    auto blockAt = [](uint32_t b) { return size_t(2048) + size_t(b) * 512; };
    // Catalog.
    std::vector<uint8_t> dirRoot(70, 0), dirSub(70, 0), thread(46, 0);
    dirRoot[0] = 1; dirRoot[9] = 2;
    dirSub[0] = 1; dirSub[9] = 17;
    thread[0] = 3;
    std::vector<uint8_t> fileA = FileRecord(16, 2300, 100);
    SetExt(fileA, 74, {{10, 1}, {12, 1}, {14, 1}});
    SetExt(fileA, 86, {{20, 1}});
    std::vector<uint8_t> fileXY = FileRecord(18, 10, 0);
    SetExt(fileXY, 74, {{21, 1}});
    std::vector<uint8_t> fileB = FileRecord(19, 0, 0);
    std::string bName = "B\x8A";
    std::vector<std::vector<uint8_t>> cat = {
        Rec(CatKey(1, "Vol"), dirRoot), Rec(CatKey(2, ""), thread),       Rec(CatKey(2, "A"), fileA),
        Rec(CatKey(2, bName), fileB),   Rec(CatKey(2, "Dir"), dirSub),    Rec(CatKey(17, "x/y"), fileXY),
    };
    // Two leaf nodes (a node holds 498 bytes of records): the catalog is the header node and two leaves.
    put(blockAt(2), HeaderNode(uint32_t(cat.size()), 2));
    put(blockAt(3), Node(0xFF, 1, {cat[0], cat[1], cat[2]}, 2));
    put(blockAt(4), Node(0xFF, 1, {cat[3], cat[4], cat[5]}));
    // Extents overflow: file 16, data fork, from block 3 of the fork.
    std::vector<uint8_t> ek = {7, 0, 0, 0, 0, 16, 0, 3};
    std::vector<uint8_t> ed(12, 0);
    SetExt(ed, 0, {{16, 1}, {18, 1}});
    put(blockAt(0), HeaderNode(1));
    put(blockAt(1), Node(0xFF, 1, {Rec(ek, ed)}));
    // File data.
    for (uint32_t b : {10u, 12u, 14u, 16u, 18u, 20u, 21u}) { std::string blk = BlockByte(b, 512); put(blockAt(b), std::vector<uint8_t>(blk.begin(), blk.end())); }
    // Master directory block.
    uint8_t* m = &vol[1024];
    m[0] = 'B'; m[1] = 'D';
    m[19] = 23;                      // allocation blocks
    m[22] = 2;                       // 512 bytes each
    m[29] = 4;                       // the first starts at 4 * 512
    m[132] = 4;                      // extents file: 1024 bytes
    m[134 + 1] = 0; m[134 + 3] = 2;  // blocks 0-1
    m[148] = 6;                      // catalog file: 1536 bytes
    m[150 + 1] = 2; m[150 + 3] = 3;  // blocks 2-4
    if (!withMap) return vol;
    std::vector<uint8_t> img(64 * 512, 0);
    img[0] = 'E'; img[1] = 'R'; img[2] = 2; img[3] = 0;
    auto entry = [&](size_t at, uint32_t start, uint32_t len, const char* name, const char* type) {
        img[at] = 'P'; img[at + 1] = 'M';
        img[at + 7] = 2;
        for (int i = 0; i < 4; i++) {
            img[at + 8 + i] = uint8_t(start >> (24 - 8 * i));
            img[at + 12 + i] = uint8_t(len >> (24 - 8 * i));
        }
        std::strcpy(reinterpret_cast<char*>(&img[at + 16]), name);
        std::strcpy(reinterpret_cast<char*>(&img[at + 48]), type);
    };
    entry(512, 1, 63, "Apple", "Apple_partition_map");
    entry(1024, 64, 27, "Untitled", "Apple_HFS");
    img.insert(img.end(), vol.begin(), vol.end());
    return img;
}

void TestHfsInMemory() {
    std::string err;
    for (int variant = 0; variant < 2; variant++) {
        auto blob = MakeMemoryBlob(BuildHfs(variant == 1));
        auto src = OpenHfs(blob, err);
        CHECK(src != nullptr);
        if (!src) {
            std::printf("  %s\n", err.c_str());
            continue;
        }
        CHECK(std::string(src->Kind()) == "hfs");
        const auto& es = src->Entries();
        CHECK(es.size() == 4);
        if (es.size() != 4) continue;
        CHECK(es[0].path == "A" && es[0].kind == EntryKind::Data && es[0].size == 2300);
        CHECK(es[1].path == "A" && es[1].kind == EntryKind::Rsrc && es[1].size == 100);
        CHECK(es[2].path == "B\xC3\xA4" && es[2].size == 0);
        CHECK(es[3].path == "Dir/x:y" && es[3].size == 10);
        std::string a;
        for (uint32_t b : {10u, 12u, 14u, 16u, 18u}) a += BlockByte(b, 512);
        a.resize(2300);
        CHECK_EQ_STR(HashEntry(*src, es[0]), Sha256::Of(a.data(), a.size()).Hex());
        std::string r = BlockByte(20, 100), x = BlockByte(21, 10);
        CHECK_EQ_STR(HashEntry(*src, es[1]), Sha256::Of(r.data(), r.size()).Hex());
        CHECK_EQ_STR(HashEntry(*src, es[2]), Sha256::Of("", 0).Hex());
        CHECK_EQ_STR(HashEntry(*src, es[3]), Sha256::Of(x.data(), x.size()).Hex());
        SniffOptions opts;
        auto sniffed = OpenSniffed(blob, opts, 0, err);
        CHECK(sniffed != nullptr && std::string(sniffed->Kind()) == "hfs");
    }
    // Garbage and a truncated volume are refused.
    CHECK(OpenHfs(MakeMemoryBlob(std::vector<uint8_t>(40000, 0)), err) == nullptr);
    std::vector<uint8_t> cut = BuildHfs(true);
    cut.resize(cut.size() - 3000);
    CHECK(OpenHfs(MakeMemoryBlob(cut), err) == nullptr);
}

void TestSniffDepth() {
    std::string err;
    SniffOptions opts;
    // Five zips inside each other: the fifth is the fifth open container and is refused.
    std::vector<uint8_t> inner = BuildMacBinary("leaf", "payload", "", true);
    std::vector<uint8_t> level = inner;
    for (int i = 0; i < 5; i++) {
        std::string s(level.begin(), level.end());
        level = BuildZip({{"inner.bin", s, false, false, false}}, false);
    }
    std::shared_ptr<Blob> blob = MakeMemoryBlob(level);
    std::shared_ptr<Source> src;
    int opened = 0;
    for (int depth = 0; depth < 8; depth++) {
        src = depth == 0 ? OpenSniffed(blob, opts, 0, err) : OpenEntrySniffed(src, src->Entries()[0], opts, depth, err);
        if (!src) break;
        opened++;
        if (std::string(src->Kind()) != "zip") break;
    }
    CHECK(opened == kMaxNesting);
    CHECK(!src && err.find("nested too deeply") != std::string::npos);
    // The same chain one level shorter opens down to the MacBinary leaf.
    level = inner;
    for (int i = 0; i < 3; i++) {
        std::string s(level.begin(), level.end());
        level = BuildZip({{"inner.bin", s, false, false, false}}, false);
    }
    src = OpenSniffed(MakeMemoryBlob(level), opts, 0, err);
    for (int depth = 1; src && std::string(src->Kind()) == "zip"; depth++) src = OpenEntrySniffed(src, src->Entries()[0], opts, depth, err);
    CHECK(src && std::string(src->Kind()) == "macbinary");
    if (!src) std::printf("  %s\n", err.c_str());
}

// ---- The US Mac disc: the toast inside its zip against the extracted tree

size_t CountSpillFiles(const std::string& dir) {
    size_t n = 0;
    std::error_code ec;
    for (auto it = std::filesystem::directory_iterator(dir, ec); !ec && it != std::filesystem::directory_iterator(); it.increment(ec)) {
        if (it->path().filename().string().rfind("tpw-spill-", 0) == 0) n++;
    }
    return n;
}

void TestToast(const std::string& root, const std::string& transfer, const std::string& scratch) {
    std::string zipPath = transfer + "/2 SimThemePark Mac US CD v2.0.1 Feral Aspyr/"
                          "SimThemePark_Mac_US-CD-Image_v2.0.1_2000_Feral-Aspyr_PPC-Classic.toast.zip";
    std::string treePath = root + "/extracted/disc-hfs";
    std::string err;
    if (!Exists(zipPath) || scratch.empty()) {
        Skip("toast: the US Mac disc's zip is absent or TPW_TEST_SCRATCH is not set");
        return;
    }
    auto tree = OpenFolder(treePath, err);
    if (!tree) {
        Skip("toast: " + treePath + " (" + err + ")");
        return;
    }
    SniffOptions opts;
    opts.spillDir = scratch;
    size_t spills = CountSpillFiles(scratch);
    {
        auto zip = OpenPathSniffed(zipPath, opts, err);
        CHECK(zip != nullptr && std::string(zip->Kind()) == "zip");
        if (!zip) return;
        const Entry* toast = Find(*zip, "Theme Park.toast");
        CHECK(toast != nullptr);
        if (!toast) return;
        auto hfs = OpenEntrySniffed(zip, *toast, opts, 1, err);
        CHECK(hfs != nullptr);
        if (!hfs) {
            std::printf("  %s\n", err.c_str());
            return;
        }
        CHECK(std::string(hfs->Kind()) == "hfs");
        CHECK(CountSpillFiles(scratch) == spills + 1);  // the 670 MB image was spilled to a file
        // The extracted tree is the expectation: every data fork and resource fork, the .finfo files ignored.
        std::map<std::pair<std::string, int>, const Entry*> want;
        size_t wantData = 0, wantRsrc = 0;
        for (const Entry& e : tree->Entries()) {
            if (e.path.size() > 6 && e.path.compare(e.path.size() - 6, 6, ".finfo") == 0) continue;
            want[{e.path, int(e.kind)}] = &e;
            (e.kind == EntryKind::Data ? wantData : wantRsrc)++;
        }
        size_t gotData = 0, gotRsrc = 0, checked = 0, bad = 0, notWanted = 0, finderFiles = 0;
        uint64_t bytes = 0;
        for (const Entry& e : hfs->Entries()) {
            (e.kind == EntryKind::Data ? gotData : gotRsrc)++;
            // The Finder's two desktop database files at the volume root are in the volume but not in the extracted tree.
            if (e.kind == EntryKind::Data && (e.path == "Desktop DB" || e.path == "Desktop DF")) {
                finderFiles++;
                gotData--;
                continue;
            }
            auto it = want.find({e.path, int(e.kind)});
            if (it == want.end()) {
                notWanted++;
                std::printf("  not in the extracted tree: %s\n", e.path.c_str());
                continue;
            }
            if (g_quick && checked >= 25) continue;
            checked++;
            bytes += e.size;
            if (e.size != it->second->size || HashEntry(*hfs, e) != HashEntry(*tree, *it->second)) {
                bad++;
                if (bad < 10) std::printf("  differs: %s\n", e.path.c_str());
            }
        }
        std::printf("toast: %zu data and %zu resource forks listed (the tree has %zu and %zu), %zu hashed (%llu bytes), "
                    "%zu differ, %zu not in the tree\n",
                    gotData, gotRsrc, wantData, wantRsrc, checked, (unsigned long long)bytes, bad, notWanted);
        CHECK(gotData == wantData && gotRsrc == wantRsrc);
        CHECK(bad == 0 && notWanted == 0);
        CHECK(wantData == 789 && wantRsrc == 61 && finderFiles == 2);
    }
    CHECK(CountSpillFiles(scratch) == spills);  // released with the source
}

// ---- The Gold disc: zip, raw ISO image, installer .exe, cabinet; against the files cabextract wrote

void TestGoldCab(const std::string& transfer, const std::string& expectDir) {
    std::string zipPath = transfer + "/4 SimThemePark PC Gold Edition Bonus Rides and Attractions CD/"
                          "SimThemePark_PC_US-Gold-Edition-Bonus-Rides-and-Attractions-CD-Image_2000-07_Bullfrog-EA_Win9x.zip";
    std::string err;
    std::string listPath = expectDir + "/gold-wads.sha256";
    auto cabDir = OpenFolder(expectDir + "/gold-cab", err);
    if (!Exists(zipPath) || !cabDir || !Exists(listPath)) {
        Skip("gold cabinet: the zip, gold-cab or gold-wads.sha256 is absent");
        return;
    }
    SniffOptions opts;
    auto zip = OpenPathSniffed(zipPath, opts, err);
    CHECK(zip != nullptr);
    if (!zip) return;
    const Entry* image = nullptr;
    for (const Entry& e : zip->Entries()) if (e.path.size() > 4 && e.path.compare(e.path.size() - 4, 4, ".bin") == 0) image = &e;
    CHECK(image != nullptr);
    if (!image) return;
    auto iso = OpenEntrySniffed(zip, *image, opts, 1, err);
    CHECK(iso != nullptr && std::string(iso->Kind()) == "iso9660");
    if (!iso) return;
    const Entry* exe = Find(*iso, "allitems.exe");
    CHECK(exe != nullptr);
    if (!exe) return;
    auto cab = OpenEntrySniffed(iso, *exe, opts, 2, err);
    CHECK(cab != nullptr);
    if (!cab) {
        std::printf("  %s\n", err.c_str());
        return;
    }
    CHECK(std::string(cab->Kind()) == "cab");
    CHECK(OpenEntrySniffed(iso, *exe, opts, kMaxNesting, err) == nullptr);
    // The list of the 35 wad hashes: "<sha256>  gold-cab/<name>".
    std::map<std::string, std::string> wads;
    std::ifstream list(listPath);
    for (std::string line; std::getline(list, line);) {
        size_t sp = line.find("  gold-cab/");
        if (sp == 64) wads[line.substr(sp + 11)] = line.substr(0, 64);
    }
    CHECK(wads.size() == 35);
    size_t bad = 0, wadsSeen = 0, missing = 0;
    uint64_t bytes = 0;
    CHECK(cab->Entries().size() == 37);
    for (const Entry& e : cab->Entries()) {
        const Entry* t = Find(*cabDir, e.path);
        if (!t) {
            missing++;
            std::printf("  not in gold-cab: %s\n", e.path.c_str());
            continue;
        }
        std::string h = HashEntry(*cab, e);
        bytes += e.size;
        if (e.size != t->size || h != HashEntry(*cabDir, *t)) bad++;
        auto w = wads.find(e.path);
        if (w != wads.end()) {
            wadsSeen++;
            if (h != w->second) bad++;
        }
    }
    std::printf("gold cabinet: %zu files (%llu bytes), %zu of the 35 wads on the list, %zu differ, %zu not in gold-cab\n",
                cab->Entries().size(), (unsigned long long)bytes, wadsSeen, bad, missing);
    CHECK(bad == 0 && missing == 0 && wadsSeen == 35);
}

}  // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IOLBF, 0);
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--quick") == 0) g_quick = true;
    }
    std::string root = Env("TPW_GDI_ROOT", "");
    std::string transfer = Env("TPW_TEST_TRANSFER", "");
    std::string expectDir = Env("TPW_TEST_EXPECT", "");
    TestSha256();
    TestZipInMemory();
    TestMacRoman();
    TestMacBinary();
    TestCabInMemory();
    TestHfsInMemory();
    TestSniffDepth();
    if (root.empty()) Skip("folder source: TPW_GDI_ROOT is not set"); else TestFolder(root);
    if (transfer.empty()) {
        Skip("zip and iso sources: TPW_TEST_TRANSFER is not set");
    } else {
        TestMacZip(transfer);
        if (root.empty()) Skip("toast: TPW_GDI_ROOT is not set"); else TestToast(root, transfer, Env("TPW_TEST_SCRATCH", ""));
        if (expectDir.empty()) {
            Skip("gold and PC images: TPW_TEST_EXPECT is not set");
        } else {
            TestGold(transfer, expectDir);
            TestPcImages(transfer, expectDir);
            TestGoldCab(transfer, expectDir);
        }
    }
    std::printf("assets tests: %d passed, %d failed, %d skipped\n", g_pass, g_fail, g_skip);
    return g_fail == 0 ? 0 : 1;
}
