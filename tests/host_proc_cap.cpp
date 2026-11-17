// host_proc_cap.cpp: the procedural textures of the map's cells when more of the procedural window's cells are in view than
// Prepare's list and the handler's ring hold. CHField::Prepare lists every cell of the window in view for its procedural
// texture, up to 2500 (kMaxOverlays), and the handler's ring of tiles holds 1500, 2000 or 2500 of them; GetTexturesUsed then
// takes one listed entry for each such cell, with no limit. The original's camera never shows that many, but the free camera
// tilted low (and Far zoom) shows the whole map: GetTexturesUsed read past the 2500 entries and the game faulted
// (GetTexturesUsed + 264, Land of Invention on a trackpad, 0.5.6 test build). With the port's fixes (proccap,
// src/engine/tb_hfield.cpp) only the cells nearest the camera are listed, as many as both hold, and the others take the
// mesh's own texture:
//   - a view the list and the ring hold: every window cell listed, in Prepare's order, as in the original (both builds);
//   - the whole map in view with a ring of 2500, 2000 and 1500 tiles: as many cells listed as the ring holds, the nearest
//     ones, hidden cells never; GetTexturesUsed takes the listed entries in order and gives every other cell the mesh's;
//   - more window cells in GetTexturesUsed than Prepare saw: never past the list's end.
// The program runs the real CHField::Prepare, GetTexturesUsed and QSort_textures on a map of 128 by 128 cells; the gamut,
// the texture cache and the handler are stand-ins. Made in both builds: without the port's fixes it checks only the view the
// list holds (the original reads past the list in the others, as the game did). Exits nonzero on a failure.
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "m3d2.h"
#include "platform/arena.h"
#include "tb_hfield.h"
#include "tb_loadmanager.h"
#include "tb_matrix.h"
#include "tb_proctexture.h"
#include "tb_resource_system.h"
#include "tb_resource_texture.h"
#include "tb_texturecache.h"
#include "tb_textureentry.h"

namespace {

int gFailures = 0;
int gChecks = 0;

void Check(bool ok, const char* what, long value = -1) {
    ++gChecks;
    if (ok) return;
    ++gFailures;
    std::printf("FAIL %s (%ld)\n", what, value);
}

constexpr int32_t kSide = 128;
constexpr int32_t kCells = kSide * kSide;

// The objects the unit reaches, as raw storage where the program links no constructor.
alignas(CHField) unsigned char gFieldStorage[sizeof(CHField)];
CHField& Field() { return *reinterpret_cast<CHField*>(gFieldStorage); }
alignas(CGamut) unsigned char gGamutStorage[sizeof(CGamut)];
alignas(CTextureCache) unsigned char gCacheStorage[sizeof(CTextureCache)];
alignas(CProcTextureHandler) unsigned char gHandlerStorage[sizeof(CProcTextureHandler)];
CProcTextureHandler& Handler() { return *reinterpret_cast<CProcTextureHandler*>(gHandlerStorage); }
alignas(CTextureEntry) unsigned char gMeshEntryStorage[sizeof(CTextureEntry)];
CTextureEntry& MeshEntry() { return *reinterpret_cast<CTextureEntry*>(gMeshEntryStorage); }
// The entry PrepareList gives the n-th listed cell.
alignas(CTextureEntry) unsigned char gProcEntryStorage[CHField::kMaxOverlays][sizeof(CTextureEntry)];
CTextureEntry* ProcEntry(int32_t n) { return reinterpret_cast<CTextureEntry*>(gProcEntryStorage[n]); }
char gPages[3];   // a page is only an address: 1 the mesh's, 2 the procedural tiles'
CMatrix gView;

// The rows of cells the gamut answers, and what the stand-ins saw.
SGamutQuads gQuads;
SGamutQuads gVertices;
int32_t gPrepareListCalls = 0;
int32_t gPrepareListCount = 0;
std::vector<uint32_t> gTiles;   // the tiles PrepareList was given, in order
int32_t gMeshPrepares = 0;

// The map's cells (from the arena: the record's pointer to them is a 32-bit field) and its texture record.
sHField* gRecord = nullptr;
sHFieldQuad* gQuadBlock = nullptr;
sTexture* gTextureRecord = nullptr;
CsTexture* gTextureObject = nullptr;
CTextureEntry** gTextureEntries = nullptr;

}  // namespace

