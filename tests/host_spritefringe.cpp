// Synthetic sprite overlap, with the engine's actual pass state and a tiny software framebuffer.
// No game assets, graphics context or window. The unfixed build records the original depth cut-outs.
//
// The scene is one row of 8 pixels. Opaque scenery is already drawn (pixel 4 in front of every
// sprite, pixel 5 behind them). Four people are sprites on two texture pages, and a translucent
// pane of park scenery is a deferred primitive between them. As in CRenderMemory::
// ExecuteUnrenderedPrimitives, the deferred primitives draw farthest first by their average depth.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <vector>

#ifndef TPW_HOST_SPRITEFRINGE_FIXED
#define TPW_HOST_SPRITEFRINGE_FIXED 0
#endif
#if TPW_HOST_SPRITEFRINGE_FIXED
#include "PortSpriteFringe.h"
#define SPRITEFRINGE_FIXED 1
#else
#include "platform/opengl.h"
#define SPRITEFRINGE_FIXED 0
#endif

namespace {
int checks = 0, failures = 0;
void Check(bool good, const char* message) {
    ++checks;
    if (!good) { ++failures; std::fprintf(stderr, "spritefringe: FAIL: %s\n", message); }
}
struct Colour { float r, g, b; };
struct Pixel { Colour colour{}; float depth = 1.0f; };
struct State {
    bool alpha = true, depth = true, write = true, blend = true;
    GLenum alphaFunc = GL_GREATER;
    float alphaRef = 0;
};
State state;
bool Same(const State& a, const State& b) {
    return a.alpha == b.alpha && a.depth == b.depth && a.write == b.write &&
        a.blend == b.blend && a.alphaFunc == b.alphaFunc && a.alphaRef == b.alphaRef;
}
bool Near(float a, float b) { return std::fabs(a - b) < 0.00001f; }
void Expect(const Pixel& p, Colour colour, float depth, const char* message) {
    const bool good = Near(p.colour.r, colour.r) && Near(p.colour.g, colour.g) && Near(p.colour.b, colour.b) &&
        Near(p.depth, depth);
    Check(good, message);
    if (!good)
        std::fprintf(stderr, "  actual RGB %.3f %.3f %.3f depth %.3f; expected %.3f %.3f %.3f depth %.3f\n",
                     p.colour.r, p.colour.g, p.colour.b, p.depth, colour.r, colour.g, colour.b, depth);
}
using Picture = std::array<Pixel, 8>;
struct Sprite { Colour colour; float depth; std::array<float, 8> alpha; };
// Larger depth is farther, as in SSpriteGameData::mDepth and SPrimitive::mDepth.
const Sprite front{{1, 0, 0}, .3f, {0, .25f, 1, .5f, .5f, .5f, 0, 1}};
const Sprite rear{{0, 0, 1}, .7f, {1, 1, 1, .5f, 1, 1, .5f, 0}};
const Sprite farther{{1, 1, 0}, .8f, {0, 0, 0, 0, 0, 0, 1, 0}};
// Only behind the front scenery; it moves its page's average depth.
const Sprite nearest{{1, 1, 1}, .15f, {0, 0, 0, 0, 1, 0, 0, 0}};
// The translucent pane: blended, alpha tested, depth tested, no depth writes (mode 0xd56).
const Sprite pane{{0, 1, 0}, .5f, {0, 0, 0, 0, 0, 0, .5f, 0}};
constexpr uint32_t kSpriteMode = 0x556, kPaneMode = 0xd56;

void Raster(Picture& image, const Sprite& sprite) {
    for (size_t x = 0; x < image.size(); ++x) {
        const float a = sprite.alpha[x];
        bool accepted = true;
        if (state.alpha) {
            if (state.alphaFunc == GL_GREATER) accepted = a > state.alphaRef;
            else if (state.alphaFunc == GL_GEQUAL) accepted = a >= state.alphaRef;
            else if (state.alphaFunc == 0x0201) accepted = a < state.alphaRef;
            else { Check(false, "supported alpha function"); accepted = false; }
        }
        Pixel& p = image[x];
        if (!accepted || (state.depth && sprite.depth > p.depth)) continue;
        if (state.blend) {
            p.colour = {sprite.colour.r * a + p.colour.r * (1 - a),
                        sprite.colour.g * a + p.colour.g * (1 - a),
                        sprite.colour.b * a + p.colour.b * (1 - a)};
        } else p.colour = sprite.colour;
        if (state.depth && state.write) p.depth = sprite.depth;
    }
}

// A deferred primitive as the engine builds it: depth is the running sum of its vertices' depths
// (four per sprite) until it is finished, then their average.
struct Primitive {
    uint32_t mFlags = 0, mode = kSpriteMode;
    float mDepth = 0;
    int32_t mVertexCount = 0;
    std::vector<const Sprite*> sprites;
};
void Add(Primitive& primitive, const Sprite& sprite) {
    primitive.sprites.push_back(&sprite);
    primitive.mDepth += 4 * sprite.depth;
    primitive.mVertexCount += 4;
}
void Finish(Primitive& primitive, bool faceSort) {
    primitive.mDepth /= static_cast<float>(primitive.mVertexCount);
    // ExecuteCurrentPrimitive's face sort draws a primitive's own sprites farthest first.
    if (faceSort) std::stable_sort(primitive.sprites.begin(), primitive.sprites.end(),
                                   [](const Sprite* a, const Sprite* b) { return a->depth > b->depth; });
}

// COpenGLDrawPrimitive::Execute and DrawIndexedPrimitive for one primitive.
void Execute(Picture& image, const Primitive& primitive) {
    // SetBlendingMode: 0x800 turns depth writes off.
    state = State{};
    state.write = (primitive.mode & 0x800) == 0;
    const State selected = state;
    {
#if SPRITEFRINGE_FIXED
        tpw::spritefringe::Scope pass((primitive.mFlags & tpw::spritefringe::kCore) != 0 ?
            tpw::spritefringe::Pass::Core : (primitive.mFlags & tpw::spritefringe::kFringe) != 0 ?
            tpw::spritefringe::Pass::Fringe : tpw::spritefringe::Pass::Original);
        tpw::spritefringe::DrawState draw(primitive.mode);
        if (!draw.skip) for (const Sprite* sprite : primitive.sprites) Raster(image, *sprite);
#else
        for (const Sprite* sprite : primitive.sprites) Raster(image, *sprite);
#endif
    }
    Check(Same(state, selected), "draw restores all alpha/depth/blend state");
}

using Pages = std::array<std::vector<const Sprite*>, 2>;
Picture Render(const Pages& pages, bool reverseTies) {
    Picture image{};
    // Park scenery already drawn, in front of and behind the sprites.
    image[4] = {{0, 1, 0}, .1f};
    image[5] = {{0, 1, 0}, .9f};
    std::vector<Primitive> deferred;
    deferred.reserve(16);
    for (const auto& page : pages) {
        // CSpriteManager::RenderSprites sorts each page's group farthest first.
        auto group = page;
        std::stable_sort(group.begin(), group.end(), [](const Sprite* a, const Sprite* b) { return a->depth > b->depth; });
        deferred.emplace_back();
        Primitive& primitive = deferred.back();
#if SPRITEFRINGE_FIXED
        primitive.mFlags = tpw::spritefringe::kCore;
        for (const Sprite* sprite : group) {
            const Primitive* before = primitive.mVertexCount != 0 ? &primitive : nullptr;
            const float depthBefore = primitive.mDepth;
            const int32_t countBefore = primitive.mVertexCount;
            Add(primitive, *sprite);
            tpw::spritefringe::KeepFarthest(&primitive, before, depthBefore, countBefore);
        }
#else
        for (const Sprite* sprite : group) Add(primitive, *sprite);
#endif
        Finish(primitive, true);
    }
#if SPRITEFRINGE_FIXED
    // The fringe pass: every page's sprites farthest first, a new primitive at each change of page.
    struct Entry { const Sprite* sprite; size_t page; };
    std::vector<Entry> all;
    for (size_t page = 0; page < pages.size(); ++page)
        for (const Sprite* sprite : pages[page]) all.push_back({sprite, page});
    tpw::spritefringe::SortFringes(all.begin(), all.end(), [](const Entry& entry) { return entry.sprite->depth; });
    size_t currentPage = pages.size();
    for (const Entry& entry : all) {
        if (entry.page != currentPage) {
            if (currentPage != pages.size()) Finish(deferred.back(), false);
            deferred.emplace_back();
            deferred.back().mFlags = tpw::spritefringe::kFringe;
            currentPage = entry.page;
        }
        Add(deferred.back(), *entry.sprite);
    }
    if (currentPage != pages.size()) Finish(deferred.back(), false);
#endif
    deferred.emplace_back();
    deferred.back().mode = kPaneMode;
    Add(deferred.back(), pane);
    Finish(deferred.back(), true);
    // The render memory's depth sort, farthest first; its order at equal depths is not defined.
    if (reverseTies) std::reverse(deferred.begin(), deferred.end());
    std::stable_sort(deferred.begin(), deferred.end(),
                     [](const Primitive& a, const Primitive& b) { return a.mDepth > b.mDepth; });
    for (const Primitive& primitive : deferred) Execute(image, primitive);
#if SPRITEFRINGE_FIXED
    Check(tpw::spritefringe::pass == tpw::spritefringe::Pass::Original, "pass scopes restore the original pass");
#endif
    return image;
}
}  // namespace

