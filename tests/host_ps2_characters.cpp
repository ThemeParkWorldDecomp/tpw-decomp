// host_ps2_characters.cpp: the PS2 Guest Models mod's pure helpers (TPW_MOD_HOOK ps2_characters, include/Ps2Characters.h),
// on synthetic packs and pictures, without game data: the sprite sets paired with the PS2's characters, the PC actions'
// clips, the strike sign, the render side's hash, a pack's frames, where a model stands, a sprite's picture's height in the world and the
// TGA reader.
// Run by ctest ("ps2_characters") in a build with the port's fixes, which alone has the hook. Exits nonzero on a failure.
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "Ps2Characters.h"

namespace {

int gFailures = 0;

void Check(bool ok, const std::string& what) {
    std::printf("%s: %s\n", ok ? "ok" : "FAILED", what.c_str());
    if (!ok) ++gFailures;
}

bool Near(float a, float b, float within = 1e-3f) { return std::fabs(a - b) <= within; }

bool Pack(const char* name, const char* want) {
    const char* got = tpw::ps2::PackForSprite(name);
    if (want == nullptr) return got == nullptr;
    return got != nullptr && std::string(got) == want;
}

// A pack of `vertices` vertices and one clip (group 0, clip 0) of `frames` frames, filled by `at(f, v, xyz)`; scale 1/16.
template <typename At>
tpw::Ps2Pack MakePack(uint32_t vertices, uint16_t frames, At at) {
    tpw::Ps2Pack pack;
    pack.vertices = vertices;
    pack.scale = 1.0f / 16;
    tpw::Ps2PackClip clip;
    clip.frames = frames;
    clip.length = frames;
    pack.clips.push_back(clip);
    for (uint16_t f = 0; f < frames; ++f)
        for (uint32_t v = 0; v < vertices; ++v) {
            float p[3];
            at(f, v, p);
            for (float c : p) pack.frames.push_back(static_cast<int16_t>(std::lround(c * 16)));
        }
    return pack;
}

}  // namespace