// ---- the engine objects the unit reads
CSystem gei_System;
CTextureSettings gei_TextureSettings;
CGamut* gei_Gamut = reinterpret_cast<CGamut*>(gGamutStorage);
CTextureCache* gei_TCache = reinterpret_cast<CTextureCache*>(gCacheStorage);
CMatrix* gei_ViewMatrix = &gView;
CSystem::CSystem() {}
CTextureSettings::CTextureSettings() {}

// ---- the stand-ins
SGamutQuads* CGamut::GetQuads() { return &gQuads; }
SGamutQuads* CGamut::GetVertices() { return &gVertices; }
void CTextureEntry::Lock() {}
void CTextureCache::Prepare(CTextureEntry*, float) { ++gMeshPrepares; }

// The handler turns each tile in place into its texture entry, the n-th listed cell's being ProcEntry(n).
int32_t CProcTextureHandler::PrepareList(PrepareEntry* entries, sTileOverlay*, int32_t count) {
    ++gPrepareListCalls;
    gPrepareListCount = count;
    gTiles.clear();
    for (int32_t i = 0; i < count; i++) {
        gTiles.push_back(entries[i].mTile);
        entries[i].mTexture = ProcEntry(i);
    }
    return 1;
}

namespace {

void MakeWorld() {
    gRecord = static_cast<sHField*>(tpw::ArenaAllocZeroed(sizeof(sHField)));
    gQuadBlock = static_cast<sHFieldQuad*>(tpw::ArenaAllocZeroed(sizeof(sHFieldQuad) * kCells));
    gTextureRecord = static_cast<sTexture*>(tpw::ArenaAllocZeroed(sizeof(sTexture)));
    gTextureObject = static_cast<CsTexture*>(tpw::ArenaAllocZeroed(sizeof(CsTexture)));
    gTextureEntries = static_cast<CTextureEntry**>(tpw::ArenaAllocZeroed(sizeof(CTextureEntry*) * 2));
    gRecord->mF18[0] = kSide;
    gRecord->mQuads = gQuadBlock;
    gTextureEntries[0] = &MeshEntry();
    gTextureObject->mEntries = gTextureEntries;
    gTextureObject->mEntryCount = 1;
    gTextureRecord->mTexture = gTextureObject;
    std::memset(gMeshEntryStorage, 0, sizeof gMeshEntryStorage);
    MeshEntry().mFlags = CTextureEntry::kOnPage;
    MeshEntry().mTexture = reinterpret_cast<CTextureBase*>(&gPages[1]);
    for (int32_t n = 0; n < CHField::kMaxOverlays; n++) {
        std::memset(gProcEntryStorage[n], 0, sizeof gProcEntryStorage[n]);
        ProcEntry(n)->mFlags = CTextureEntry::kOnPage;
        ProcEntry(n)->mTexture = reinterpret_cast<CTextureBase*>(&gPages[2]);
    }
}

// A case: the cells in view are rows first..end - 1 of columns low..high - 1, the procedural window is columns x0..x1 by rows
// z0..z1, the ring holds `ring` tiles, the camera stands over cell (cameraX, cameraZ), and every `hiddenEvery`-th cell (0:
// none) is hidden.
struct Case {
    int32_t first, end, low, high;
    int32_t x0, z0, x1, z1;
    int32_t ring;
    float cameraX, cameraZ;
    int32_t hiddenEvery;
};

bool Hidden(const Case& c, int32_t column, int32_t row) {
    return c.hiddenEvery != 0 && (row * kSide + column) % c.hiddenEvery == 0;
}

bool InWindow(const Case& c, int32_t column, int32_t row) {
    return column >= c.x0 && column <= c.x1 && row >= c.z0 && row <= c.z1;
}

void SetRows(SGamutQuads& rows, const Case& c) {
    std::memset(&rows, 0, sizeof rows);
    rows.mFirstRow = c.first;
    rows.mEndRow = c.end;
    for (int32_t z = 0; z < 128; z++) {
        rows.mLow[z] = static_cast<int8_t>(c.low);
        rows.mHigh[z] = static_cast<int8_t>(c.high);
    }
}

// Builds the field for a case. Everything after the list (the colours of the grid points) is a pattern that is no
// texture entry: reading an entry there faults.
void Begin(const Case& c) {
    CHField& field = Field();
    std::memset(static_cast<void*>(&field), 0xa5, sizeof field);
    field.mF00 = gRecord;
    field.mF04 = gTextureRecord;
    field.mInvCellX = 1.0f;
    field.mInvCellZ = 1.0f;
    field.mProcTextures = &Handler();
    field.mF234 = c.x0;
    field.mF238 = c.z0;
    field.mF23C = c.x1;
    field.mF240 = c.z1;
    std::memset(gHandlerStorage, 0, sizeof gHandlerStorage);
    Handler().mCount = c.ring;
    for (int32_t row = 0; row < kSide; row++) {
        for (int32_t column = 0; column < kSide; column++) {
            sHFieldQuad& cell = gQuadBlock[row * kSide + column];
            cell.mF00 = Hidden(c, column, row) ? 1 : 0;
            cell.mF02 = 0;
        }
    }
    SetRows(gQuads, c);
    SetRows(gVertices, c);
    std::memset(&gView, 0, sizeof gView);
    gView.m[3][0] = c.cameraX;
    gView.m[3][2] = c.cameraZ;
    gei_System.mFlags = CSystem::kViewChanged;
    gei_TextureSettings.mFlags = CTextureSettings::kProcTexturesOn;
    gei_TextureSettings.mFormat = 4;
    gPrepareListCalls = 0;
    gPrepareListCount = 0;
    gTiles.clear();
    gMeshPrepares = 0;
}

int32_t Distance(const Case& c, int32_t column, int32_t row) {
    const float dx = static_cast<float>(column) + 0.5f - c.cameraX;
    const float dz = static_cast<float>(row) + 0.5f - c.cameraZ;
    return static_cast<int32_t>(std::sqrt(dx * dx + dz * dz));
}

uint32_t Tile(const Case& c, int32_t column, int32_t row) {
    return static_cast<uint32_t>((row - c.z0) * 0x100 + (column - c.x0));
}

// The window's cells in view that are not hidden.
int32_t WindowCells(const Case& c) {
    int32_t cells = 0;
    for (int32_t row = c.first; row < c.end; row++)
        for (int32_t column = c.low; column < c.high; column++)
            if (!Hidden(c, column, row) && InWindow(c, column, row)) ++cells;
    return cells;
}

// Runs Prepare and GetTexturesUsed and checks them against `expectedListed` cells listed (all the window's cells in view
// when they fit, else the nearest that many).
void Run(const char* name, const Case& c, int32_t expectedListed) {
    Begin(c);
    CHField& field = Field();
    Check(field.Prepare() == 1, name);
    Check(gPrepareListCalls == 1, name, gPrepareListCalls);
    Check(gPrepareListCount == expectedListed, name, gPrepareListCount);
    Check(field.mF1296C == expectedListed, name, field.mF1296C);

    // Which cells the list holds, and that it holds them in Prepare's order (row by row).
    std::vector<uint8_t> listed(kCells, 0);
    int32_t windowCells = 0;
    int32_t maxListed = -1;
    int32_t minLeft = 1 << 30;
    {
        size_t next = 0;
        bool ordered = true;
        for (int32_t row = c.first; row < c.end; row++) {
            for (int32_t column = c.low; column < c.high; column++) {
                if (Hidden(c, column, row) || !InWindow(c, column, row)) continue;
                ++windowCells;
                if (next < gTiles.size() && gTiles[next] == Tile(c, column, row)) {
                    listed[row * kSide + column] = 1;
                    ++next;
                    if (Distance(c, column, row) > maxListed) maxListed = Distance(c, column, row);
                } else if (Distance(c, column, row) < minLeft) {
                    minLeft = Distance(c, column, row);
                }
            }
        }
        ordered = next == gTiles.size();
        Check(ordered, name, static_cast<long>(next));
    }
    if (expectedListed < windowCells) {
        // Only the nearest: no cell left out is nearer than a listed one.
        Check(maxListed <= minLeft, name, maxListed - minLeft);
    }
    // Every cell in view that is not listed asked the texture cache for the mesh's picture.
    int32_t visible = 0;
    for (int32_t row = c.first; row < c.end; row++)
        for (int32_t column = c.low; column < c.high; column++)
            if (!Hidden(c, column, row)) ++visible;
    Check(gMeshPrepares == visible - expectedListed, name, gMeshPrepares);

    // GetTexturesUsed: the n-th listed cell takes the n-th entry, every other cell the mesh's.
    static void* pages[1024];
    static CTextureEntry* entries[kCells];
    int32_t count = 0;
    field.GetTexturesUsed(pages, count, entries);
    int32_t n = 0;
    int32_t at = 0;
    bool right = true;
    for (int32_t row = c.first; row < c.end; row++) {
        for (int32_t column = c.low; column < c.high; column++) {
            if (Hidden(c, column, row)) continue;
            CTextureEntry* expected = listed[row * kSide + column] != 0 ? ProcEntry(n++) : &MeshEntry();
            if (entries[at++] != expected) right = false;
        }
    }
    Check(right, name);
    Check(n == expectedListed, name, n);
    Check(count == (expectedListed > 0 && expectedListed < visible ? 2 : 1), name, count);
}

}  // namespace