extern "C" {
void glAlphaFunc(GLenum func, GLfloat ref) { state.alphaFunc = func; state.alphaRef = ref; }
void glDepthMask(uint8_t write) { state.write = write != 0; }
}

int main() {
#if SPRITEFRINGE_FIXED
    struct SortItem { int id; float depth; };
    const float nan = std::numeric_limits<float>::quiet_NaN();
    std::array<SortItem, 7> ordering{{{0, nan}, {1, .3f}, {2, .7f}, {3, .7f}, {4, nan}, {5, -.1f}, {6, .3f}}};
    tpw::spritefringe::SortFringes(ordering.begin(), ordering.end(), [](const SortItem& item) { return item.depth; });
    const std::array<int, 7> expected{{2, 3, 1, 6, 5, 0, 4}};
    for (size_t i = 0; i < ordering.size(); ++i)
        Check(ordering[i].id == expected[i], "sort is descending, stable at equal depths, and NaNs are last and stable");

    // KeepFarthest keeps a core primitive's sum at its farthest sprite times its vertex count.
    struct Sum { float mDepth = 0; int32_t mVertexCount = 0; };
    Sum first, second;
    auto add = [](Sum& primitive, Sum* before, float depth, int32_t vertices) {
        const float depthBefore = before != nullptr ? before->mDepth : 0.0f;
        const int32_t countBefore = before != nullptr ? before->mVertexCount : 0;
        primitive.mDepth += depth * static_cast<float>(vertices);
        primitive.mVertexCount += vertices;
        tpw::spritefringe::KeepFarthest(&primitive, before, depthBefore, countBefore);
        return primitive.mDepth / static_cast<float>(primitive.mVertexCount);
    };
    Check(Near(add(first, nullptr, .4f, 4), .4f), "a fresh core primitive starts at its sprite's depth");
    Check(Near(add(first, &first, .9f, 4), .9f), "a farther sprite moves the core to its depth");
    Check(Near(add(first, &first, .2f, 4), .9f), "a nearer sprite leaves the core at the farthest");
    Check(Near(add(first, &first, .2f, 2), .9f), "a clipped sprite's vertex count is followed");
    Check(Near(add(second, &first, .3f, 4), .3f), "a primitive after another starts afresh");
    Check(Near(add(second, &second, .1f, 4), .3f), "and keeps its own farthest sprite");
    const float kept = first.mDepth;
    tpw::spritefringe::KeepFarthest(&first, &first, first.mDepth, first.mVertexCount);
    Check(kept == first.mDepth, "a sprite that added no vertices changes nothing");

    Check(tpw::spritefringe::Eligible(0x556), "ordinary world sprites eligible");
    Check(tpw::spritefringe::Eligible(0x100556) && tpw::spritefringe::Eligible(0x80556), "fogged sprites eligible");
    Check(!tpw::spritefringe::Eligible(0x552), "additive blend is excluded");
    Check(!tpw::spritefringe::Eligible(0xd56), "no-depth-write primitives excluded");
    Check(!tpw::spritefringe::Eligible(0x8556), "no-depth-test sprites excluded");
    Check(!tpw::spritefringe::Eligible(0x2756), "one-bit alpha sprites excluded");
    // What SetBlendingMode selects for 0x556, and a state no eligible mode has.
    const State selected{};
    const State unusual{false, false, false, false, GL_GEQUAL, .37f};
    state = unusual;
    {
        tpw::spritefringe::DrawState draw(0x556);
        Check(Same(state, unusual) && !draw.skip && draw.secondPass, "the original pass changes nothing");
    }
    Check(Same(state, unusual), "the original pass restores nothing");
    {
        tpw::spritefringe::Scope outer(tpw::spritefringe::Pass::Core);
        state = selected;
        {
            tpw::spritefringe::DrawState draw(0x556);
            Check(state.alpha && state.depth && state.write && state.alphaFunc == GL_GEQUAL &&
                  Near(state.alphaRef, 1), "core alpha and depth state");
            Check(state.blend, "draw state preserves selected blending");
            Check(!draw.skip && !draw.secondPass, "cores draw without the fog's second pass");
        }
        Check(Same(state, selected), "core state restored to the selected mode's");
        state = unusual;
        {
            tpw::spritefringe::DrawState draw(0xd56);
            Check(Same(state, unusual) && !draw.skip && draw.secondPass, "core pass leaves other modes alone");
        }
        Check(Same(state, unusual), "and restores nothing for them");
        {
            tpw::spritefringe::Scope inner(tpw::spritefringe::Pass::Fringe);
            state = selected;
            {
                tpw::spritefringe::DrawState draw(0x80556);
                Check(state.alpha && state.depth && !state.write && state.alphaFunc == 0x0201 &&
                      Near(state.alphaRef, 1) && state.blend, "fringe alpha and depth state");
                Check(!draw.skip && draw.secondPass, "fringes draw the fog's second pass");
            }
            Check(Same(state, selected), "fringe state restored to the selected mode's");
            state = unusual;
            {
                tpw::spritefringe::DrawState draw(0x552);
                Check(draw.skip && Same(state, unusual), "fringe pass skips other modes");
            }
            Check(Same(state, unusual), "and leaves their state alone");
        }
        Check(tpw::spritefringe::pass == tpw::spritefringe::Pass::Core, "nested pass restored");
    }
    Check(tpw::spritefringe::pass == tpw::spritefringe::Pass::Original, "outer pass restored");
#endif
    // Three ways the four people can fall on two texture pages.
    const std::array<Pages, 3> groupings{{
        {{{&front, &farther}, {&rear}}},
        {{{&front}, {&rear, &farther}}},
        {{{&front, &farther}, {&rear, &nearest}}},
    }};
    for (size_t g = 0; g < groupings.size(); ++g) {
        for (bool reverseTies : {false, true}) {
            const Picture image = Render(groupings[g], reverseTies);
            Expect(image[0], {0, 0, 1}, .7f, "alpha zero neither hides nor writes depth");
            Expect(image[2], {1, 0, 0}, .3f, "front opaque core hides rear core");
            Expect(image[4], {0, 1, 0}, .1f, "scenery in front hides every sprite");
            Expect(image[7], {1, 0, 0}, .3f, "rear alpha zero preserves front core");
#if SPRITEFRINGE_FIXED
            // The same picture whatever the pages: every pixel composited back to front.
            Expect(image[1], {.25f, 0, .75f}, .7f, "front soft edge blends over rear opaque sprite");
            Expect(image[3], {.5f, 0, .25f}, 1, "soft edges blend back to front across texture pages");
            Expect(image[5], {.5f, 0, .5f}, .7f, "sprites hide scenery behind without soft-edge depth writes");
            Expect(image[6], {.25f, .75f, .25f}, .8f, "pane over a soft edge over a farther person on another page");
#else
            if (g == 0) {
                // Page 0 (average .55) draws after page 1 (.7): the rear soft edge cuts the far person out.
                Expect(image[1], {.25f, 0, .75f}, .3f, "original rear page first composites the soft edge");
                Expect(image[3], {.5f, 0, .25f}, .3f, "original rear page first composites both soft edges");
                Expect(image[5], {.5f, 0, .5f}, .3f, "original rear page first hides scenery behind");
                Expect(image[6], {0, .5f, .25f}, .7f, "original rear soft edge cuts out the far person");
            } else if (g == 1) {
                Expect(image[1], {.25f, 0, .75f}, .3f, "original page order happens to composite");
                Expect(image[3], {.5f, 0, .25f}, .3f, "original page order happens to composite soft edges");
                Expect(image[5], {.5f, 0, .5f}, .3f, "original page order hides scenery behind");
                Expect(image[6], {.25f, .75f, .25f}, .7f, "original far person behind the rear soft edge");
            } else {
                // Page 0 (average .55) draws before page 1 (.425): the front soft edges cut the rear person out.
                Expect(image[1], {.25f, 0, 0}, .3f, "original front soft edge cuts out rear opaque sprite");
                Expect(image[3], {.5f, 0, 0}, .3f, "original front soft edge cuts out rear soft edge");
                Expect(image[5], {.5f, .5f, 0}, .3f, "original cut-out reveals scenery behind");
                Expect(image[6], {.25f, .5f, .5f}, .7f, "original rear soft edge over the pane in front of it");
            }
#endif
        }
    }
    std::printf("spritefringe %s: %d checks, %d failures\n", SPRITEFRINGE_FIXED ? "fixed" : "original", checks, failures);
    return failures ? 1 : 0;
}
