// TPW_MOD_HOOK tpi_worlds (include/TpiDoorway.h): Theme Park Inc's doorway floors against the base under them. The
// texture names and the marking on a made-up mesh, then the umbrella shop's entrance floor (y 0.03 over its base at y 0;
// also 0.002, the closest of the mod's floors, and 0.08) put through the port's depth math as the
// camera turns: the projection and CMatrixPowerPC::Project's perspective depth curve in single precision, with the
// constants the game uses at zooms 130, 250, 400 and 500 (gei_Display.mZ68..mZ78 and the projection, read from the game),
// each floor sample compared with the base under it in a 24-bit depth buffer. Projected as the Mac does, the base wins
// the test on part of the floor beyond the original's zoom limit; moved nearer by tpw::TpiDoorwayNearer, the floor wins
// every sample. No game data.
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
#include "TpiDoorway.h"

namespace tpw {
uint8_t* gArenaBase = nullptr;   // the test's own block stands in for the arena (Ptr32 offsets from it)
}

namespace {
int checks = 0, failures = 0;
void Check(bool ok, const char* what) {
    ++checks;
    if (!ok) { ++failures; std::printf("FAIL: %s\n", what); }
}

void Names() {
    char name[0x14];
    std::memset(name, 0, sizeof name);
    std::memcpy(name, "doorway_alpha.tga", 17);
    Check(tpw::TpiDoorwayTexture(name), "doorway_alpha.tga is a doorway floor");
    std::memcpy(name, "DOORWAY_ALPHA.TGA", 17);
    Check(tpw::TpiDoorwayTexture(name), "the name in capitals is one too");
    std::memcpy(name, "doorway.tga\0\0\0\0\0\0", 17);
    Check(!tpw::TpiDoorwayTexture(name), "doorway.tga (the door frame) is not");
    std::memcpy(name, "doorway_alpha.tgaxyz", 0x14);   // 0x14 bytes with no zero: read no further
    Check(!tpw::TpiDoorwayTexture(name), "a longer name is not");
    Check(!tpw::TpiDoorwayTexture(nullptr), "no name is not");
}

void Marking() {
    // A mesh's records laid out in one block: the header, three textures, their infos and names, five face sets.
    std::vector<uint64_t> storage(512, 0);
    uint8_t* const block = reinterpret_cast<uint8_t*>(storage.data());
    tpw::gArenaBase = block;
    uint32_t at = 16;   // offset 0 is the null pointer
    const auto take = [&](uint32_t size) { const uint32_t offset = at; at += (size + 15) & ~15u; return offset; };
    const uint32_t headerAt = take(sizeof(tag_sM3D2Header)), texturesAt = take(3 * sizeof(sTexture));
    const uint32_t infosAt = take(3 * sizeof(sTextureInfo)), setsAt = take(5 * sizeof(sFaceSet));
    const char* const names[3] = {"ww_g03a.tga", "Doorway_Alpha.tga", "doorway.tga"};
    auto* header = reinterpret_cast<tag_sM3D2Header*>(block + headerAt);
    auto* infos = reinterpret_cast<sTextureInfo*>(block + infosAt);
    auto* sets = reinterpret_cast<sFaceSet*>(block + setsAt);
    for (int i = 0; i < 3; ++i) {
        const uint32_t nameAt = take(0x14);
        std::memcpy(block + nameAt, names[i], std::strlen(names[i]));
        infos[i].mName.mOffset = nameAt;
    }
    header->mTextureCount = 3;
    header->mFaceSetCount = 5;
    header->mTextures.mOffset = texturesAt;
    header->mTextureInfos.mOffset = infosAt;
    header->mFaceSets.mOffset = setsAt;
    const int textureOf[5] = {0, 1, -1, 2, 1};   // -1: no texture
    for (int i = 0; i < 5; ++i) {
        sets[i].mTexture.mOffset = textureOf[i] < 0 ? 0 : texturesAt + textureOf[i] * static_cast<uint32_t>(sizeof(sTexture));
        sets[i].mF0C[0] = static_cast<uint8_t>(i & 1);   // the first flag byte is the mesh's own; left as it is
    }
    Check(tpw::TpiMarkDoorways(header) == 2, "the two doorway_alpha.tga face sets are marked");
    const uint8_t marks[5] = {0, 1, 0, 0, 1};
    for (int i = 0; i < 5; ++i) {
        Check(sets[i].mF0C[3] == marks[i], "only the doorway floors carry the mark");
        Check(sets[i].mF0C[0] == (i & 1) && sets[i].mF0C[1] == 0 && sets[i].mF0C[2] == 0, "the other flag bytes stay");
    }
    Check(tpw::TpiMarkDoorways(header) == 2 && sets[1].mF0C[3] == tpw::kTpiDoorwayMark, "marking again changes nothing");
    header->mTextureInfos.mOffset = 0;
    Check(tpw::TpiMarkDoorways(header) == 0, "a mesh without texture infos marks nothing");
    tpw::gArenaBase = nullptr;
}

// The game's depth constants at one zoom (tp zinfo in Polar Zone, the free camera at pitch 0.9): the projection's z row
// (clip z = p22 * view z + p32, w = p23 * view z) and CDisplay::ZSetup's outputs.
struct Zoom {
    float zoom, p22, p32, p23, split, nearW, nearZW, farW, farZ;
};
const Zoom kZooms[] = {
    {130, 0.707453f, -0.0707453f, 0.707107f, 0.541695f, 0.1f, 0.184606f, -5.95167f, 6.95167f},
    {250, 0.707315f, -0.0707315f, 0.707107f, 0.949966f, 0.1f, 0.105267f, -10.5902f, 11.5902f},
    {400, 0.707244f, -0.0707244f, 0.707107f, 1.47919f, 0.1f, 0.0676046f, -16.6032f, 17.6032f},
    {500, 0.707219f, -0.0707219f, 0.707107f, 1.82322f, 0.1f, 0.0548482f, -20.5119f, 21.5119f},
};

// A projected vertex as the render list holds it: x, y, clip z, w first (the rest of SFVF_TLCTEX is not used here).
struct Vertex {
    float v[4];
};

// CMatrixPowerPC::Project, perspective type (src/engine/MatrixPowerPC.cpp), on one vertex.
void Project(Vertex& out, const Zoom& c) {
    float* const v = out.v;
    v[1] = -v[1];
    const float z = v[2];
    float r;
    if (z < c.split) {
        r = std::fmaf(c.nearW, v[3], c.nearZW * (z * v[3]));
    } else {
        r = std::fmaf(c.farW, v[3], z * c.farZ);
    }
    v[2] = r;
    const float w = v[3];
    const float q = v[2] / w;
    v[2] = static_cast<float>(static_cast<double>(w) * std::fma(2.0, static_cast<double>(q), -1.0));
}

struct Camera {
    double eye[3], right[3], up[3], forward[3];
};
// The free camera looking down at `pitch` at the shop's entrance from `zoom` away, turned by `yaw`.
Camera Look(double zoom, double yaw, double pitch) {
    const double poi[3] = {535.0, 0.0, 186.0};
    Camera c{};
    const double f[3] = {std::sin(yaw) * std::cos(pitch), -std::sin(pitch), std::cos(yaw) * std::cos(pitch)};
    const double r[3] = {std::cos(yaw), 0.0, -std::sin(yaw)};
    for (int i = 0; i < 3; ++i) {
        c.forward[i] = f[i];
        c.right[i] = r[i];
        c.eye[i] = poi[i] - f[i] * zoom;
    }
    c.up[0] = f[1] * r[2] - f[2] * r[1];
    c.up[1] = f[2] * r[0] - f[0] * r[2];
    c.up[2] = f[0] * r[1] - f[1] * r[0];
    return c;
}

// A point of the shop (its own coordinates; the shop stands at 520, 0, 171) projected: screen x, y and window depth.
struct Screen {
    double x, y, depth;
};
Screen ToScreen(const double p[3], const Camera& cam, const Zoom& c, bool nearer) {
    const double world[3] = {p[0] + 520.0, p[1], p[2] + 171.0};
    double rel[3];
    for (int i = 0; i < 3; ++i) rel[i] = world[i] - cam.eye[i];
    const auto dot = [&](const double a[3]) { return static_cast<float>(rel[0] * a[0] + rel[1] * a[1] + rel[2] * a[2]); };
    const float xv = dot(cam.right), yv = dot(cam.up), zv = dot(cam.forward);
    Vertex vertex{{0.707107f * xv, 0.707107f * yv, c.p22 * zv + c.p32, c.p23 * zv}};
    Project(vertex, c);
    if (nearer) tpw::TpiDoorwayNearer(reinterpret_cast<uint8_t*>(&vertex), sizeof(Vertex), 1, c.farZ);
    const float* v = vertex.v;
    const float ndc = v[2] / v[3];
    return {static_cast<double>(v[0] / v[3]), static_cast<double>(v[1] / v[3]), (static_cast<double>(ndc) + 1.0) / 2.0};
}

struct Turn {
    long samples = 0, lost = 0;
    double margin = 1e9;   // the fewest depth steps the floor was nearer by (negative: farther)
};

// The umbrella shop's entrance floor (face sets 54 and 55 of node 1, x 11.02..18.98, z 9.41..15.70) at `height` over its
// base (node 0's ww_g03a.tga quads, x 10..20, z 10..20, and the path quad in front, z 1.21..10, all at y 0), sampled on
// a grid in each floor triangle on screen at each step of a full turn and three pitches.
Turn RunTurn(const Zoom& c, double height, bool nearer) {
    const double floor[2][3][3] = {{{11.02, height, 9.41}, {18.98, height, 15.70}, {18.98, height, 9.41}},
                                   {{18.98, height, 15.70}, {11.02, height, 9.41}, {11.02, height, 15.70}}};
    const double base[4][3][3] = {{{20, 0, 20}, {10, 0, 10}, {10, 0, 20}},
                                  {{10, 0, 10}, {20, 0, 20}, {20, 0, 10}},
                                  {{10, 0, 1.21}, {20, 0, 10}, {20, 0, 1.21}},
                                  {{20, 0, 10}, {10, 0, 1.21}, {10, 0, 10}}};
    const int n = 20;
    Turn turn;
    for (double pitch : {0.6, 0.9, 1.2}) {
        for (int step = 0; step < 63; ++step) {
            const Camera cam = Look(c.zoom, -3.1 + 0.1 * step, pitch);
            Screen fl[2][3], bs[4][3];
            for (int t = 0; t < 2; ++t)
                for (int k = 0; k < 3; ++k) fl[t][k] = ToScreen(floor[t][k], cam, c, nearer);
            for (int t = 0; t < 4; ++t)
                for (int k = 0; k < 3; ++k) bs[t][k] = ToScreen(base[t][k], cam, c, false);
            for (int t = 0; t < 2; ++t) {
                for (int i = 1; i < n; ++i) {
                    for (int j = 1; j < n - i; ++j) {
                        const double l[3] = {double(i) / n, double(j) / n, 1.0 - double(i) / n - double(j) / n};
                        double sx = 0, sy = 0, zf = 0;
                        for (int k = 0; k < 3; ++k) {
                            sx += l[k] * fl[t][k].x;
                            sy += l[k] * fl[t][k].y;
                            zf += l[k] * fl[t][k].depth;
                        }
                        for (const auto& b : bs) {
                            const double d = (b[1].y - b[2].y) * (b[0].x - b[2].x) + (b[2].x - b[1].x) * (b[0].y - b[2].y);
                            const double l1 = ((b[1].y - b[2].y) * (sx - b[2].x) + (b[2].x - b[1].x) * (sy - b[2].y)) / d;
                            const double l2 = ((b[2].y - b[0].y) * (sx - b[2].x) + (b[0].x - b[2].x) * (sy - b[2].y)) / d;
                            const double l3 = 1.0 - l1 - l2;
                            if (l1 < -1e-9 || l2 < -1e-9 || l3 < -1e-9) continue;
                            const double zb = l1 * b[0].depth + l2 * b[1].depth + l3 * b[2].depth;
                            const double qf = std::floor(zf * 16777216.0), qb = std::floor(zb * 16777216.0);
                            ++turn.samples;
                            if (qf >= qb) ++turn.lost;
                            if (qb - qf < turn.margin) turn.margin = qb - qf;
                            break;
                        }
                    }
                }
            }
        }
    }
    return turn;
}

void Depth() {
    char what[160];
    for (double height : {0.03, 0.002, 0.08}) {
        for (const Zoom& c : kZooms) {
            const Turn mac = RunTurn(c, height, false);
            const Turn port = RunTurn(c, height, true);
            std::printf("floor %.3f zoom %3.0f: as projected %ld of %ld samples lost (margin %.0f steps), moved nearer "
                        "%ld lost (margin %.0f)\n",
                        height, c.zoom, mac.lost, mac.samples, mac.margin, port.lost, port.margin);
            std::snprintf(what, sizeof what, "floor %.3f zoom %.0f: every sample is compared", height, c.zoom);
            Check(mac.samples > 10000 && port.samples == mac.samples, what);
            std::snprintf(what, sizeof what, "floor %.3f zoom %.0f: moved nearer, the floor wins every sample by 8 steps",
                          height, c.zoom);
            Check(port.lost == 0 && port.margin >= 8, what);
            if (height == 0.03 && c.zoom == 130) {
                Check(mac.lost == 0, "the umbrella shop's floor wins at the original's zoom limit as projected");
            }
            if (height == 0.03 && c.zoom >= 250) {
                std::snprintf(what, sizeof what, "zoom %.0f: as projected the base wins part of the umbrella shop's floor",
                              c.zoom);
                Check(mac.lost > 0, what);
            }
        }
    }
}

void Nearer() {
    // Only the depth moves, by the same share of w for any vertex; the stride is honoured.
    float data[2][6] = {{1.5f, -2.0f, 0.25f, 4.0f, 7.0f, 8.0f}, {-3.0f, 1.0f, -0.5f, 2.0f, 9.0f, 10.0f}};
    tpw::TpiDoorwayNearer(reinterpret_cast<uint8_t*>(data), sizeof data[0], 2, 11.5902f);
    const float shift = tpw::kTpiDoorwayFarSteps * 11.5902f / 8388608.0f;
    Check(data[0][0] == 1.5f && data[0][1] == -2.0f && data[0][3] == 4.0f && data[0][4] == 7.0f, "x, y, w and the rest stay");
    Check(std::fabs(data[0][2] - (0.25f - shift * 4.0f)) < 1e-9f && std::fabs(data[1][2] - (-0.5f - shift * 2.0f)) < 1e-9f,
          "clip z moves by kTpiDoorwayFarSteps * farZ steps of w");
    Check(data[1][0] == -3.0f && data[1][3] == 2.0f && data[1][5] == 10.0f, "the second vertex is read at the stride");
}
}  // namespace

int main() {
    Names();
    Marking();
    Nearer();
    Depth();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
