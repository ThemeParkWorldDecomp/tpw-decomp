// host_pylon_codes.cpp: a park's action recording with the coaster pylon codes the Mac's recorder wrote as 0. The Mac's
// RecordAction(RecordedAction) wrote the high byte of the action word (0) for its three actions without operands: 0x37 (a normal
// pylon), 0x38 (a loop pylon) and 0x41 (the last brace undone). The port's recorder writes the code since 0.3.3 (arcode), and its
// replay skips such a 0 (a 0 whose halfword is above 0xff), so a park begun before 0.3.3 replayed without its loops (rebuilt from
// normal pylons) and its undos (every later brace edit then hit the wrong brace). With the port's fixes (loopcode,
// src/game/ActionRecorder.cpp) CActionRecorder::Serialise puts the codes back with PortRepairOldPylonCodes. This program builds
// recordings as the game records a coaster's edits, with the real codes, writes them as the Mac's recorder did (each code a 0) and
// checks the repair gives back the recording byte for byte:
//   - a pylon button press (the mode, 0x44, the ride, the type, 0x43), clicks and Backspaces with normal pylons;
//   - a loop (its press a 0x38, its two pylons, the second's click, 0x44, then the 0x37 of UIHandleEndLoop), with a press in mid
//     loop (one pylon to go);
//   - a Backspace that undoes a loop's first brace (0x41, then the 0x37 of RefreshButtons), a Backspace after a Backspace, and
//     one that undoes a loop's end (one pylon to go, with or without a press);
//   - DoDeleteMultiPylons (0x3f, then the type), and a mode change (a 0 with a halfword up to 0xff), which is left alone;
//   - a recording with the real codes (0.3.3 on) and one that has both (an old park edited again) are left as they are.
// Each recording is also cut at every length, at the end of a page with no access behind it on the systems that have mmap: the
// repair reads nothing past the end and changes only 0s. With a file's path as its argument (the unpacked block of an online park
// or the recording module of a save: an int, the recording's size, then the recording) it prints what the repair does to it.
// Run by ctest ("pylon_codes") in a build with the port's fixes; a build without them has no such call. Exits nonzero on a failure.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#if !defined(_WIN32)
#include <sys/mman.h>
#include <unistd.h>
#endif

#include "ActionRecorder.h"

