// Host only: UI2DMapDrawOps (include/ui_main.h), the four slots CUI2DMap::UpdateAllDrawRelated calls on CDraw's render
// context. The Mac calls them through the context's vtable (0x100739a4, 0x10073958, 0x1007390c, 0x100738c0 for 8, 16, 24
// and 32 bits); the host's gDrawContextN have no vtable, so each answers an adapter onto its CRenderClassN.
#include "DrawRenderClasses.h"
#include "DrawState.h"
#include "ui_main.h"

namespace {

template <typename RenderClass>
struct RenderClassOps : CUI2DMapDrawOps {
    RenderClass mClass;

    static TbRenderArea* Area(const void* viewPort) { return static_cast<TbRenderArea*>(const_cast<void*>(viewPort)); }
    static const TbRect& Rect(const void* rect) { return *static_cast<const TbRect*>(rect); }
    static const bfl::draw::CColour& Colour(const void* colour) { return *static_cast<const bfl::draw::CColour*>(colour); }

    void Slot10(const void* viewPort, const void* clip, int32_t flags, int32_t x, int32_t y, int32_t length, const void* colour) override {
        mClass.HorizontalLine(Area(viewPort), Rect(clip), static_cast<uint32_t>(flags), x, y, length, Colour(colour));
    }
    void Slot14(const void* viewPort, const void* clip, int32_t flags, int32_t x, int32_t y, int32_t length, const void* colour) override {
        mClass.VerticalLine(Area(viewPort), Rect(clip), static_cast<uint32_t>(flags), x, y, length, Colour(colour));
    }
    void Slot24(const void* viewPort, const void* clip, int32_t flags, const int32_t* rect, const void* colour) override {
        mClass.RectangleOutline(Area(viewPort), Rect(clip), static_cast<uint32_t>(flags), Rect(rect), Colour(colour));
    }
    void Slot28(const void* viewPort, const void* clip, int32_t flags, const int32_t* rect, const void* colour) override {
        mClass.RectangleFilled(Area(viewPort), Rect(clip), static_cast<uint32_t>(flags), Rect(rect), Colour(colour));
    }
};

RenderClassOps<bfl::draw::CRenderClass8> gOps8;
RenderClassOps<bfl::draw::CRenderClass16> gOps16;
RenderClassOps<bfl::draw::CRenderClass24> gOps24;
RenderClassOps<bfl::draw::CRenderClass32> gOps32;

}  // namespace

CUI2DMapDrawOps* UI2DMapDrawOps(void* context) {
    if (context == &bfl::draw::gDrawContext8) return &gOps8;
    if (context == &bfl::draw::gDrawContext16) return &gOps16;
    if (context == &bfl::draw::gDrawContext24) return &gOps24;
    if (context == &bfl::draw::gDrawContext32) return &gOps32;
    return static_cast<CUI2DMapDrawOps*>(context);
}
