// Host tests of the Theme Park Inc converter's terrain repair and obstacle merge (assets/tpi_terrain.h). Every fixture
// is built here, in code: a small terrain mesh (one geometry node and a 4 x 2 height field), obstacle meshes, their
// animations and .sam files, and a few TGAs; no game data.
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "../tpi_island_md2.h"
#include "../tpi_terrain.h"

using namespace tpw::assets;
namespace md2 = tpw::assets::tpi_island;

namespace {

int g_fail = 0, g_pass = 0;

#define CHECK(cond)                                                     \
    do {                                                                \
        if (cond) {                                                     \
            g_pass++;                                                   \
        } else {                                                        \
            g_fail++;                                                   \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        }                                                               \
    } while (0)

using Bytes = std::vector<uint8_t>;

uint16_t L16(const Bytes& d, size_t at) { return static_cast<uint16_t>(d[at] | (d[at + 1] << 8)); }
uint32_t L32(const Bytes& d, size_t at) { return L16(d, at) | (static_cast<uint32_t>(L16(d, at + 2)) << 16); }
void Put32(Bytes& d, size_t at, uint32_t v) {
    for (int i = 0; i < 4; ++i) d[at + i] = static_cast<uint8_t>(v >> (8 * i));
}
void Add16(Bytes& d, uint32_t v) {
    d.push_back(static_cast<uint8_t>(v));
    d.push_back(static_cast<uint8_t>(v >> 8));
}
void Add32(Bytes& d, uint32_t v) {
    Add16(d, v & 0xffff);
    Add16(d, v >> 16);
}
void AddF(Bytes& d, float f) {
    uint32_t v;
    std::memcpy(&v, &f, 4);
    Add32(d, v);
}

// A 32-bit (or `bpp`) TGA of image type `type`, w x h, every pixel's alpha `alpha`.
Bytes Tga(int w, int h, uint8_t alpha, uint8_t type = 2, uint8_t bpp = 32) {
    Bytes t = {0, 0, type, 0, 0, 0, 0, 0, 0, 0, 0, 0, static_cast<uint8_t>(w), 0, static_cast<uint8_t>(h), 0, bpp, 0x28};
    for (int i = 0; i < w * h; ++i) {
        t.insert(t.end(), {255, 255, 255});
        if (bpp == 32) t.push_back(alpha);
    }
    return t;
}

constexpr uint32_t kW = 4, kH = 2;
constexpr uint16_t kHidden = 0x0001;
constexpr uint16_t kTexGrass = 0, kTexClear = 1, kTexRock = 2;

// The terrain: textures grass, clear, rock; geometry over quads (0,0) and (1,0) (world x 0..20, z 0..10) and a small
// corner triangle in quad (2,0) (an eighth of it); the height field's quads as given, row by row.
Bytes Terrain(const std::vector<std::pair<uint16_t, uint16_t>>& quads, bool heightField = true) {
    md2::Md2Model m;
    m.name = "base.MD2";
    for (const char* name : {"grass.tga", "trans01.tga", "rock.tga"}) {
        md2::Md2Texture t;
        t.frames.push_back(name);
        m.textures.push_back(t);
    }
    md2::NodeBuilder b;
    b.Quad(md2::Vec3{0, 0, 0}, md2::Vec3{20, 0, 0}, md2::Vec3{20, 0, 10}, md2::Vec3{0, 0, 10}, 0, 0, 1, 0, 1, 1, 0, 1, kTexRock);
    b.Triangle(md2::Vec3{20, 0, 0}, md2::Vec3{25, 0, 0}, md2::Vec3{20, 0, 5}, 0, 0, 1, 0, 0, 1, kTexRock);
    m.nodes.push_back(b.Build("root"));
    md2::RecomputeBoxes(m);
    Bytes d = md2::WriteMd2(m);
    if (!heightField) return d;
    while (d.size() % 4) d.push_back(0);
    const uint32_t hf = static_cast<uint32_t>(d.size());
    Add32(d, 0);
    Add16(d, (kW + 1) * (kH + 1));
    Add16(d, kW * kH);
    for (float f : {0.0f, 0.0f, 10.0f, 10.0f}) AddF(d, f);
    Add32(d, kW);
    Add32(d, kH);
    AddF(d, -10.0f);
    AddF(d, 10.0f);
    const size_t ptrs = d.size();
    Add32(d, 0);
    Add32(d, 0);
    Put32(d, ptrs, static_cast<uint32_t>(d.size()));
    for (uint32_t i = 0; i < (kW + 1) * (kH + 1); ++i) AddF(d, 0.0f);
    Put32(d, ptrs + 4, static_cast<uint32_t>(d.size()));
    for (const auto& q : quads) {
        Add16(d, q.first);
        Add16(d, q.second);
    }
    Put32(d, 0x30, L32(d, 0x30) | 2);
    Put32(d, 0x6c, hf);
    return d;
}

std::pair<uint16_t, uint16_t> Quad(const Bytes& d, uint32_t x, uint32_t y) {
    const uint32_t q = L32(d, L32(d, 0x6c) + 0x2c) + 4 * (x + y * kW);
    return {L16(d, q), L16(d, q + 2)};
}

void TestTga() {
    CHECK(TpiTgaFullyTransparent(Tga(8, 8, 0)));
    CHECK(!TpiTgaFullyTransparent(Tga(8, 8, 255)));
    Bytes oneSeen = Tga(2, 2, 0);
    oneSeen[18 + 4 * 3 + 3] = 1;
    CHECK(!TpiTgaFullyTransparent(oneSeen));
    CHECK(!TpiTgaFullyTransparent(Tga(2, 2, 0, 2, 24)));   // no alpha channel: opaque
    CHECK(!TpiTgaFullyTransparent(Tga(2, 2, 0, 10)));      // run-length: the game does not load it
    Bytes cut = Tga(2, 2, 0);
    cut.pop_back();
    CHECK(!TpiTgaFullyTransparent(cut));
    CHECK(!TpiTgaFullyTransparent(Tga(0, 2, 0)));
    // An id field is skipped.
    Bytes id = Tga(2, 2, 0);
    id[0] = 3;
    id.insert(id.begin() + 18, {9, 9, 9});
    CHECK(TpiTgaFullyTransparent(id));
    CHECK(!TpiTgaFullyTransparent(Bytes{1, 2, 3}));
}

void TestRepair() {
    const Bytes clear = Tga(8, 8, 0), grass = Tga(2, 2, 255);
    const std::map<std::string, const Bytes*> files = {{"trans01.tga", &clear}, {"grass.tga", &grass}};
    auto lookup = [&](const std::string& name) -> const Bytes* {
        std::string lower;
        for (char c : name) lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        auto it = files.find(lower);
        return it == files.end() ? nullptr : it->second;
    };
    // Row 0: a clear quad under the geometry, a hidden one under it, a hidden one with only an eighth under it, a
    // clear one nothing covers. Row 1: shown ground, mostly flags 0x42 on grass, one 0x44 on rock.
    const std::vector<std::pair<uint16_t, uint16_t>> quads = {
        {0x0042, kTexClear}, {kHidden, kTexGrass}, {kHidden, kTexGrass}, {0x0044, kTexClear},
        {0x0042, kTexGrass}, {0x0042, kTexGrass}, {0x0044, kTexRock},   {0x0042, kTexGrass}};
    Bytes d = Terrain(quads);
    const Bytes before = d;
    TpiTerrainReport report;
    std::string err;
    CHECK(RepairTpiTerrain(d, lookup, report, err));
    CHECK(err.empty());
    CHECK(report.madeHidden == 2);
    CHECK(report.clearTextures.size() == 1 && report.clearTextures[0] == "trans01.tga");
    // The clear quad the geometry covers is hidden, its texture kept.
    CHECK(Quad(d, 0, 0) == std::make_pair(uint16_t(0x0043), kTexClear));
    // So is the clear quad nothing covers (an obstacle draws there); the hidden quads stay as they are, covered or not.
    CHECK(Quad(d, 3, 0) == std::make_pair(uint16_t(0x0045), kTexClear));
    CHECK(Quad(d, 1, 0) == quads[1]);
    CHECK(Quad(d, 2, 0) == quads[2]);
    // Shown quads are left alone, and nothing outside the quads changes.
    for (uint32_t x = 0; x < kW; ++x) CHECK(Quad(d, x, 1) == quads[kW + x]);
    const size_t at = L32(d, L32(d, 0x6c) + 0x2c);
    CHECK(d.size() == before.size());
    CHECK(std::equal(d.begin(), d.begin() + at, before.begin()));
    CHECK(std::equal(d.begin() + at + 4 * kW * kH, d.end(), before.begin() + at + 4 * kW * kH));

    // A second pass finds nothing more to do.
    const Bytes once = d;
    CHECK(RepairTpiTerrain(d, lookup, report, err));
    CHECK(report.madeHidden == 0 && d == once);

    // Without the clear texture's file nothing changes.
    d = Terrain(quads);
    CHECK(RepairTpiTerrain(d, [](const std::string&) -> const Bytes* { return nullptr; }, report, err));
    CHECK(report.madeHidden == 0 && report.clearTextures.empty());
    CHECK(d == Terrain(quads));

    // A mesh without a height field is left as it is.
    d = Terrain(quads, false);
    const Bytes plain = d;
    CHECK(RepairTpiTerrain(d, lookup, report, err));
    CHECK(d == plain && report.madeHidden == 0);

    // A height field whose quads run past the end of the file, and a file that is not a mesh, are refused.
    d = Terrain(quads);
    d.resize(d.size() - 2);
    CHECK(!RepairTpiTerrain(d, lookup, report, err) && !err.empty());
    d = Bytes(64, 0);
    err.clear();
    CHECK(!RepairTpiTerrain(d, lookup, report, err) && !err.empty());
}

// ---- the obstacle merge

// A .sam as Theme Park Inc's obstacles have them (CRLF lines): its Info.Id, Info.IsObstacle, the shape between lines of
// dashes, the shape once cleared (Obstacle.AfterShape, when given), their offset in the map and any more lines.
Bytes Sam(int isObstacle, const std::vector<std::string>& rows, int ox, int oy, const std::vector<std::string>& after = {},
          int id = 1702, const std::string& more = "") {
    std::string s = "#\r\n#\tTheme Park 2 Ride Description File\r\n\r\nInfo.Id\t\t\t" + std::to_string(id) +
                    "\r\nInfo.Shape\r\n---\r\n";
    for (const std::string& r : rows) s += r + "\r\n";
    s += "---\r\n\r\nInfo.IsObstacle\t\t\t\t" + std::to_string(isObstacle) + "\r\nInfo.DontApplyOffset\t\t\t1\t\r\n";
    if (!after.empty()) {
        s += "Obstacle.AfterShape\r\n---\r\n";
        for (const std::string& r : after) s += r + "\r\n";
        s += "---\r\n";
    }
    s += "\r\nObstacle.ShapeOffsetX \t\t\t" + std::to_string(ox) + "\r\nObstacle.ShapeOffsetY \t\t\t" +
         std::to_string(oy) + "\r\n" + more;
    return Bytes(s.begin(), s.end());
}

// A flat square node of side `size` at height `y`, its corner at the node's origin, all of texture `texture`.
md2::Md2Node Square(const std::string& name, float size, float y, uint16_t texture) {
    md2::NodeBuilder b;
    b.Quad(md2::Vec3{0, y, 0}, md2::Vec3{size, y, 0}, md2::Vec3{size, y, size}, md2::Vec3{0, y, size}, 0, 0, 1, 0, 1, 1, 0, 1,
           texture);
    return b.Build(name);
}

md2::Md2Model Model(const std::string& name, const std::vector<std::string>& textures) {
    md2::Md2Model m;
    m.name = name;
    for (const std::string& t : textures) {
        md2::Md2Texture tex;
        tex.frames.push_back(t);
        m.textures.push_back(tex);
    }
    md2::Md2Node root;
    root.name = name.substr(0, name.find('.'));
    m.nodes.push_back(root);
    return m;
}

void AddNode(md2::Md2Model& m, md2::Md2Node n, int parent, const md2::Matrix& matrix) {
    n.parent = parent;
    n.matrix = matrix;
    m.nodes.push_back(std::move(n));
}

// An animation file as RideAnim.cpp reads its block (header 0x98): its start frame, its list of nodes to disable (node
// ids) and node animations with flag 0x20000 and an enable list, each for a node by record index, and with keys that
// move it (a place key pointer, 0x18) when `moves`.
struct Track {
    uint16_t node;
    std::vector<int16_t> enables;
    bool moves = false;
};
Bytes Anim(uint32_t frame, const std::vector<uint16_t>& disable, const std::vector<Track>& tracks) {
    Bytes d(0xa0, 0);
    Put32(d, 0x98, 0xa0);
    d.resize(0xa0 + 0x48, 0);
    Put32(d, 0xa0 + 4, frame);
    d[0xa0 + 0x12] = static_cast<uint8_t>(tracks.size());
    d[0xa0 + 0x1a] = static_cast<uint8_t>(disable.size());
    Put32(d, 0xa0 + 0x38, static_cast<uint32_t>(d.size()));
    for (uint16_t id : disable) Add16(d, id);
    while (d.size() % 4) d.push_back(0);
    const size_t at = d.size();
    Put32(d, 0xa0 + 0x2c, static_cast<uint32_t>(at));
    d.resize(at + 0x40 * tracks.size(), 0);
    for (size_t k = 0; k < tracks.size(); ++k) {
        const size_t o = at + 0x40 * k;
        Put32(d, o + 4, 0x20000);
        d[o + 0x14] = static_cast<uint8_t>(tracks[k].node);
        d[o + 0x16] = static_cast<uint8_t>(tracks[k].enables.size());
        Put32(d, o + 0x30, static_cast<uint32_t>(d.size()));
        if (tracks[k].moves) Put32(d, o + 0x18, static_cast<uint32_t>(d.size()));
        for (int16_t e : tracks[k].enables) Add16(d, static_cast<uint16_t>(e));
    }
    return d;
}

Bytes Text(const std::string& s) { return Bytes(s.begin(), s.end()); }

const TpiWadMember* Member(const std::vector<TpiWadMember>& members, const std::string& name) {
    for (const TpiWadMember& m : members)
        if (m.name == name) return &m;
    return nullptr;
}

void TestMerge() {
    // The terrain: row 0 shown ground; row 1 a hidden quad the lake obstacle draws over (0,1), shown ground, a hidden
    // quad nothing draws over (2,1), shown ground.
    const std::vector<std::pair<uint16_t, uint16_t>> quads = {
        {0x0042, kTexGrass}, {0x0042, kTexGrass}, {0x0042, kTexGrass}, {0x0042, kTexGrass},
        {kHidden, kTexGrass}, {0x0042, kTexGrass}, {kHidden, kTexRock}, {0x0042, kTexGrass}};
    const Bytes grass = Tga(2, 2, 255), rock = Tga(2, 2, 200), smallGrass = Tga(1, 1, 255);
    const Bytes otherRock = Tga(4, 4, 100), plank = Tga(2, 2, 90), smallPlank = Tga(1, 1, 90), water = Tga(2, 2, 60);
    const Bytes wood = Tga(2, 2, 30), smallWood = Tga(1, 1, 30), takenRock = Tga(3, 3, 10);
    const std::vector<TpiWadMember> terrain = {
        {"base.MD2", Terrain(quads)},
        {"base.map", Text("map")},
        {"textures/grass.tga", grass},
        {"textures/rock.tga", rock},
        {"textures/qickload.txt", Text("grass.tga\r\nrock.tga")},
        {"stexture/grass.tga", smallGrass},
        {"stexture/qickload.txt", Text("grass.tga\n")}};
    const std::map<std::string, const Bytes*> sharedFiles = {
        {"textures/wood.tga", &wood}, {"stexture/wood.tga", &smallWood}, {"textures/rock_1.tga", &takenRock}};
    auto shared = [&](const std::string& path) -> const Bytes* {
        auto it = sharedFiles.find(path);
        return it == sharedFiles.end() ? nullptr : it->second;
    };

    // The bridge, modelled in the world's frame under a root the game places (its matrix is left out): a deck at ground
    // level over quads (1,0) and (2,0), an arch 8 above the ground over (3,0) below it, a wreck the animation disables,
    // a plank its enable list hides at the start frame, a clearing crew member and an attachment point. Its shape
    // blocks (1,0) to (4,0), the last past the grid, and (2,1).
    md2::Md2Model bridge = Model("bridge.MD2", {"rock.tga", "Wood.tga", "grass.tga", "plank.tga", "ghost.tga"});
    bridge.nodes[0].matrix = md2::Translation(500, 0, 0);
    md2::NodeBuilder deck;
    deck.Quad(md2::Vec3{0, 0, 0}, md2::Vec3{10, 0, 0}, md2::Vec3{10, 0, 10}, md2::Vec3{0, 0, 10}, 0, 0, 1, 0, 1, 1, 0, 1, 2);
    deck.Quad(md2::Vec3{10, 0, 0}, md2::Vec3{20, 0, 0}, md2::Vec3{20, 0, 10}, md2::Vec3{10, 0, 10}, 0, 0, 1, 0, 1, 1, 0, 1,
              3);
    AddNode(bridge, deck.Build("deck"), 0, md2::Translation(10, 0, 0));                 // 1
    AddNode(bridge, Square("arch", 10, 0, 0), 1, md2::Translation(20, 8, 0));           // 2: world (30, 8, 0)
    AddNode(bridge, Square("wreck", 10, 0, 1), 0, md2::Translation(0, 0, 10));          // 3
    AddNode(bridge, Square("plank", 10, 0, 4), 0, md2::Translation(30, 0, 10));         // 4
    AddNode(bridge, Square("boy 01", 5, 0, 1), 0, md2::Translation(30, 0, 0));          // 5
    md2::Md2Node attach;
    attach.name = "attach";
    attach.flags = 0x200;
    attach.mini = true;
    AddNode(bridge, attach, 1, md2::Translation(5, 0, 5));
    md2::RecomputeBoxes(bridge);
    const TpiObstacle bridgeObstacle = {
        "Bridge",
        {{"bridge.sam", Sam(1, {"HHHH", ".X.."}, 1, 0)},
         {"bridge.MD2", md2::WriteMd2(bridge)},
         {"bridgem1.MD2", Anim(10, {3}, {{4, {1, -5, 20}}})},
         {"bridgem.MD2", Anim(0, {1, 2}, {})},
         {"textures/rock.tga", otherRock},
         {"textures/grass.tga", grass},
         {"textures/plank.tga", plank},
         {"stexture/plank.tga", smallPlank},
         {"textures/qickload.txt", Text("rock.tga\r\n")}}};

    // The lake, below the ground over the hidden quad (0,1), with a dry bed its only animation (lakem.MD2) disables.
    md2::Md2Model lake = Model("lake.MD2", {"water.tga"});
    AddNode(lake, Square("water", 10, -2, 0), 0, md2::Translation(0, 0, 10));           // 1
    AddNode(lake, Square("dry", 10, 0, 0), 0, md2::Translation(10, 0, 10));             // 2
    md2::RecomputeBoxes(lake);
    const TpiObstacle lakeObstacle = {"lake",
                                      {{"lake.sam", Sam(1, {"H"}, 0, 1)},
                                       {"lake.MD2", md2::WriteMd2(lake)},
                                       {"lakem.MD2", Anim(0, {2}, {})},
                                       {"TEXTURES/water.tga", water}}};

    // A camera wad (not an obstacle) and a wad without a .sam are skipped.
    const TpiObstacle camera = {"Camera", {{"camera.sam", Sam(0, {"H"}, 0, 0)}, {"camera.MD2", md2::WriteMd2(lake)}}};
    const TpiObstacle empty = {"empty", {{"textures/water.tga", water}}};

    std::vector<TpiWadMember> members = terrain;
    TpiObstacleReport report;
    std::string err;
    CHECK(MergeTpiObstacles(members, {camera, bridgeObstacle, lakeObstacle, empty}, shared, report, err));
    CHECK(err.empty());
    CHECK(report.obstacles.size() == 4);
    if (report.obstacles.size() != 4) return;
    CHECK(report.obstacles[0].name == "Camera" && report.obstacles[0].skipped == "not an obstacle");
    CHECK(report.obstacles[3].name == "empty" && report.obstacles[3].skipped == "no .sam");
    const TpiObstacleResult& b = report.obstacles[1];
    CHECK(b.name == "Bridge" && b.skipped.empty());
    CHECK(b.nodes == 2 && b.faces == 6);
    CHECK(b.blocked == 2 && b.alreadyHidden == 1 && b.open == 1);
    CHECK(b.leftOut == std::vector<std::string>{"boy 01"});
    const TpiObstacleResult& l = report.obstacles[2];
    CHECK(l.name == "lake" && l.skipped.empty());
    CHECK(l.nodes == 1 && l.faces == 2 && l.blocked == 0 && l.alreadyHidden == 1 && l.open == 0 && l.leftOut.empty());

    // The terrain's mesh: its own node, then the shown nodes under its root at their places in the world, then the
    // quads the obstacles' cells lost, lowered under them.
    const TpiWadMember* base = Member(members, "base.MD2");
    CHECK(base != nullptr);
    if (base == nullptr) return;
    md2::Md2Model merged;
    CHECK(md2::ReadMd2(base->data, merged, err, true));
    CHECK(merged.nodes.size() == 5);
    if (merged.nodes.size() != 5) return;
    CHECK(merged.nodes[0].name == "root" && merged.nodes[0].matrix == md2::IdentityMatrix());
    CHECK(merged.nodes[1].name == "deck" && merged.nodes[1].parent == 0);
    CHECK(merged.nodes[1].matrix == md2::Translation(10, 0, 0));
    CHECK(merged.nodes[2].name == "arch" && merged.nodes[2].parent == 0);
    CHECK(merged.nodes[2].matrix == md2::Translation(30, 8, 0));
    CHECK(merged.nodes[3].name == "water" && merged.nodes[3].matrix == md2::Translation(0, 0, 10));
    // The deck's two cells (1,0) and (2,0) and the shown cells around them, (0,0), (3,0), (1,1) and (3,1) (not (0,1) or
    // (2,1), hidden before), each two triangles with the quad's own texture, 0.6 below the ground.
    const md2::Md2Node& under = merged.nodes[4];
    CHECK(under.name == "tpi underground" && under.parent == 0 && under.matrix == md2::IdentityMatrix());
    CHECK(under.faces.size() == 12 && under.faceSets.size() == 1 && under.faceSets[0].texture == kTexGrass);
    for (const md2::Vec3& v : under.vertices) {
        CHECK(v.y == -0.6f && v.x >= 0 && v.x <= 40 && v.z >= 0 && v.z <= 20);
        CHECK(!(v.x == 0 && v.z == 20));
    }
    CHECK(under.box[0] == 0 && under.box[3] == 40 && under.box[2] == 0 && under.box[5] == 20);
    // The textures: the terrain's three, then the obstacles' under the names the terrain has their pictures by; grass is
    // the terrain's own picture and slot.
    std::vector<std::string> names;
    for (const md2::Md2Texture& t : merged.textures) names.push_back(t.frames.empty() ? "" : t.frames[0]);
    CHECK((names == std::vector<std::string>{"grass.tga", "trans01.tga", "rock.tga", "rock_2.tga", "Wood.tga", "plank.tga",
                                             "ghost.tga", "water.tga"}));
    CHECK(merged.nodes[1].faceSets.size() == 2 && merged.nodes[1].faceSets[0].texture == 0 &&
          merged.nodes[1].faceSets[1].texture == 5);
    CHECK(merged.nodes[2].faceSets.size() == 1 && merged.nodes[2].faceSets[0].texture == 3);
    CHECK(merged.nodes[3].faceSets.size() == 1 && merged.nodes[3].faceSets[0].texture == 7);

    // The deck's covered cells are hidden; the arch's cell, above the ground, is not; nothing else changes, and no
    // hidden quad becomes ground.
    CHECK(Quad(base->data, 0, 0) == quads[0]);
    CHECK(Quad(base->data, 1, 0) == std::make_pair(uint16_t(0x0043), kTexGrass));
    CHECK(Quad(base->data, 2, 0) == std::make_pair(uint16_t(0x0043), kTexGrass));
    CHECK(Quad(base->data, 3, 0) == quads[3]);
    for (uint32_t x = 0; x < kW; ++x) CHECK(Quad(base->data, x, 1) == quads[kW + x]);
    const Bytes original = Terrain(quads);
    const uint32_t hf = L32(base->data, 0x6c), ohf = L32(original, 0x6c);
    CHECK(std::equal(base->data.begin() + hf, base->data.begin() + hf + 0x28, original.begin() + ohf));
    CHECK(L32(base->data, 0x30) == L32(original, 0x30));

    // The texture files the terrain gains, and its lists.
    CHECK((report.addedTextures == std::vector<std::string>{"textures/rock_2.tga", "textures/plank.tga", "stexture/plank.tga",
                                                            "textures/water.tga"}));
    CHECK(report.renamedTextures == std::vector<std::string>{"Bridge: rock.tga -> rock_2.tga"});
    CHECK(report.missingTextures == std::vector<std::string>{"Bridge: ghost.tga"});
    CHECK(members.size() == terrain.size() + 4);
    const TpiWadMember* added = Member(members, "textures/rock_2.tga");
    CHECK(added != nullptr && added->data == otherRock);
    added = Member(members, "stexture/plank.tga");
    CHECK(added != nullptr && added->data == smallPlank);
    added = Member(members, "textures/water.tga");
    CHECK(added != nullptr && added->data == water);
    CHECK(Member(members, "stexture/rock_2.tga") == nullptr && Member(members, "textures/Wood.tga") == nullptr);
    const TpiWadMember* list = Member(members, "textures/qickload.txt");
    CHECK(list != nullptr && list->data == Text("grass.tga\r\nrock.tga\nrock_2.tga\nplank.tga\nwater.tga\n"));
    list = Member(members, "stexture/qickload.txt");
    CHECK(list != nullptr && list->data == Text("grass.tga\nplank.tga\n"));
    for (const TpiWadMember& m : terrain)
        if (m.name != "base.MD2" && m.name.find("qickload") == std::string::npos) CHECK(Member(members, m.name)->data == m.data);

    // The hidden quad nothing draws over is reported.
    CHECK((report.stillClear == std::vector<std::pair<int, int>>{{2, 1}}));

    // Without obstacles, or with only skipped ones, the terrain stays as it is; its uncovered hidden quads are reported.
    members = terrain;
    CHECK(MergeTpiObstacles(members, {}, shared, report, err));
    CHECK(report.obstacles.empty() && report.addedTextures.empty());
    CHECK((report.stillClear == std::vector<std::pair<int, int>>{{0, 1}, {2, 1}}));
    CHECK(MergeTpiObstacles(members, {camera, empty}, shared, report, err));
    CHECK(report.obstacles.size() == 2 && report.stillClear.size() == 2);
    CHECK(members.size() == terrain.size());
    for (size_t i = 0; i < members.size() && i < terrain.size(); ++i)
        CHECK(members[i].name == terrain[i].name && members[i].data == terrain[i].data);

    // A terrain without base.MD2 or without a height field, and an obstacle mesh that cannot be read, are refused and
    // leave the members as they were.
    members = {terrain[1]};
    err.clear();
    CHECK(!MergeTpiObstacles(members, {lakeObstacle}, shared, report, err) && !err.empty() && members.size() == 1);
    members = terrain;
    members[0].data = Terrain(quads, false);
    err.clear();
    CHECK(!MergeTpiObstacles(members, {lakeObstacle}, shared, report, err) && !err.empty());
    CHECK(members[0].data == Terrain(quads, false));
    TpiObstacle broken = lakeObstacle;
    broken.members[1].data = Bytes(64, 0);
    members = terrain;
    err.clear();
    CHECK(!MergeTpiObstacles(members, {broken}, shared, report, err) && err.find("lake/lake.MD2") == 0);
    CHECK(members.size() == terrain.size() && members[0].data == terrain[0].data);
}

// A flat square of side `size` at height `y`, its corner at the node's origin, cut into n x n quads.
md2::Md2Node Grid(const std::string& name, float size, int n, float y, uint16_t texture) {
    md2::NodeBuilder b;
    const float c = size / n;
    for (int j = 0; j < n; ++j)
        for (int i = 0; i < n; ++i)
            b.Quad(md2::Vec3{i * c, y, j * c}, md2::Vec3{(i + 1) * c, y, j * c}, md2::Vec3{(i + 1) * c, y, (j + 1) * c},
                   md2::Vec3{i * c, y, (j + 1) * c}, 0, 0, 1, 0, 1, 1, 0, 1, texture);
    return b.Build(name);
}

// Obstacles drawn as they are once cleared: from the last clearing animation, swapped for another object, or (when the
// animations move too much of them) as before; their cells from Obstacle.AfterShape. A bridge's walkway cells (',') keep
// their hidden quads and base.lnd is left as it was: no ground is drawn across the river.
void TestCleared() {
    // Row 0: shown grass, two hidden quads with the clear texture (a bridge's walkway under the deck: (1,0) is a ','
    // cell, (2,0) a '.' one), shown rock. Row 1: shown grass, shown grass, shown grass, shown rock.
    const std::vector<std::pair<uint16_t, uint16_t>> quads = {
        {0x0043, kTexGrass}, {0x0043, kTexClear}, {0x0043, kTexClear}, {0x0042, kTexRock},
        {0x0042, kTexGrass}, {0x0042, kTexGrass}, {0x0042, kTexGrass}, {0x0042, kTexRock}};
    std::vector<std::pair<uint16_t, uint16_t>> shownFirst = quads;
    shownFirst[0].first = 0x0042;
    const Bytes clear = Tga(2, 2, 0), grass = Tga(2, 2, 255), rock = Tga(2, 2, 200), pond = Tga(2, 2, 70);
    std::vector<TpiWadMember> terrain = {{"base.MD2", Terrain(shownFirst)},
                                         {"textures/grass.tga", grass},
                                               {"textures/rock.tga", rock},
                                               {"textures/trans01.tga", clear},
                                               {"textures/qickload.txt", Text("grass.tga\n")}};
    // base.lnd's control map (17 x 9 texels: 4 for each quad and one more): channel 0 is 0xff but where the river runs
    // under the bridge, at the corners of quads (1,0) and (2,0); channels 1 to 3 hold the quad's index.
    const uint32_t cw = 17, ch = 9;
    Bytes lnd(23 + cw * ch * 4 + 4, 0);
    lnd[0] = 3;
    const uint32_t header[3] = {cw, ch, 4};
    std::memcpy(lnd.data() + 11, header, 12);
    auto texel = [&](uint32_t x, uint32_t y, uint32_t col, uint32_t row) {
        return 23 + (((2 - y - 1) * 4 + row) * cw + x * 4 + col) * 4;
    };
    for (uint32_t y = 0; y < 2; ++y)
        for (uint32_t x = 0; x < 4; ++x)
            for (uint32_t row = 0; row < 4; ++row)
                for (uint32_t col = 0; col < 4; ++col) {
                    const uint32_t at = texel(x, y, col, row);
                    lnd[at] = y == 0 && (x == 1 || x == 2) ? 0x18 : 0xff;
                    lnd[at + 1] = lnd[at + 2] = lnd[at + 3] = static_cast<uint8_t>(y * 4 + x);
                }
    terrain.push_back({"base.lnd", lnd});
    auto shared = [](const std::string&) -> const Bytes* { return nullptr; };

    // The tunnel: a passage at ground level over (2,1) (18 faces), a gate its last animation disables, a ship piece the
    // animations move (2 faces, a tenth), a marker decal, a crew member and a plank. Before clearing it closes all four
    // of its cells; once cleared (1,0) is a walkway, (2,0) and (1,1) are open and (2,1) is closed.
    md2::Md2Model tunnel = Model("tunnel.MD2", {"grass.tga"});
    AddNode(tunnel, Grid("passage", 10, 3, 0, 0), 0, md2::Translation(20, 0, 10));     // 1
    AddNode(tunnel, Square("gate", 10, 0, 0), 0, md2::Translation(10, 0, 0));           // 2
    AddNode(tunnel, Square("ship", 10, 3, 0), 0, md2::Translation(0, 0, 10));           // 3
    AddNode(tunnel, Square("unbuildable01", 10, 0, 0), 0, md2::Translation(10, 0, 10)); // 4
    AddNode(tunnel, Square("boy 01", 5, 0, 0), 0, md2::Translation(10, 0, 0));          // 5
    AddNode(tunnel, Square("plank 01", 5, 0, 0), 0, md2::Translation(10, 0, 0));        // 6
    md2::RecomputeBoxes(tunnel);
    const TpiObstacle tunnelObstacle = {"tunnel",
                                        {{"tunnel.sam", Sam(1, {"HH", "HH"}, 1, 0, {",.", ".X"}, 2703)},
                                         {"tunnel.MD2", md2::WriteMd2(tunnel)},
                                         {"tunnelm1.MD2", Anim(0, {}, {})},
                                         {"tunnelm2.MD2", Anim(0, {2}, {{3, {1}, true}})},
                                         {"textures/grass.tga", grass}}};

    // The lake, swapped once cleared for the drained lake (its Obstacle.ReplaceWithID: dlake's Info.Id), which is not an
    // obstacle itself: a pond at ground level over (0,1), the lake's remaining water ('X').
    md2::Md2Model lake = Model("lake.MD2", {"grass.tga"});
    AddNode(lake, Square("water", 10, 0, 0), 0, md2::Translation(0, 0, 0));
    md2::RecomputeBoxes(lake);
    md2::Md2Model dlake = Model("dlake.MD2", {"pond.tga"});
    AddNode(dlake, Square("pond", 10, 0, 0), 0, md2::Translation(0, 0, 10));
    md2::RecomputeBoxes(dlake);
    const TpiObstacle lakeObstacle = {
        "lake",
        {{"lake.sam", Sam(1, {"H"}, 0, 1, {"X"}, 1703, "Obstacle.ReplaceWithID 1704\r\n")},
         {"lake.MD2", md2::WriteMd2(lake)},
         {"lakem1.MD2", Anim(0, {}, {})},
         {"lakem2.MD2", Anim(0, {}, {})},
         {"textures/grass.tga", grass}}};
    const TpiObstacle dlakeObstacle = {"dlake",
                                       {{"dlake.sam", Sam(0, {"."}, 0, 1, {"."}, 1704)},
                                        {"dlake.MD2", md2::WriteMd2(dlake)},
                                        {"dlakem.MD2", Anim(0, {}, {})},
                                        {"textures/pond.tga", pond}}};

    // The rock: its animations move all of it, so it stays as it is before clearing, closing its Info.Shape (3,1).
    md2::Md2Model rockMesh = Model("rock.MD2", {"rock.tga"});
    AddNode(rockMesh, Square("boulder", 10, 0, 0), 0, md2::Translation(30, 0, 10));
    md2::RecomputeBoxes(rockMesh);
    const TpiObstacle rockObstacle = {"Rock",
                                      {{"rock.sam", Sam(1, {"H"}, 3, 1, {"."}, 3700)},
                                       {"rock.MD2", md2::WriteMd2(rockMesh)},
                                       {"rockm1.MD2", Anim(0, {}, {{1, {1}, true}})},
                                       {"rockm2.MD2", Anim(0, {}, {})},
                                       {"textures/rock.tga", rock}}};

    std::vector<TpiWadMember> members = terrain;
    TpiObstacleReport report;
    std::string err;
    CHECK(MergeTpiObstacles(members, {tunnelObstacle, lakeObstacle, dlakeObstacle, rockObstacle}, shared, report, err));
    CHECK(err.empty());
    CHECK(report.obstacles.size() == 4);
    if (report.obstacles.size() != 4) return;
    const TpiObstacleResult& t = report.obstacles[0];
    CHECK(t.name == "tunnel" && t.skipped.empty() && t.state == "cleared");
    CHECK(t.nodes == 1 && t.faces == 18);
    CHECK(t.blocked == 1 && t.alreadyHidden == 0 && t.open == 0);
    CHECK((t.leftOut == std::vector<std::string>{"ship", "unbuildable01", "boy 01", "plank 01"}));
    const TpiObstacleResult& l = report.obstacles[1];
    CHECK(l.name == "lake" && l.skipped.empty() && l.state == "replaced by dlake");
    CHECK(l.nodes == 1 && l.faces == 2 && l.blocked == 1);
    CHECK(report.obstacles[2].name == "dlake" && report.obstacles[2].skipped == "not an obstacle");
    const TpiObstacleResult& r = report.obstacles[3];
    CHECK(r.name == "Rock" && r.state == "before clearing" && r.nodes == 1 && r.blocked == 1);
    CHECK(report.addedTextures == std::vector<std::string>{"textures/pond.tga"});
    CHECK(report.missingTextures.empty());

    const TpiWadMember* base = Member(members, "base.MD2");
    CHECK(base != nullptr);
    if (base == nullptr) return;
    md2::Md2Model merged;
    CHECK(md2::ReadMd2(base->data, merged, err, true));
    std::vector<std::string> names;
    for (const md2::Md2Node& n : merged.nodes) names.push_back(n.name);
    CHECK((names == std::vector<std::string>{"root", "passage", "pond", "boulder", "tpi underground"}));
    // The walkway's clear quad and the '.' cell's both stay hidden: no grass is painted over the river.
    CHECK(Quad(base->data, 1, 0) == quads[1]);
    CHECK(Quad(base->data, 2, 0) == quads[2]);
    // The closed cells the meshes cover are hidden; the open ones are not, whatever Info.Shape said.
    CHECK(Quad(base->data, 2, 1) == std::make_pair(uint16_t(0x0043), kTexGrass));
    CHECK(Quad(base->data, 0, 1) == std::make_pair(uint16_t(0x0043), kTexGrass));
    CHECK(Quad(base->data, 3, 1) == std::make_pair(uint16_t(0x0043), kTexRock));
    CHECK(Quad(base->data, 1, 1) == quads[5]);
    CHECK(Quad(base->data, 0, 0) == shownFirst[0] && Quad(base->data, 3, 0) == quads[3]);

    // base.lnd's control map is left as it was, so the river under the bridge is drawn as before.
    const TpiWadMember* landscape = Member(members, "base.lnd");
    CHECK(landscape != nullptr && landscape->data == lnd);
}

// A bridge its animations move all of, with walkway cells (','): drawn posed at its animations' last frame, the deck
// where the last one's shape track puts it (sunk 20 below the ground before), and its walkway quads left hidden.
void TestPosed() {
    const std::vector<std::pair<uint16_t, uint16_t>> quads = {
        {0x0042, kTexGrass}, {0x0043, kTexClear}, {0x0042, kTexGrass}, {0x0042, kTexGrass},
        {0x0042, kTexGrass}, {0x0042, kTexGrass}, {0x0042, kTexGrass}, {0x0042, kTexGrass}};
    const Bytes grass = Tga(2, 2, 255), clear = Tga(2, 2, 0);
    std::vector<TpiWadMember> members = {{"base.MD2", Terrain(quads)},
                                         {"textures/grass.tga", grass},
                                         {"textures/trans01.tga", clear},
                                         {"textures/qickload.txt", Text("grass.tga\n")}};
    md2::Md2Model bridge = Model("bridge.MD2", {"grass.tga"});
    AddNode(bridge, Square("deck", 10, 0, 0), 0, md2::Translation(10, -20, 0));
    md2::RecomputeBoxes(bridge);
    // The last animation, frames 0 to 30: its track moves the deck and has a shape track (flag 1) whose last key, at
    // frame 20, puts it at ground level.
    Bytes last = Anim(0, {}, {{1, {}, true}});
    Put32(last, 0xa0 + 8, 30);
    uint32_t track = 0;
    std::memcpy(&track, last.data() + 0xa0 + 0x2c, 4);
    Put32(last, track + 4, 0x20001);
    const uint32_t shape = static_cast<uint32_t>(last.size());
    Put32(last, track + 0x18, shape);
    Add32(last, 0);
    Add16(last, 2);
    Add16(last, 2);
    Add32(last, shape + 0x10);
    Add32(last, shape + 0x28);
    for (float f : {10.0f, -20.0f, 0.0f, 10.0f, 0.0f, 0.0f}) AddF(last, f);
    for (uint16_t f : {0, 0, 20, 0}) Add16(last, f);
    const TpiObstacle bridgeObstacle = {"bridge",
                                        {{"bridge.sam", Sam(1, {"H"}, 1, 0, {","}, 2800)},
                                         {"bridge.MD2", md2::WriteMd2(bridge)},
                                         {"bridgem1.MD2", Anim(0, {}, {{1, {}, true}})},
                                         {"bridgem2.MD2", last},
                                         {"textures/grass.tga", grass}}};
    auto shared = [](const std::string&) -> const Bytes* { return nullptr; };
    TpiObstacleReport report;
    std::string err;
    CHECK(MergeTpiObstacles(members, {bridgeObstacle}, shared, report, err));
    CHECK(report.obstacles.size() == 1);
    if (report.obstacles.size() != 1) return;
    CHECK(report.obstacles[0].state == "cleared, posed" && report.obstacles[0].nodes == 1);
    const TpiWadMember* base = Member(members, "base.MD2");
    md2::Md2Model merged;
    CHECK(base != nullptr && md2::ReadMd2(base->data, merged, err, true));
    bool atGround = false;
    for (const md2::Md2Node& n : merged.nodes)
        if (n.name == "deck") atGround = n.matrix[13] == 0.0f && n.matrix[12] == 10.0f;
    CHECK(atGround);
    CHECK(base != nullptr && Quad(base->data, 1, 0) == quads[1]);
}

}  // namespace

int main() {
    TestTga();
    TestRepair();
    TestMerge();
    TestCleared();
    TestPosed();
    std::printf("tpi_terrain: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