namespace {

int gFailures = 0;
int gChecks = 0;

void Check(bool ok, const char* what, long value = -1) {
    ++gChecks;
    if (ok) return;
    ++gFailures;
    std::printf("FAIL %s (%ld)\n", what, value);
}

// The operand kinds CActionRecorder::Construct sets (the rest 0).
std::vector<uint32_t> Kinds() {
    std::vector<uint32_t> k(256, 0);
    const int pairs[][2] = {{0, 1},   {25, 1},  {5, 3},   {30, 1},  {35, 1},  {10, 1},  {15, 0},  {20, 0},  {40, 2},  {45, 2},  {50, 1},
                            {55, 8},  {56, 8},  {57, 4},  {58, 4},  {59, 5},  {60, 7},  {61, 1},  {62, 1},  {63, 6},  {64, 8},  {65, 8},
                            {66, 1},  {67, 1},  {68, 1},  {69, 6},  {70, 9},  {75, 9},  {80, 10}, {85, 10}, {231, 11}};
    for (const auto& p : pairs) k[p[0]] = static_cast<uint32_t>(p[1]);
    return k;
}

constexpr uint8_t kRide = 5;

// A recording as the port's recorder (0.3.3 on) writes it.
struct Rec {
    std::vector<uint8_t> b;
    void Half(uint8_t action, uint16_t v) { b.insert(b.end(), {action, uint8_t(v), uint8_t(v >> 8)}); }
    void Mode(uint8_t m) { Half(0, m); }
    void Code(uint8_t c) { b.push_back(c); }
    // ButtonPathStraight / ButtonPathSpecial (or the panel opening): the mode, 0x44, the ride, the type, the regeneration.
    void Press(uint8_t type) {
        Mode(0x15);
        Half(0x44, 1);
        Half(0x1e, kRide);
        Code(type);
        Half(0x43, kRide);
    }
    void Click(bool shift = false) {
        if (shift) {
            b.insert(b.end(), {0x2d, 3, 4, 0x10, 0x00});
        } else {
            b.insert(b.end(), {0x14, 3, 4});
        }
    }
    void Place(bool shift = false) {   // a pylon placed: the click, then CoasterAcceptLeftMouseClick's 0x44
        Click(shift);
        Half(0x44, 2);
    }
    void EndLoop(bool shift = false) {   // the loop's last pylon placed: UIHandleEndLoop's normal type, then the regeneration
        Place(shift);
        Code(0x37);
        Half(0x43, kRide);
    }
    void Backspace(bool loopStart = false) {   // DeleteLastTrackBrace; RefreshButtons's 0x37 when the brace began a loop
        Code(0x41);
        if (loopStart) Code(0x37);
        Half(0x43, kRide);
    }
    void DeleteMulti() {
        b.insert(b.end(), {0x3f, 1, 0, 3, 0});
        Code(0x37);
        Half(0x43, kRide);
    }
    void Other() {   // actions the repair reads past: a camera move (kind 5), a name (kind 9), a speed (kind 7)
        b.insert(b.end(), {59, 0, 0, 0x80, 0x3f, 0, 0, 0, 0x40, 9, 0});
        b.insert(b.end(), {70, 2, 'A', 0, 'B', 0});
        b.insert(b.end(), {60, 1, 0, 2, 0, 3, 0});
    }
};

// The same recording as the Mac's recorder wrote it: each code a 0.
std::vector<uint8_t> Old(const std::vector<uint8_t>& real, const std::vector<uint32_t>& kinds) {
    std::vector<uint8_t> out = real;
    size_t i = 0;
    while (i < out.size()) {
        const uint8_t a = out[i];
        const uint32_t k = kinds[a];
        size_t need = 0;
        if (a == 0x37 || a == 0x38 || a == 0x41) out[i] = 0;
        switch (k) {
            case 0: case 1: need = 2; break;
            case 2: case 6: need = 4; break;
            case 3: need = 3; break;
            case 4: case 7: need = 6; break;
            case 5: need = 10; break;
            case 9: need = 1 + 2 * size_t(out[i + 1]); break;
            case 10: need = 3 + 2 * size_t(out[i + 3]); break;
            case 11: need = 128; break;
            default: need = 0; break;
        }
        i += 1 + need;
    }
    return out;
}

size_t Zeros(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
    size_t n = 0;
    for (size_t i = 0; i < a.size(); ++i) n += a[i] != b[i];
    return n;
}

// The repair on a copy of the first size bytes at the end of a page with no access behind it (where there is mmap).
std::vector<uint8_t> Repaired(const std::vector<uint8_t>& in, size_t size, const std::vector<uint32_t>& kinds, uint32_t* changed) {
#if !defined(_WIN32)
    const size_t page = static_cast<size_t>(sysconf(_SC_PAGESIZE));
    const size_t span = (size + page - 1) / page * page + page;
    void* map = mmap(nullptr, span + page, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (map == MAP_FAILED) {
        Check(false, "mmap");
        return {};
    }
    uint8_t* base = static_cast<uint8_t*>(map);
    mprotect(base + span, page, PROT_NONE);
    uint8_t* data = base + span - size;
    if (size != 0) std::memcpy(data, in.data(), size);
    *changed = PortRepairOldPylonCodes(data, static_cast<uint32_t>(size), kinds.data());
    std::vector<uint8_t> out(data, data + size);
    munmap(map, span + page);
    return out;
#else
    std::vector<uint8_t> out(in.begin(), in.begin() + static_cast<long>(size));
    out.push_back(0xcc);
    *changed = PortRepairOldPylonCodes(out.data(), static_cast<uint32_t>(size), kinds.data());
    Check(out.back() == 0xcc, "the byte past the end", static_cast<long>(size));
    out.pop_back();
    return out;
#endif
}

void RoundTrip(const char* name, const Rec& rec, const std::vector<uint32_t>& kinds) {
    const std::vector<uint8_t> old = Old(rec.b, kinds);
    uint32_t changed = 0;
    const std::vector<uint8_t> back = Repaired(old, old.size(), kinds, &changed);
    const bool same = back == rec.b;
    if (!same) {
        for (size_t i = 0; i < back.size(); ++i) {
            if (back[i] != rec.b[i]) std::printf("  %s: at %zu 0x%02x, recorded 0x%02x\n", name, i, back[i], rec.b[i]);
        }
    }
    Check(same, name);
    Check(changed == Zeros(old, rec.b), name, static_cast<long>(changed));
    // the real codes are left as they are
    const std::vector<uint8_t> again = Repaired(rec.b, rec.b.size(), kinds, &changed);
    Check(again == rec.b && changed == 0, name, static_cast<long>(changed));
    // cut short anywhere: nothing read past the end, only 0s changed, and only to a pylon code
    for (size_t size = 0; size <= old.size(); ++size) {
        const std::vector<uint8_t> cut = Repaired(old, size, kinds, &changed);
        bool ok = cut.size() == size;
        for (size_t i = 0; ok && i < size; ++i) {
            ok = cut[i] == old[i] || (old[i] == 0 && (cut[i] == 0x37 || cut[i] == 0x38 || cut[i] == 0x41));
        }
        Check(ok, name, static_cast<long>(size));
    }
    std::printf("%-34s %zu bytes, %zu codes put back\n", name, rec.b.size(), Zeros(old, rec.b));
}

int File(const char* path, const std::vector<uint32_t>& kinds) {
    FILE* f = std::fopen(path, "rb");
    if (f == nullptr) {
        std::printf("cannot open %s\n", path);
        return 2;
    }
    std::vector<uint8_t> all;
    uint8_t chunk[65536];
    size_t got;
    while ((got = std::fread(chunk, 1, sizeof chunk, f)) != 0) all.insert(all.end(), chunk, chunk + got);
    std::fclose(f);
    if (all.size() < 8) return 2;
    uint32_t size = 0;
    std::memcpy(&size, all.data() + 4, 4);
    if (size > all.size() - 8) return 2;
    std::vector<uint8_t> rec(all.begin() + 8, all.begin() + 8 + size);
    uint32_t changed = 0;
    const std::vector<uint8_t> back = Repaired(rec, rec.size(), kinds, &changed);
    size_t n37 = 0, n38 = 0, n41 = 0;
    for (size_t i = 0; i < back.size(); ++i) {
        if (back[i] == rec[i]) continue;
        n37 += back[i] == 0x37;
        n38 += back[i] == 0x38;
        n41 += back[i] == 0x41;
        if (back[i] == 0x38) std::printf("  0x38 at %zu\n", i);
    }
    std::printf("%s: %u bytes, %u codes put back: %zu 0x37, %zu 0x38, %zu 0x41\n", path, size, changed, n37, n38, n41);
    return changed == n37 + n38 + n41 ? 0 : 1;
}

}  // namespace

int main(int argc, char** argv) {
    const std::vector<uint32_t> kinds = Kinds();
    if (argc > 1) return File(argv[1], kinds);

    {   // normal pylons, then a Backspace after three (as in Jurassic Park) and more pylons
        Rec r;
        r.Mode(0x15);
        r.Press(0x37);
        r.Place();
        r.Place();
        r.Place();
        r.Backspace();
        r.Place(true);
        r.Other();
        r.Backspace();
        r.Backspace();
        r.Place();
        RoundTrip("normal pylons and Backspaces", r, kinds);
    }
    {   // a loop (two pylons, the second ends it), then normal track
        Rec r;
        r.Press(0x37);
        r.Place();
        r.Press(0x38);
        r.Place();
        r.EndLoop();
        r.Place();
        r.Place();
        RoundTrip("a loop", r, kinds);
    }
    {   // the loop button pressed again in mid loop (one pylon to go), with shift clicks
        Rec r;
        r.Press(0x37);
        r.Place();
        r.Press(0x38);
        r.Place(true);
        r.Press(0x38);
        r.EndLoop(true);
        r.Other();
        r.Place();
        RoundTrip("a loop pressed again in mid loop", r, kinds);
    }
    {   // two loops in a row, the second undone, then the first's end undone and placed again (one pylon to go)
        Rec r;
        r.Press(0x37);
        r.Place();
        r.Press(0x38);
        r.Place();
        r.EndLoop();
        r.Press(0x38);
        r.Place();
        r.EndLoop();
        r.Backspace();       // the second loop's end: the loop goes on
        r.Backspace(true);   // its first brace: the normal type again
        r.Backspace();       // the first loop's end
        r.EndLoop();
        r.Place();
        RoundTrip("two loops, the second undone", r, kinds);
    }
    {   // a loop's end undone, the loop button pressed (one pylon to go), the end placed again
        Rec r;
        r.Press(0x38);
        r.Place();
        r.EndLoop();
        r.Backspace();
        r.Press(0x38);
        r.EndLoop(true);
        r.Place();
        r.Place();
        r.Place();
        r.Backspace();
        RoundTrip("a loop's end placed again", r, kinds);
    }
    {   // a loop undone before its end: its first brace (the 0x37 of RefreshButtons), then normal track and a Backspace
        Rec r;
        r.Press(0x38);
        r.Place();
        r.Backspace(true);
        r.Press(0x37);
        r.Place();
        r.Backspace();
        r.Place();
        RoundTrip("a loop undone before its end", r, kinds);
    }
    {   // pylons deleted together, then a normal press
        Rec r;
        r.Press(0x37);
        r.Place();
        r.Place();
        r.Place();
        r.DeleteMulti();
        r.Press(0x37);
        r.Place();
        RoundTrip("pylons deleted together", r, kinds);
    }
    {   // mode changes only: no 0 to put back
        Rec r;
        r.Mode(0x15);
        r.Mode(0);
        r.Mode(0x3f);
        r.Other();
        RoundTrip("mode changes", r, kinds);
    }
    {   // an old park edited again from 0.3.3: the real codes after the old 0s (Old gives 0s for both, so check by hand)
        Rec first;
        first.Press(0x37);
        first.Place();
        first.Press(0x38);
        first.Place();
        first.EndLoop();
        first.Backspace();
        Rec second;
        second.Press(0x38);
        second.Place();
        second.EndLoop();
        second.Backspace();
        std::vector<uint8_t> mixed = Old(first.b, kinds);
        mixed.insert(mixed.end(), second.b.begin(), second.b.end());
        std::vector<uint8_t> want = first.b;
        want.insert(want.end(), second.b.begin(), second.b.end());
        uint32_t changed = 0;
        const std::vector<uint8_t> back = Repaired(mixed, mixed.size(), kinds, &changed);
        Check(back == want, "an old park edited again");
        Check(changed == Zeros(mixed, want), "an old park edited again: count", static_cast<long>(changed));
        std::printf("%-34s %zu bytes, %u codes put back\n", "an old park edited again", want.size(), changed);
    }
    {   // a table with no halfword action 0 (kind 0 is 1 in the game): nothing is changed
        Rec r;
        r.Press(0x38);
        r.EndLoop();
        std::vector<uint32_t> other = kinds;
        other[0] = 0;
        const std::vector<uint8_t> old = Old(r.b, kinds);
        uint32_t changed = 1;
        const std::vector<uint8_t> back = Repaired(old, old.size(), other, &changed);
        Check(back == old && changed == 0, "action 0 not a halfword", static_cast<long>(changed));
    }

    std::printf("%d checks, %d failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
