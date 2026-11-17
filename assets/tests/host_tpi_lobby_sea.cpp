// Host test of the island screen's larger sea (assets/tpi_lobby_sea.h): a synthetic sea laid out as the lobby's
// Base.MD2 and Basem1.MD2 are (tpi_lobby_fixture.h), no game data. Runs alone and repeatedly.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "tpi_island_md2.h"
#include "tpi_lobby_fixture.h"
#include "tpi_lobby_sea.h"
#include "tpi_ripple.h"

// TPI_SEA_RING_TWO: set by CMakeLists.txt in a build with the port's fixes, which has include/tpi_island_ring.h.
#ifdef TPI_SEA_RING_TWO
#include "tpi_island_ring.h"
#endif

using namespace tpw::assets;
using namespace tpw::assets::tpi_island;
using namespace tpi_lobby_fixture;

static int g_fail = 0, g_pass = 0;
#define CHECK(cond)                                                      \
    do {                                                                 \
        if (cond) ++g_pass;                                              \
        else {                                                           \
            ++g_fail;                                                    \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        }                                                                \
    } while (0)

namespace {

constexpr double kPi = 3.14159265358979323846;
// The browsing camera's circle round an island (lobby.txt's spin radius) and how far an island reaches from its place
// (include/tpi_island_ring.h: every island stands inside 50 of it).
constexpr float kSpinRadius = 70.0f, kIslandReach = 50.0f;

bool InReach(float x, float z) {
    return x >= kLobbyReachMinX && x <= kLobbyReachMaxX && z >= kLobbyReachMinZ && z <= kLobbyReachMaxZ;
}

// cross(b - a, c - a).y: which way a face turns seen from above.
float TurnY(const Vec3& a, const Vec3& b, const Vec3& c) { return (b.z - a.z) * (c.x - a.x) - (b.x - a.x) * (c.z - a.z); }

const Vec3& At(const Md2Node& n, uint16_t corner) { return n.vertices[n.cornerVertex[corner]]; }

// The edges (vertex pairs) only one face uses.
std::vector<std::pair<uint16_t, uint16_t>> Boundary(const Md2Node& n) {
    std::map<std::pair<uint16_t, uint16_t>, int> uses;
    for (const Md2Face& f : n.faces)
        for (int i = 0; i < 3; ++i) {
            uint16_t a = n.cornerVertex[f.corner[i]], b = n.cornerVertex[f.corner[(i + 1) % 3]];
            if (a > b) std::swap(a, b);
            ++uses[{a, b}];
        }
    std::vector<std::pair<uint16_t, uint16_t>> edge;
    for (const auto& [key, count] : uses)
        if (count == 1) edge.push_back(key);
    return edge;
}

float Radius(const Vec3& p) { return std::hypot(p.x, p.z); }

void TestRadius() {
    const float r = LobbySeaRadius(kSeaX, kSeaZ);
    // Every place the camera can be, and every point it can see from there, is inside the radius with the margin to spare.
    for (float x : {kLobbyReachMinX, kLobbyReachMaxX})
        for (float z : {kLobbyReachMinZ, kLobbyReachMaxZ})
            CHECK(std::hypot(x - kSeaX, z - kSeaZ) + kLobbyFarPlane * kLobbyViewSlant + kLobbySeaMargin <= r + 1e-3f);
    CHECK(r >= std::hypot(kLobbyReachMinX - kSeaX, kLobbyReachMinZ - kSeaZ) + kLobbyFarPlane * kLobbyViewSlant + kLobbySeaMargin - 1e-3f);
    // The view's corner at 115 degrees in the 4:3 frame.
    const double t = std::tan(0.5 * 115.0 * kPi / 180.0);
    CHECK(std::sqrt(1 + t * t * 16 / 9 + t * t) <= kLobbyViewSlant);
    // The original islands' places (a square of side 200 round (500, 500)) and the camera's circle round each.
    for (float x : {400.0f, 600.0f})
        for (float z : {400.0f, 600.0f})
            for (int a = 0; a < 16; ++a) {
                const double angle = 2 * kPi * a / 16;
                CHECK(InReach(x + kSpinRadius * static_cast<float>(std::sin(angle)), z + kSpinRadius * static_cast<float>(std::cos(angle))));
            }
#ifdef TPI_SEA_RING_TWO
    // The second ring's: the camera's circle round each island inside the reach, the island well inside the sea.
    for (const tpw::TpiRingPlace& place : tpw::kTpiRingTwoPlaces) {
        for (int a = 0; a < 16; ++a) {
            const double angle = 2 * kPi * a / 16;
            CHECK(InReach(place.x + kSpinRadius * static_cast<float>(std::sin(angle)),
                          place.z + kSpinRadius * static_cast<float>(std::cos(angle))));
        }
        CHECK(std::hypot(place.x - kSeaX, place.z - kSeaZ) + kIslandReach + kLobbySeaMargin < r);
    }
#endif
}

void TestRingPlan() {
    const std::vector<SeaRing> plan = SeaRingPlan(528.0f, 24, 3800.0f, kLobbySeaStep, kLobbySeaMaxSide);
    CHECK(!plan.empty());
    bool steps = true, sides = true, doubling = true;
    float last = 528.0f;
    int lastSides = 24;
    for (const SeaRing& ring : plan) {
        steps = steps && std::fabs(ring.radius - last - kLobbySeaStep) < 1e-2f;
        sides = sides && 2 * kPi * ring.radius / ring.sides <= kLobbySeaMaxSide;
        doubling = doubling && (ring.sides == lastSides || ring.sides == 2 * lastSides);
        last = ring.radius;
        lastSides = ring.sides;
    }
    CHECK(steps);
    CHECK(sides);
    CHECK(doubling);
    CHECK(plan.back().radius >= 3800.0f);
    CHECK(plan.size() < 2 || plan[plan.size() - 2].radius < 3800.0f);
    CHECK(SeaRingPlan(528.0f, 24, 500.0f, kLobbySeaStep, kLobbySeaMaxSide).empty());
    CHECK(SeaRingPlan(528.0f, 2, 3800.0f, kLobbySeaStep, kLobbySeaMaxSide).empty());
    CHECK(SeaRingPlan(528.0f, 24, 3800.0f, 0, kLobbySeaMaxSide).empty());
    CHECK(SeaRingPlan(0, 24, 3800.0f, kLobbySeaStep, kLobbySeaMaxSide).empty());
}

// Extends one of the fixture's discs and checks the new part against the old.
void CheckExtended(const Md2Node& before, bool clockwise, float edgeY) {
    Md2Node n = before;
    UvPlane plane;
    size_t firstNew = 0;
    std::string err;
    const float outer = 1500.0f;
    CHECK(ExtendSeaNode(n, outer, plane, firstNew, err));
    CHECK(err.empty());
    CHECK(firstNew == before.u.size());
    CHECK(n.u.size() > before.u.size() && n.faces.size() > before.faces.size());
    // The old corners and faces as they were (the face normals' indices move up past the new corner normals).
    bool kept = true;
    for (size_t c = 0; c < before.u.size(); ++c)
        kept = kept && n.u[c] == before.u[c] && n.v[c] == before.v[c] && n.cornerVertex[c] == before.cornerVertex[c];
    for (size_t f = 0; f < before.faces.size(); ++f) {
        for (int k = 0; k < 3; ++k) kept = kept && n.faces[f].corner[k] == before.faces[f].corner[k];
        const Vec3& was = before.points[before.faces[f].normal];
        const Vec3& is = n.points[n.faces[f].normal];
        kept = kept && was.x == is.x && was.y == is.y && was.z == is.z;
    }
    CHECK(kept);
    // The new corners: flat at the edge's height, mapped by the same plane (the texture runs on), lit from straight up.
    bool flat = true, mapped = true, lit = true;
    float farthest = 0;
    for (size_t c = firstNew; c < n.u.size(); ++c) {
        const Vec3& p = At(n, static_cast<uint16_t>(c));
        flat = flat && std::fabs(p.y - edgeY) < 1e-4f;
        mapped = mapped && std::fabs(n.u[c] - SeaU(p.x, p.z)) < 2e-3f && std::fabs(n.v[c] - SeaV(p.x, p.z)) < 2e-3f;
        lit = lit && n.points[c].y == 1 && n.points[c].x == 0 && n.points[c].z == 0;
        farthest = std::max(farthest, Radius(p));
    }
    CHECK(flat);
    CHECK(mapped);
    CHECK(lit);
    CHECK(farthest >= outer && farthest < outer + kLobbySeaStep + 1);
    CHECK(std::fabs(plane.U(100, -40) - SeaU(100, -40)) < 1e-4f && std::fabs(plane.V(-30, 70) - SeaV(-30, 70)) < 1e-4f);
    // Every new face wound as the old ones (seen from above), its normal pointing the same way, and no face folded.
    bool wound = true, facing = true;
    for (size_t f = before.faces.size(); f < n.faces.size(); ++f) {
        const Md2Face& face = n.faces[f];
        const float turn = TurnY(At(n, face.corner[0]), At(n, face.corner[1]), At(n, face.corner[2]));
        wound = wound && (clockwise ? turn > 0 : turn < 0) && std::fabs(turn) > 1;
        facing = facing && n.points[face.normal].y == 1;
    }
    CHECK(wound);
    CHECK(facing);
    // No T-junction and no gap: the only edges one face uses are the new outer ring's.
    bool closed = true;
    for (const auto& [a, b] : Boundary(n))
        closed = closed && Radius(n.vertices[a]) >= outer && Radius(n.vertices[b]) >= outer;
    CHECK(closed);
    CHECK(!Boundary(n).empty());
    // The outer ring covers the radius asked for, not only at its corners: the polygon's sides are short enough.
    float nearestEdge = 1e9f;
    for (const auto& [a, b] : Boundary(n)) {
        const Vec3& p = n.vertices[a];
        const Vec3& q = n.vertices[b];
        nearestEdge = std::min(nearestEdge, std::hypot(0.5f * (p.x + q.x), 0.5f * (p.z + q.z)));
    }
    CHECK(nearestEdge >= outer - 2.0f);
    // One face set still holds the node's every face and corner.
    const Md2FaceSet& set = n.faceSets.back();
    CHECK(set.firstFace + set.faceCount == n.faces.size());
    CHECK(set.lastCorner + 1u == n.u.size());
    CHECK(n.points.size() == n.u.size() + n.faces.size());
    CHECK(n.box[0] <= -outer && n.box[3] >= outer && n.box[2] <= -outer && n.box[5] >= outer);
}

void TestExtendNode() {
    const Md2Model sea = MakeSeaModel();
    CheckExtended(sea.nodes[1], true, 16.89f);
    CheckExtended(sea.nodes[2], false, 0.0f);
}

void TestExtendNodeRefuses() {
    const Md2Model sea = MakeSeaModel();
    auto refuses = [](Md2Node n, const char* why) {
        const Md2Node before = n;
        UvPlane plane;
        size_t firstNew = 0;
        std::string err;
        const bool ok = ExtendSeaNode(n, 1500.0f, plane, firstNew, err);
        CHECK(!ok);
        if (err.find(why) == std::string::npos) std::printf("  expected \"%s\", got \"%s\"\n", why, err.c_str());
        CHECK(err.find(why) != std::string::npos);
        CHECK(n.u.size() == before.u.size() && n.faces.size() == before.faces.size());
    };
    Md2Node empty;
    empty.name = "Empty";
    refuses(empty, "no geometry");
    // An edge point lifted.
    Md2Node bumpy = sea.nodes[2];
    bumpy.vertices.back().y += 1.0f;
    refuses(bumpy, "not level");
    // A corner's texture coordinate moved off the plane.
    Md2Node warped = sea.nodes[2];
    warped.u[3] += 0.5f;
    refuses(warped, "not mapped flat");
    // A hole in the middle (the centre's fan taken out): two loops.
    Md2Node holed = sea.nodes[2];
    std::vector<Md2Face> faces;
    for (size_t f = 0; f < holed.faces.size(); ++f)
        if (f % 3 != 0) faces.push_back(holed.faces[f]);
    holed.faces = faces;
    holed.faceSets.back().faceCount = static_cast<uint16_t>(faces.size());
    refuses(holed, "more than one loop");
    // The disc not round its frame's origin.
    Md2Node moved = sea.nodes[2];
    for (Vec3& p : moved.vertices) p.x += 2000.0f;
    for (size_t c = 0; c < moved.u.size(); ++c) {
        const Vec3& p = At(moved, static_cast<uint16_t>(c));
        moved.u[c] = SeaU(p.x, p.z);
        moved.v[c] = SeaV(p.x, p.z);
    }
    refuses(moved, "round its centre");
    // A second face set after the sea's.
    Md2Node split = sea.nodes[2];
    split.faceSets.back().faceCount = static_cast<uint16_t>(split.faceSets.back().faceCount - 1);
    split.faceSets.back().lastCorner = static_cast<uint16_t>(split.faceSets.back().lastCorner - 1);
    Md2FaceSet extra;
    extra.texture = 1;
    extra.firstFace = 0;
    extra.faceCount = 1;
    extra.firstCorner = 0;
    extra.lastCorner = 2;
    split.faceSets.insert(split.faceSets.begin(), extra);
    refuses(split, "does not end the node");
}

void TestEmbeddedRecords() {
    const Md2Model sea = MakeSeaModel();
    std::vector<uint8_t> bytes = WriteMd2(sea);
    std::vector<uint8_t> records;
    uint16_t count = 9;
    std::string err;
    CHECK(ReadEmbeddedRecords(bytes, records, count, err));
    CHECK(count == 0 && records.empty());
    AppendEmbeddedRecords(bytes, SeaRecord(), 1);
    CHECK(ReadEmbeddedRecords(bytes, records, count, err));
    CHECK(count == 1 && records == SeaRecord());
    // The mesh reads as before with them on its end.
    Md2Model back;
    CHECK(ReadMd2(bytes, back, err, true));
    CHECK(back.nodes.size() == sea.nodes.size());
    CHECK(!ReadEmbeddedRecords(std::vector<uint8_t>(8, 0), records, count, err));
}

void TestExtendLobbySea() {
    const Md2Model sea = MakeSeaModel();
    const std::vector<uint8_t> base = MakeSeaBase(sea);
    const std::vector<uint8_t> anim = MakeSeaAnim(sea, base);
    std::vector<uint8_t> outBase, outAnim;
    std::string err;
    CHECK(ExtendLobbySea(base, anim, outBase, outAnim, err));
    if (!err.empty()) std::printf("  %s\n", err.c_str());
    Md2Model out;
    CHECK(ReadMd2(outBase, out, err, true));
    CHECK(out.nodes.size() == sea.nodes.size());
    if (out.nodes.size() != sea.nodes.size()) return;
    // The nodes, their order, frames and textures kept; the root still a mini node.
    bool same = out.textures.size() == sea.textures.size();
    for (size_t i = 0; i < sea.nodes.size(); ++i) {
        same = same && out.nodes[i].name == sea.nodes[i].name && out.nodes[i].parent == sea.nodes[i].parent &&
               out.nodes[i].mini == sea.nodes[i].mini;
        for (int k = 0; k < 16; ++k) same = same && std::fabs(out.nodes[i].matrix[static_cast<size_t>(k)] - sea.nodes[i].matrix[static_cast<size_t>(k)]) < 1e-5f;
    }
    for (size_t t = 0; same && t < sea.textures.size(); ++t)
        same = out.textures[t].material == sea.textures[t].material && out.textures[t].frames == sea.textures[t].frames;
    CHECK(same);
    // The embedded record carried over.
    std::vector<uint8_t> records;
    uint16_t count = 0;
    CHECK(ReadEmbeddedRecords(outBase, records, count, err));
    CHECK(count == 1 && records == SeaRecord());
    // Both discs reach the radius round the sea's centre in the lobby's world (the turned Sea_Under too).
    const float radius = LobbySeaRadius(kSeaX, kSeaZ);
    const std::vector<Matrix> world = WorldMatrices(out);
    for (size_t i = 1; i < out.nodes.size(); ++i) {
        float nearestEdge = 1e9f;
        for (const auto& [a, b] : Boundary(out.nodes[i])) {
            const Vec3 p = Transform(out.nodes[i].vertices[a], world[i]);
            const Vec3 q = Transform(out.nodes[i].vertices[b], world[i]);
            nearestEdge = std::min(nearestEdge, std::hypot(0.5f * (p.x + q.x) - kSeaX, 0.5f * (p.z + q.z) - kSeaZ));
        }
        CHECK(nearestEdge >= radius - 2.0f);
        CHECK(nearestEdge < radius + kLobbySeaStep + 1);
    }
    CHECK(out.box[0] <= kSeaX - radius + 2 && out.box[3] >= kSeaX + radius - 2);
    // The animation: a track for every corner; the old ones as they were, the new ones sliding as the old do.
    UvAnim slide, was;
    CHECK(ReadUvAnim(outAnim, slide, err));
    CHECK(ReadUvAnim(anim, was, err));
    CHECK(!slide.other && slide.frames == 200 && slide.nodes.size() == 2);
    const std::string name = "Basem1.MD2";
    CHECK(outAnim.size() > 0x98 && std::equal(name.begin(), name.end(), outAnim.begin() + 0x18));
    for (size_t k = 0; k < slide.nodes.size() && k < 2; ++k) {
        const UvNodeAnim& track = slide.nodes[k];
        const size_t node = k + 1;
        CHECK(track.Node() == FileNodeIndex(out, node));
        const Md2Node& n = out.nodes[node];
        CHECK(track.tracks.size() == n.u.size());
        if (track.tracks.size() != n.u.size()) continue;
        bool old = true, fresh = true;
        for (size_t c = 0; c < was.nodes[k].tracks.size(); ++c)
            for (size_t j = 0; j < 2; ++j)
                old = old && track.tracks[c][j].frame == was.nodes[k].tracks[c][j].frame &&
                      track.tracks[c][j].u == was.nodes[k].tracks[c][j].u && track.tracks[c][j].v == was.nodes[k].tracks[c][j].v;
        for (size_t c = was.nodes[k].tracks.size(); c < n.u.size(); ++c) {
            const std::vector<UvKey>& keys = track.tracks[c];
            fresh = fresh && keys.size() == 2 && keys[0].frame == 0 && keys[1].frame == 200 && keys[0].u == n.u[c] &&
                    keys[0].v == n.v[c] && std::fabs(keys[1].u - keys[0].u + 1) < 1e-3f && std::fabs(keys[1].v - keys[0].v + 1) < 1e-3f;
        }
        CHECK(old);
        CHECK(fresh);
    }
}

void TestExtendLobbySeaRefuses() {
    const Md2Model sea = MakeSeaModel();
    const std::vector<uint8_t> base = MakeSeaBase(sea);
    std::vector<uint8_t> outBase, outAnim;
    std::string err;
    // The corners not all sliding alike.
    UvAnim uneven;
    CHECK(ReadUvAnim(MakeSeaAnim(sea, base), uneven, err));
    uneven.nodes[0].tracks[5].back().u += 0.25f;
    CHECK(!ExtendLobbySea(base, WriteUvAnim(base, "Basem1.MD2", uneven), outBase, outAnim, err));
    CHECK(err.find("slide differently") != std::string::npos);
    // A track missing.
    UvAnim short_;
    CHECK(ReadUvAnim(MakeSeaAnim(sea, base), short_, err));
    short_.nodes[1].tracks.pop_back();
    CHECK(!ExtendLobbySea(base, WriteUvAnim(base, "Basem1.MD2", short_), outBase, outAnim, err));
    CHECK(err.find("tracks for") != std::string::npos);
    // A node tilted.
    Md2Model tilted = sea;
    tilted.nodes[1].matrix = Multiply(Matrix{1, 0, 0, 0, 0, 0.8f, 0.6f, 0, 0, -0.6f, 0.8f, 0, 0, 0, 0, 1}, tilted.nodes[1].matrix);
    const std::vector<uint8_t> tiltedBase = MakeSeaBase(tilted);
    CHECK(!ExtendLobbySea(tiltedBase, MakeSeaAnim(tilted, tiltedBase), outBase, outAnim, err));
    CHECK(err.find("tilted") != std::string::npos);
    // Not a mesh, not an animation.
    CHECK(!ExtendLobbySea(std::vector<uint8_t>(16, 0), MakeSeaAnim(sea, base), outBase, outAnim, err));
    CHECK(!ExtendLobbySea(base, std::vector<uint8_t>(16, 0), outBase, outAnim, err));
}

}  // namespace

int main() {
    TestRadius();
    TestRingPlan();
    TestExtendNode();
    TestExtendNodeRefuses();
    TestEmbeddedRecords();
    TestExtendLobbySea();
    TestExtendLobbySeaRefuses();
    std::printf("tpi_lobby_sea: %d checks passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
