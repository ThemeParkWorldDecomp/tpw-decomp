// host_ground_pad.cpp: the port's fix groundpad (include/PortGroundPad.h), a ride's base against the park's ground
// under it (tpw-port #125). The switch's text, the base's record bit, and then a shop's base laid over the ground cells
// under it (the same flat ground, the base's cells split along the ground's diagonal or the other one) put through the
// port's depth math as the camera turns: the projection and CMatrixPowerPC::Project's perspective depth curve in single
// precision, with the constants the game uses at zooms 130, 250, 400 and 500 (gei_Display.mZ68..mZ78 and the
// projection, read from the game, as tests/host_tpi_doorway.cpp has them), each base sample compared with the ground
// under it in a 24-bit depth buffer. Projected as the Mac does, a base split as the ground is lies at the ground's
// depth (a tie: only GL_LEQUAL, the draw order and the driver's own rounding decide the test), and one split the other
// way is up to about two times the far slope steps behind or ahead of it, as each vertex's rounded depth bends the two
// splits apart; moved nearer by tpw::groundpad::Nearer at 4 steps, either is ahead of the ground at every sample by 8
// depth steps or more. No game data.
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include "PortGroundPad.h"

namespace {
int checks = 0, failures = 0;
void Check(bool ok, const char* what) {
    ++checks;
    if (!ok) { ++failures; std::printf("FAIL: %s\n", what); }
}

void Settings() {
    Check(tpw::groundpad::StepsSetting(nullptr) == 0 && tpw::groundpad::StepsSetting("") == 0, "no value is off");
    Check(tpw::groundpad::StepsSetting("0") == 0, "0 is off");
    Check(tpw::groundpad::StepsSetting("1") == 1 && tpw::groundpad::StepsSetting("4") == 4, "1 and 4 are steps");
    Check(tpw::groundpad::StepsSetting("64") == 64, "64 is the most");
    Check(tpw::groundpad::StepsSetting("65") == 0 && tpw::groundpad::StepsSetting("99999999999") == 0, "more is off");
    Check(tpw::groundpad::StepsSetting("-4") == 0 && tpw::groundpad::StepsSetting("4x") == 0 &&
              tpw::groundpad::StepsSetting(" 4") == 0 && tpw::groundpad::StepsSetting("true") == 0,
          "anything else is off");
    Check(tpw::groundpad::nearerSteps == 0.0f, "the switch starts off");
}

void Records() {
    const uint16_t base = 0x0801, plain = 0x0001, only = 0x0800;
    Check(tpw::groundpad::IsBase(&base) && tpw::groundpad::IsBase(&only), "a record with bit 0x800 is a base's");
    Check(!tpw::groundpad::IsBase(&plain), "one without it is not");
    Check(!tpw::groundpad::IsBase(nullptr), "an untextured face set is not");
}

void Nearer() {
    // Only the depth moves, by the same share of w for any vertex; the stride is honoured.
    float data[2][6] = {{1.5f, -2.0f, 0.25f, 4.0f, 7.0f, 8.0f}, {-3.0f, 1.0f, -0.5f, 2.0f, 9.0f, 10.0f}};
    tpw::groundpad::Nearer(reinterpret_cast<uint8_t*>(data), sizeof data[0], 2, 4.0f, 11.5902f);
    const float shift = 4.0f * 11.5902f / 8388608.0f;
    Check(data[0][0] == 1.5f && data[0][1] == -2.0f && data[0][3] == 4.0f && data[0][4] == 7.0f, "x, y, w and the rest stay");
    Check(std::fabs(data[0][2] - (0.25f - shift * 4.0f)) < 1e-9f && std::fabs(data[1][2] - (-0.5f - shift * 2.0f)) < 1e-9f,
          "clip z moves by steps * farZ steps of w");
    Check(data[1][0] == -3.0f && data[1][3] == 2.0f && data[1][5] == 10.0f, "the second vertex is read at the stride");
    float same[4] = {1.0f, 2.0f, 0.5f, 3.0f};
    tpw::groundpad::Nearer(reinterpret_cast<uint8_t*>(same), sizeof same, 0, 4.0f, 11.5902f);
    Check(same[2] == 0.5f, "no vertices, nothing moves");
}

// The game's depth constants at one zoom: the projection's z row (clip z = p22 * view z + p32, w = p23 * view z) and
// CDisplay::ZSetup's outputs.
struct Zoom {
    float zoom, p22, p32, p23, split, nearW, nearZW, farW, farZ;
};
const Zoom kZooms[] = {
    {130, 0.707453f, -0.0707453f, 0.707107f, 0.541695f, 0.1f, 0.184606f, -5.95167f, 6.95167f},
    {250, 0.707315f, -0.0707315f, 0.707107f, 0.949966f, 0.1f, 0.105267f, -10.5902f, 11.5902f},
    {400, 0.707244f, -0.0707244f, 0.707107f, 1.47919f, 0.1f, 0.0676046f, -16.6032f, 17.6032f},
    {500, 0.707219f, -0.0707219f, 0.707107f, 1.82322f, 0.1f, 0.0548482f, -20.5119f, 21.5119f},
};

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
// The camera looking down at `pitch` at the shop from `zoom` away, turned by `yaw`.
Camera Look(double zoom, double yaw, double pitch) {
    const double poi[3] = {525.0, 0.0, 176.0};
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

struct Screen {
    double x, y, depth;
};
Screen ToScreen(const double p[3], const Camera& cam, const Zoom& c, float steps) {
    double rel[3];
    for (int i = 0; i < 3; ++i) rel[i] = p[i] - cam.eye[i];
    const auto dot = [&](const double a[3]) { return static_cast<float>(rel[0] * a[0] + rel[1] * a[1] + rel[2] * a[2]); };
    const float xv = dot(cam.right), yv = dot(cam.up), zv = dot(cam.forward);
    Vertex vertex{{0.707107f * xv, 0.707107f * yv, c.p22 * zv + c.p32, c.p23 * zv}};
    Project(vertex, c);
    if (steps > 0.0f) tpw::groundpad::Nearer(reinterpret_cast<uint8_t*>(&vertex), sizeof(Vertex), 1, steps, c.farZ);
    const float* v = vertex.v;
    const float ndc = v[2] / v[3];
    return {static_cast<double>(v[0] / v[3]), static_cast<double>(v[1] / v[3]), (static_cast<double>(ndc) + 1.0) / 2.0};
}

struct Turn {
    long samples = 0, behind = 0;
    double least = 1e9, most = -1e9;   // the fewest and most depth steps the base was ahead of the ground by
};

// A 2 by 2 cell shop base (x 520..530, z 171..181, y 0, cells of 5) over the same ground cells, the ground's split from
// one corner, the base's from the same (`sameSplit`) or the other, sampled on a grid in each base triangle on screen at
// each step of a full turn and three pitches.
Turn RunTurn(const Zoom& c, float steps, bool sameSplit) {
    const int n = 12;
    Turn turn;
    for (double pitch : {0.6, 0.9, 1.2}) {
        for (int step = 0; step < 63; ++step) {
            const Camera cam = Look(c.zoom, -3.1 + 0.1 * step, pitch);
            for (int cx = 0; cx < 2; ++cx) {
                for (int cz = 0; cz < 2; ++cz) {
                    const double x0 = 520.0 + 5.0 * cx, x1 = x0 + 5.0, z0 = 171.0 + 5.0 * cz, z1 = z0 + 5.0;
                    const double ground[2][3][3] = {{{x0, 0, z0}, {x1, 0, z1}, {x1, 0, z0}},
                                                    {{x0, 0, z0}, {x0, 0, z1}, {x1, 0, z1}}};
                    const double other[2][3][3] = {{{x1, 0, z0}, {x0, 0, z1}, {x0, 0, z0}},
                                                   {{x1, 0, z0}, {x1, 0, z1}, {x0, 0, z1}}};
                    const auto& base = sameSplit ? ground : other;
                    Screen bs[2][3], gs[2][3];
                    for (int t = 0; t < 2; ++t) {
                        for (int k = 0; k < 3; ++k) {
                            bs[t][k] = ToScreen(base[t][k], cam, c, steps);
                            gs[t][k] = ToScreen(ground[t][k], cam, c, 0.0f);
                        }
                    }
                    for (int t = 0; t < 2; ++t) {
                        for (int i = 1; i < n; ++i) {
                            for (int j = 1; j < n - i; ++j) {
                                const double l[3] = {double(i) / n, double(j) / n, 1.0 - double(i) / n - double(j) / n};
                                double sx = 0, sy = 0, zb = 0;
                                for (int k = 0; k < 3; ++k) {
                                    sx += l[k] * bs[t][k].x;
                                    sy += l[k] * bs[t][k].y;
                                    zb += l[k] * bs[t][k].depth;
                                }
                                for (const auto& g : gs) {
                                    const double d =
                                        (g[1].y - g[2].y) * (g[0].x - g[2].x) + (g[2].x - g[1].x) * (g[0].y - g[2].y);
                                    const double l1 =
                                        ((g[1].y - g[2].y) * (sx - g[2].x) + (g[2].x - g[1].x) * (sy - g[2].y)) / d;
                                    const double l2 =
                                        ((g[2].y - g[0].y) * (sx - g[2].x) + (g[0].x - g[2].x) * (sy - g[2].y)) / d;
                                    const double l3 = 1.0 - l1 - l2;
                                    if (l1 < -1e-9 || l2 < -1e-9 || l3 < -1e-9) continue;
                                    const double zg = l1 * g[0].depth + l2 * g[1].depth + l3 * g[2].depth;
                                    const double qb = std::floor(zb * 16777216.0), qg = std::floor(zg * 16777216.0);
                                    ++turn.samples;
                                    if (qb > qg) ++turn.behind;
                                    if (qg - qb < turn.least) turn.least = qg - qb;
                                    if (qg - qb > turn.most) turn.most = qg - qb;
                                    break;
                                }
                            }
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
    for (const bool sameSplit : {true, false}) {
        const char* const split = sameSplit ? "split as the ground" : "split the other way";
        for (const Zoom& c : kZooms) {
            const Turn mac = RunTurn(c, 0.0f, sameSplit);
            const Turn port = RunTurn(c, 4.0f, sameSplit);
            std::printf("%s, zoom %3.0f: as projected the base is %.0f to %.0f steps ahead of the ground (%ld of %ld "
                        "samples behind); 4 steps nearer, %.0f to %.0f ahead (%ld behind)\n",
                        split, c.zoom, mac.least, mac.most, mac.behind, mac.samples, port.least, port.most, port.behind);
            std::snprintf(what, sizeof what, "%s, zoom %.0f: every sample is compared", split, c.zoom);
            Check(mac.samples > 10000 && port.samples == mac.samples, what);
            if (sameSplit) {
                std::snprintf(what, sizeof what, "%s, zoom %.0f: as projected the base ties with the ground", split,
                              c.zoom);
                Check(mac.least >= -1.0 && mac.most <= 1.0, what);
            } else {
                std::snprintf(what, sizeof what, "%s, zoom %.0f: as projected the ground wins part of the base", split,
                              c.zoom);
                Check(mac.behind > 0 && mac.least <= -c.farZ, what);
            }
            std::snprintf(what, sizeof what, "%s, zoom %.0f: 4 steps nearer, the base is ahead at every sample by 8 "
                          "steps", split, c.zoom);
            Check(port.behind == 0 && port.least >= 8.0, what);
        }
    }
}
}  // namespace

int main() {
    Settings();
    Records();
    Nearer();
    Depth();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