int main() {
    MakeWorld();

    // A view the list and the ring hold: a window of 40 by 40 cells, all in view, every one listed in order.
    Run("window held", Case{0, 128, 0, 127, 10, 10, 49, 49, 2000, 30.0f, 30.0f, 0}, 40 * 40);
    // With hidden cells: they are neither listed nor drawn.
    const Case hidden{0, 128, 0, 127, 10, 10, 49, 49, 2000, 30.0f, 30.0f, 7};
    Run("window held, hidden cells", hidden, WindowCells(hidden));
    // Exactly as many as the ring holds: still all of them.
    const Case exact{0, 128, 0, 127, 0, 0, 99, 14, 1500, 64.0f, 64.0f, 0};   // 15 rows of 100
    Run("window held, as many as the ring", exact, 1500);

#ifdef TPW_TEST_PORT_FIX
    // The whole map in view (128 rows of 127 cells), the window the whole map: as many as the ring holds, the nearest.
    Run("whole map, ring 2500", Case{0, 128, 0, 127, 0, 0, 127, 127, 2500, 64.0f, 64.0f, 0}, 2500);
    Run("whole map, ring 2000", Case{0, 128, 0, 127, 0, 0, 127, 127, 2000, 20.0f, 100.0f, 0}, 2000);
    Run("whole map, ring 1500", Case{0, 128, 0, 127, 0, 0, 127, 127, 1500, 127.0f, 0.0f, 0}, 1500);
    // The camera off the map's corner, as the free camera can stand.
    Run("whole map, camera outside", Case{0, 128, 0, 127, 0, 0, 127, 127, 2000, -40.0f, -40.0f, 0}, 2000);
    // Hidden cells are not counted and never listed.
    Run("whole map, hidden cells", Case{0, 128, 0, 127, 0, 0, 127, 127, 2000, 64.0f, 64.0f, 3}, 2000);
    // A ring of no count (not set up): the list's own 2500.
    Run("whole map, no ring count", Case{0, 128, 0, 127, 0, 0, 127, 127, 0, 64.0f, 64.0f, 0}, 2500);

    // More window cells in view in GetTexturesUsed than Prepare saw: never past the list's end, the rest take the mesh's. The
    // entries after the ones listed are what earlier frames left there (valid entries in the game).
    {
        const Case held{0, 128, 0, 127, 0, 0, 127, 19, 2500, 64.0f, 10.0f, 0};   // 20 rows of 127: 2540, more than 2500
        const Case small{0, 19, 0, 127, 0, 0, 127, 127, 2500, 64.0f, 10.0f, 0};  // 19 rows of 127: 2413 cells, all listed
        Begin(small);
        CHField& field = Field();
        field.Prepare();
        Check(field.mF1296C == 19 * 127, "more in view later", field.mF1296C);
        for (int32_t i = field.mF1296C; i < CHField::kMaxOverlays; i++) field.mPrepareEntries[i].mTexture = ProcEntry(i);
        SetRows(*field.QuadRows(), held);
        static void* pages[1024];
        static CTextureEntry* entries[kCells];
        int32_t count = 0;
        field.GetTexturesUsed(pages, count, entries);
        int32_t procedural = 0;
        int32_t at = 0;
        bool right = true;
        for (int32_t row = 0; row < 128; row++) {
            for (int32_t column = 0; column < 127; column++) {
                CTextureEntry* entry = entries[at++];
                if (entry == &MeshEntry()) continue;
                if (procedural >= CHField::kMaxOverlays || entry != ProcEntry(procedural)) right = false;
                ++procedural;
            }
        }
        Check(right, "more in view later");
        Check(procedural == CHField::kMaxOverlays, "more in view later", procedural);
    }
#else
    std::printf("without the port's fixes only the view the list holds is checked: the original reads past it otherwise\n");
#endif

    std::printf("%d checks, %d failed\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