int main() {
    using namespace tpw::ps2;

    // The sprite sets, by folder and file as the engine names them: the eight kids in order, boys then girls, the four
    // costume kids, the staff and the entertainers; the rides' heads and anything else have no model.
    Check(Pack("generic\\kids:spr_be", "boy1a") && Pack("generic\\kids:spr_bi", "boy2a") &&
              Pack("generic\\kids:spr_fr", "boy3a") && Pack("generic\\kids:spr_sa", "boy4a"),
          "the four boys");
    Check(Pack("generic\\kids:spr_ch", "girl1a") && Pack("generic\\kids:spr_ki", "girl2a") &&
              Pack("generic\\kids:spr_su", "girl3a") && Pack("generic\\kids:spr_ta", "girl4a"),
          "the four girls");
    Check(Pack("fantasy\\costumes:spr_bk", "fantasykid") && Pack("hallow\\costumes:spr_hk", "hallowkid") &&
              Pack("jungle\\costumes:spr_ti", "junglekid") && Pack("space\\costumes:spr_sk", "spacekid"),
          "the four costume kids");
    Check(Pack("generic\\guards:spr_gu", "guard") && Pack("generic\\handymen:spr_ha", "handyman") &&
              Pack("generic\\researchers:spr_re", "researcher") && Pack("generic\\mechanics:spr_fa", "fatmechanic") &&
              Pack("generic\\mechanics:spr_om", "fatmechanic"),
          "the staff, both mechanics as the PS2's one");
    Check(Pack("fantasy\\entertainers:spr_fl", "flower") && Pack("fantasy\\entertainers:spr_gn", "gnome") &&
              Pack("hallow\\entertainers:spr_dr", "vampire") && Pack("hallow\\entertainers:spr_fk", "franky") &&
              Pack("jungle\\entertainers:spr_di", "dino") && Pack("jungle\\entertainers:spr_ex", "hunter") &&
              Pack("space\\entertainers:spr_sm", "alien") && Pack("space\\entertainers:spr_sp", "spaceman"),
          "the eight entertainers the PS2 has");
    Check(Pack("fantasy\\entertainers:spr_la", "flower") && Pack("hallow\\entertainers:spr_hu", "franky") &&
              Pack("jungle\\entertainers:spr_na", "hunter") && Pack("space\\entertainers:spr_mo", "alien"),
          "the four it has not, as a PS2 entertainer of their own world");
    Check(Pack("GENERIC\\KIDS:SPR_BE", "boy1a") && Pack("generic\\kids:spr_be.esp", "boy1a") &&
              Pack("Generic/Kids/SPR_BE.ESP", "boy1a") && Pack("x\\space\\entertainers:spr_sp", "spaceman"),
          "a name in any case, with an extension, with slashes or a longer path");
    Check(Pack("generic\\kidsheads:spr_be", "boy1a") && Pack("generic\\kidsheads:spr_ta", "girl4a") &&
              Pack("jungle\\CostumeHeads:SPR_TI", "junglekid") && Pack("space\\costumeheads:spr_sp", "spacekid") &&
              PackForSet("generic\\kidsheads:spr_ch").kind == SetKind::Head &&
              PackForSet("space\\costumeheads:spr_sp").kind == SetKind::Head &&
              PackForSet("space\\entertainers:spr_sp").kind == SetKind::Staff,
          "the rides' heads are their kids' heads (spr_sp: the space costume kid's head, not the spaceman)");
    Check(Pack("generic\\kidsheads:spr_gu", nullptr) && Pack("space\\costumeheads:spr_be", nullptr) &&
              Pack("fantasy\\costumeheads:spr_mo", nullptr),
          "no head for a file the heads' folders do not pair");
    Check(Pack("generic\\kids:spr_bex", nullptr) && Pack("generic\\kids:spr_b", nullptr) &&
              Pack("generic\\guards:spr_be", nullptr) && Pack("generic\\kid:spr_be", nullptr) && Pack("SPR_BE", nullptr) &&
              Pack("generic\\balloons:spr_bl", nullptr) && Pack("", nullptr) && Pack(nullptr, nullptr),
          "a longer or shorter file, another folder, no folder, another set and no name have no model");
    Check(PackForSet("generic\\kids:spr_be").kind == SetKind::Kid &&
              PackForSet("space\\costumes:spr_sk").kind == SetKind::Kid &&
              PackForSet("generic\\mechanics:spr_om").kind == SetKind::Staff &&
              PackForSet("space\\entertainers:spr_mo").kind == SetKind::Staff,
          "the visitors read the kids' clip table, staff and entertainers the staff's");

    // The actions' clips.
    const auto clipIs = [](int32_t action, uint8_t group, ClipTime time) {
        const ClipChoice c = ClipForAction(action);
        return c.group == group && c.clip == 0 && c.time == time;
    };
    Check(clipIs(1, 0, ClipTime::Frame) && clipIs(2, 1, ClipTime::Frame), "walk and run follow the sprite's frame");
    Check(clipIs(0, 2, ClipTime::Idle), "standing is an idle");
    Check(clipIs(4, 5, ClipTime::Once) && clipIs(6, 4, ClipTime::Once) && clipIs(12, 8, ClipTime::Once),
          "throw, be sick and cheer play once");
    Check(clipIs(11, 6, ClipTime::Loop) && clipIs(14, 12, ClipTime::Loop), "fidget and cry loop");
    Check(clipIs(3, 2, ClipTime::Loop) && clipIs(15, 2, ClipTime::Loop) && clipIs(-1, 2, ClipTime::Loop),
          "any other action loops the first idle");

    // The staff's and entertainers' actions' clips.
    const auto staffIs = [](int32_t action, uint8_t group, ClipTime time) {
        const ClipChoice c = ClipForStaffAction(action);
        const ClipChoice d = ClipFor(SetKind::Staff, action);
        return c.group == group && c.clip == 0 && c.time == time && d.group == group && d.time == time;
    };
    Check(staffIs(1, 0, ClipTime::Frame) && staffIs(2, 1, ClipTime::Frame) && staffIs(9, 0, ClipTime::Frame),
          "the staff's walk, the guard's chase and its walk with a lost child follow the sprite's frame");
    Check(staffIs(0, 2, ClipTime::Idle) && staffIs(3, 2, ClipTime::Loop), "standing is the idle");
    Check(staffIs(4, 5, ClipTime::AfterIntro), "work is the work loop after its intro");
    Check(staffIs(5, 7, ClipTime::Loop) && staffIs(6, 3, ClipTime::Loop),
          "a strike is the strike loop, held in the pointer the PS2's STAFFPICKEDUP");
    Check(staffIs(7, 4, ClipTime::Once) && staffIs(8, 5, ClipTime::Frame), "the guard's catch, then the carry");
    Check(staffIs(10, 5, ClipTime::Loop) && staffIs(11, 2, ClipTime::Loop) && staffIs(-1, 2, ClipTime::Loop),
          "the entertainers' other acts, and anything else");
    // The strike sign: only the strike (action 5) is the strike group, and only the strike group shows the sign.
    {
        bool others = true;
        for (int32_t action = -1; action <= 15; ++action)
            if (action != 5)
                others = others && ClipForStaffAction(action).group != kStrikeGroup &&
                         !ShowsSign(ClipFor(SetKind::Staff, action).group);
        Check(others, "no staff action but the strike is the strike group or shows the sign");
        Check(ShowsSign(ClipFor(SetKind::Staff, 5).group), "the strike shows the sign");
        bool groups = true;
        for (int g = 0; g < 256; ++g) groups = groups && ShowsSign(static_cast<uint8_t>(g)) == (g == 7);
        Check(groups, "the sign shows in the strike group (7) and no other");
        // A visitor's or a ride head's clip never shows it either (the kids have no sign).
        bool kids = true;
        for (int32_t action = -1; action <= 15; ++action)
            kids = kids && !ShowsSign(ClipFor(SetKind::Kid, action).group) && !ShowsSign(ClipFor(SetKind::Head, action).group);
        Check(kids, "no visitor or ride head clip shows the sign");
    }
    // SignTriangles on a synthetic staff model: a body, a broom of its own (as the handyman's), then the sign's node: the
    // board (two triangles each way) and its stick, whose vertices follow the board's, the stick's triangles listed
    // first (as the packs list them); then another broom-textured part that does not follow the board.
    {
        const std::vector<std::string> textures = {"body", "broom", "plackard"};
        auto tri = [](uint16_t a, uint16_t b, uint16_t c, uint16_t texture) {
            tpw::Ps2PackTriangle t;
            t.vertex[0] = a, t.vertex[1] = b, t.vertex[2] = c;
            t.texture = texture;
            return t;
        };
        const std::vector<tpw::Ps2PackTriangle> tris = {
            tri(0, 1, 2, 0),                     // 0 body
            tri(3, 4, 5, 1),  tri(4, 5, 6, 1),   // 1, 2 the broom of its own
            tri(11, 12, 13, 1), tri(12, 13, 14, 1), tri(13, 14, 11, 1),   // 3 to 5 the stick (vertices 11 to 14)
            tri(7, 8, 9, 2),  tri(8, 9, 10, 2),  tri(9, 8, 7, 2), tri(10, 9, 8, 2),   // 6 to 9 the board (7 to 10)
            tri(15, 16, 17, 1),                  // 10 another broom-textured part
            tri(18, 19, 20, 0),                  // 11 body
        };
        const std::vector<uint8_t> sign = SignTriangles(tris.data(), tris.size(), textures);
        const std::vector<uint8_t> want = {0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0};
        Check(sign == want, "SignTriangles: the board and its stick, not the body or another broom");
        const std::vector<std::string> plain = {"body", "broom", "cloth"};
        const std::vector<uint8_t> none = SignTriangles(tris.data(), tris.size(), plain);
        Check(none.size() == tris.size() && std::count(none.begin(), none.end(), 1) == 0,
              "SignTriangles: a model without a board has no sign");
        Check(SignTriangles(tris.data(), 0, textures).empty(), "SignTriangles: no triangles");
    }
    Check(ClipFor(SetKind::Kid, 4).group == 5 && ClipFor(SetKind::Kid, 4).time == ClipTime::Once &&
              ClipFor(SetKind::Kid, 6).group == 4,
          "a visitor's actions read the kids' table");

    // The hash: the same key the same answer, keys next to each other far apart.
    Check(RenderHash(12345) == RenderHash(12345), "the hash is a function of its key");
    int differ = 0;
    for (uint64_t k = 0; k < 64; ++k) differ += (RenderHash(k) % 6) != (RenderHash(k + 1) % 6) ? 1 : 0;
    Check(differ > 32, "neighbouring keys pick different idles (" + std::to_string(differ) + " of 64)");

    // A pack's clips and frames.
    tpw::Ps2Pack two = MakePack(2, 3, [](int f, uint32_t v, float* p) {
        p[0] = static_cast<float>(f);
        p[1] = static_cast<float>(v);
        p[2] = 0.5f;
    });
    tpw::Ps2PackClip second = two.clips[0];
    second.group = 2;
    second.clip = 3;
    two.clips.push_back(second);
    Check(FindClip(two, 0, 0) == &two.clips[0] && FindClip(two, 2, 3) == &two.clips[1] && FindClip(two, 2, 0) == nullptr,
          "FindClip finds a clip by group and number");
    float p[3];
    FramePosition(two, two.clips[0], 2, 1, p);
    Check(Near(p[0], 2) && Near(p[1], 1) && Near(p[2], 0.5f), "FramePosition reads frame and vertex, times the scale");

    // A walk in place: a body above, a foot that is the lowest point and moves backwards (-y) while it is down, then
    // lifts and comes forwards above another foot that takes over.
    tpw::Ps2Pack walk = MakePack(4, 8, [](int f, uint32_t v, float* q) {
        const bool firstDown = f < 4;
        const float back = -0.5f * static_cast<float>(f % 4);
        switch (v) {
        case 0: q[0] = -1, q[1] = 0, q[2] = 10; break;   // the body
        case 1: q[0] = 1, q[1] = 0, q[2] = 6; break;
        case 2: q[0] = -0.3f, q[1] = firstDown ? 1 + back : 0, q[2] = firstDown ? 0 : 1; break;   // the feet
        default: q[0] = 0.3f, q[1] = firstDown ? 0 : 1 + back, q[2] = firstDown ? 1 : 0; break;
        }
    });
    const Anchor anchor = AnchorOf(walk, walk.clips[0]);
    Check(Near(anchor.cx, 0) && Near(anchor.cy, 0.5f) && Near(anchor.feet, 0) && Near(anchor.height, 10),
          "AnchorOf: the first frame's footprint middle, its lowest point and its height");

    // CardHeight: a picture's height as a card facing the screen, for views of scale 1 and 2, either edge first.
    Check(Near(CardHeight(1.5f, 3.5f, 0.0f, 1.0f, 0.0f), 2.0f), "CardHeight: a view of scale 1");
    Check(Near(CardHeight(3.5f, 1.5f, 0.0f, -1.2f, 1.6f), 1.0f), "CardHeight: a view of scale 2, edges either way");
    Check(CardHeight(1.0f, 2.0f, 0.0f, 0.0f, 0.0f) == 0.0f, "CardHeight: no answer for a view without scale");

    // HeadPartOf: a synthetic figure of square rings, 1 unit apart from z 0 to 20: a body 4 wide up to z 13, a neck
    // 1 wide at z 14 and 15, a head 3 wide from z 16 to 20, every pair of neighbouring rings joined by two triangles a
    // side. The head is the triangles above the neck (z 14 and 15 are the narrowest cross-sections, 15 the higher).
    {
        std::vector<float> p;
        std::vector<tpw::Ps2PackTriangle> tris;
        auto width = [](int z) { return z <= 13 ? 4.0f : z <= 15 ? 1.0f : 3.0f; };
        for (int z = 0; z <= 20; ++z) {
            const float h = width(z) / 2;
            const float corners[4][2] = {{-h, -h}, {h, -h}, {h, h}, {-h, h}};
            for (const auto& c : corners) p.insert(p.end(), {c[0] + 10.0f, c[1] - 5.0f, static_cast<float>(z)});
        }
        for (int z = 0; z < 20; ++z)
            for (int k = 0; k < 4; ++k) {
                const auto a0 = static_cast<uint16_t>(z * 4 + k), a1 = static_cast<uint16_t>(z * 4 + (k + 1) % 4);
                const auto b0 = static_cast<uint16_t>(a0 + 4), b1 = static_cast<uint16_t>(a1 + 4);
                tpw::Ps2PackTriangle t1, t2;
                t1.vertex[0] = a0, t1.vertex[1] = a1, t1.vertex[2] = b1;
                t2.vertex[0] = a0, t2.vertex[1] = b1, t2.vertex[2] = b0;
                tris.push_back(t1);
                tris.push_back(t2);
            }
        const std::vector<std::string> plain = {"body"};
        const HeadPart head = HeadPartOf(p.data(), static_cast<uint32_t>(p.size() / 3), tris.data(), tris.size(), plain);
        Check(Near(head.neck, 15.0f), "HeadPartOf: the neck is the top of the narrowest place");
        bool above = !head.triangles.empty();
        for (uint32_t t : head.triangles)
            for (uint16_t v : tris[t].vertex) above = above && p[v * 3 + 2] >= head.neck;
        Check(above && head.triangles.size() == (20 - 15) * 8, "HeadPartOf: the head is every triangle above it");
        Check(Near(head.cx, 10.0f) && Near(head.cy, -5.0f) && Near(head.cz, 17.5f) && Near(head.height, 5.0f),
              "HeadPartOf: the head's middle and height");
        const HeadPart flat = HeadPartOf(p.data(), 4, tris.data(), 0, plain);
        Check(flat.triangles.empty(), "HeadPartOf: a flat frame has no head");

        // With a head texture (any case): the head-textured bands from z 15 to 19 and hair hanging from 12 to 13, a
        // hat (the body's texture) from 19 to 20, above the head-textured corners' median; the neck's bands (13 to 15,
        // the body's texture) are below it.
        const std::vector<std::string> named = {"BoyBody1", "HeadB01"};
        for (size_t t = 0; t < tris.size(); ++t) {
            const int band = static_cast<int>(t / 8);
            tris[t].texture = static_cast<uint16_t>((band >= 15 && band < 19) || band == 12 ? 1 : 0);
        }
        const HeadPart textured = HeadPartOf(p.data(), static_cast<uint32_t>(p.size() / 3), tris.data(), tris.size(), named);
        bool bands = textured.triangles.size() == 6 * 8;
        for (uint32_t t : textured.triangles) bands = bands && (t / 8 >= 15 || t / 8 == 12);
        Check(bands, "HeadPartOf: the head-textured triangles and the others above their middle");
        Check(Near(textured.neck, 12.0f) && Near(textured.cz, 16.0f) && Near(textured.height, 8.0f),
              "HeadPartOf: the textured head's neck, middle and height");
        Check(detail::IsHeadTexture("junglehead") && detail::IsHeadTexture("HEAD01_N") && !detail::IsHeadTexture("hea") &&
                  !detail::IsHeadTexture("boybody1"),
              "IsHeadTexture: a name with \"head\" in any case");
    }

    // ReadTga: a 2 x 2 24-bit picture from the bottom row up, the same from the top, a 32-bit one and what is refused.
    const auto tga = [](uint8_t bits, uint8_t descriptor, const std::vector<uint8_t>& texels, uint8_t type = 2) {
        std::vector<uint8_t> file = {0, 0, type, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 2, 0, bits, descriptor};
        file.insert(file.end(), texels.begin(), texels.end());
        return file;
    };
    const std::vector<uint8_t> bottomUp = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};   // rows: bottom (1..6), top (7..12)
    int32_t width = 0, height = 0, bytes = 0;
    std::string pixels;
    std::vector<uint8_t> file = tga(24, 0, bottomUp);
    Check(ReadTga(file.data(), file.size(), width, height, bytes, pixels) && width == 2 && height == 2 && bytes == 3 &&
              pixels == std::string("\x07\x08\x09\x0a\x0b\x0c\x01\x02\x03\x04\x05\x06", 12),
          "ReadTga: a picture stored from the bottom comes out from the top");
    file = tga(24, 0x20, bottomUp);
    Check(ReadTga(file.data(), file.size(), width, height, bytes, pixels) &&
              pixels == std::string("\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c", 12),
          "ReadTga: a picture stored from the top stays in its order");
    std::vector<uint8_t> four(16);
    for (size_t i = 0; i < four.size(); ++i) four[i] = static_cast<uint8_t>(i);
    file = tga(32, 0x28, four);
    Check(ReadTga(file.data(), file.size(), width, height, bytes, pixels) && bytes == 4 && pixels.size() == 16 &&
              static_cast<uint8_t>(pixels[15]) == 15,
          "ReadTga: a 32-bit picture keeps its alpha");
    file = tga(24, 0, bottomUp, 10);
    Check(!ReadTga(file.data(), file.size(), width, height, bytes, pixels), "ReadTga: a packed TGA is refused");
    file = tga(16, 0, bottomUp);
    Check(!ReadTga(file.data(), file.size(), width, height, bytes, pixels), "ReadTga: a 16-bit TGA is refused");
    file = tga(24, 0, std::vector<uint8_t>(bottomUp.begin(), bottomUp.end() - 1));
    Check(!ReadTga(file.data(), file.size(), width, height, bytes, pixels), "ReadTga: a short file is refused");

    // Brighten: the colour times the gain, rounded and held at 255; a 32-bit texel's alpha untouched.
    std::string lit("\x10\x64\xc8\x80" "\x00\xff\x01\x7f", 8);
    Brighten(lit, 4, 1.5f);
    Check(lit == std::string("\x18\x96\xff\x80" "\x00\xff\x02\x7f", 8), "Brighten: colour times 1.5, alpha kept");
    std::string flat("\x10\x20\x30\x40\x50\x60", 6);
    Brighten(flat, 3, 1.0f);
    Check(flat == std::string("\x10\x20\x30\x40\x50\x60", 6), "Brighten: a gain of 1 changes nothing");

    // NearestCard: a kid stays a sprite under 0.02 of the screen, a ride's head under 0.01; in first person both 0.01.
    Check(NearestCard(SetKind::Kid, false) == 0.02f && NearestCard(SetKind::Staff, false) == 0.02f,
          "NearestCard: a kid or staff member 0.02 in the park view");
    Check(NearestCard(SetKind::Head, false) == 0.01f && NearestCard(SetKind::Head, true) == 0.01f,
          "NearestCard: a ride's head 0.01");
    Check(NearestCard(SetKind::Kid, true) == 0.01f && NearestCard(SetKind::Staff, true) == 0.01f,
          "NearestCard: a kid or staff member 0.01 in first person");

    std::printf("%s\n", gFailures == 0 ? "all ok" : "FAILURES");
    return gFailures == 0 ? 0 : 1;
}
