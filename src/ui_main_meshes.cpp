// ui_main.cpp (main app of SimTheme Park 2.04b3), the part that starts the interface:
// the meshes of the interface (0x10184abc..0x10185640), InitGeneralUI (0x10185b68), the
// clock tick objects (0x10185b08, 0x10187994) and the file's global strings.
//
// Every .md2 file of the "UI" resource becomes a draw object in the library: the frames
// ("!frame", "!f_plain", "!cbut") and the slider ("!slider") under their file name, any
// other mesh under the name of its root node. The meshes of the language's folder follow.
// The first mesh loaded is what puts the "welcome" picture on the screen (CMesh::Init
// steps the loading picture).
#include "ui_main.h"

#include <cstring>

#include "Debug.h"
#include "FS_System.h"
#include "Game.h"
#include "InGame.h"
#include "InterfaceWindow.h"
#include "LbTime.h"
#include "Quickload.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "TPResource.h"
#include "TPString.h"
#include "Text.h"
#include "UIInterface.h"
#include "rsse.h"
#include "ui_textmanager.h"

using bfl::FileSystem::CFileInfo;
using bfl::FileSystem::CFileStorage;

InterfaceTitleTextObject* gTitleText;
CursorTable gCursorTable;
TbDynamicStringTemplate<char> gUIFrameName("!frame");
TbDynamicStringTemplate<char> gUISliderName("!slider");
TbDynamicStringTemplate<char> gUIPlainFrameName("!f_plain");
TbDynamicStringTemplate<char> gUIButtonName("!cbut");
LbTimeClockTickFunction gUIClockTick;

uint32_t clockTickFunction::tickyticky() { return 0; }
uint32_t LbTimeClockTickFunction::tickyticky() { return LbTime_GetClock(); }

// A frame mesh ("!frame", "!f_plain" or "!cbut", whatever the case of the file's name): the
// object goes into the library under the file's name without ".md2". NULL for any other
// file, and when the mesh does not load. (`storage` is not used.)
InterfaceDrawEngineResizableFrame* TPResource_AddFrameMesh(CFileInfo info, CFileStorage&, const TbDynamicStringTemplate<char>& folder) {
    TbDynamicStringTemplate<char> name(info.mName);
    name = name.Left(static_cast<uint32_t>(name.Length() - gTPGlobals.mF42CC.Length()));
    if (!TPStrEq(name, gUIFrameName) && !TPStrEq(name, gUIPlainFrameName)) {
        TbDynamicStringTemplate<char> button("!cbut");
        const bool isButton = TPStrEq(name, button);
        if (!isButton) return nullptr;
    }
    InterfaceDrawEngineResizableFrame* frame = nullptr;
    TbDynamicStringTemplate<char> path;
    path = folder;
    path += info.mName;
    tag_sMesh* mesh = LoadMesh(path.mData, nullptr, 0, 1, 0);
    if (mesh != nullptr) {
        OverrideTexturePaths(mesh->mHeader, 0, 1);
        frame = new InterfaceDrawEngineResizableFrame(mesh);
        frame->mResizable = 1;
        DrawObjectLibrary::AddDrawObject(frame, name.mData);
    }
    return frame;
}

// The slider mesh ("!slider"), in the library under that name.
InterfaceDrawEngineResizableSlider* TPResource_AddSliderMesh(CFileInfo info, CFileStorage&, const TbDynamicStringTemplate<char>& folder) {
    TbDynamicStringTemplate<char> name(info.mName);
    TbDynamicStringTemplate<char> stem(name.Left(static_cast<uint32_t>(name.Length() - gTPGlobals.mF42CC.Length())));
    const bool isSlider = TPStrEq(stem, gUISliderName);
    if (!isSlider) return nullptr;
    InterfaceDrawEngineResizableSlider* slider = nullptr;
    TbDynamicStringTemplate<char> path;
    path = folder;
    path += info.mName;
    tag_sMesh* mesh = LoadMesh(path.mData, nullptr, 0, 1, 0);
    if (mesh != nullptr) {
        OverrideTexturePaths(mesh->mHeader, 0, 1);
        slider = new InterfaceDrawEngineResizableSlider(mesh);
        slider->mResizable = 1;
        static char libraryName[] = "!slider";
        DrawObjectLibrary::AddDrawObject(slider, libraryName);
    }
    return slider;
}

// Any other mesh, in the library under the name of its root node. Files whose name starts
// with '!' and "w_small_shadow.md2" (that exact spelling) are left out.
InterfaceDrawEngineMesh* TPResource_AddMeshDrawObject(CFileInfo info, CFileStorage&, TbDynamicStringTemplate<char> folder) {
    InterfaceDrawEngineMesh* object = nullptr;
    TbDynamicStringTemplate<char> path;
    if (*info.mName == '!') return nullptr;
    path = folder;
    path += info.mName;
    if (std::strcmp(info.mName, "w_small_shadow.md2") == 0) return nullptr;
    tag_sMesh* mesh = LoadMesh(path.mData, nullptr, 0, 1, 0);
    if (mesh != nullptr) {
        OverrideTexturePaths(mesh->mHeader, 0, 1);
        object = new InterfaceDrawEngineMesh(mesh);
        DrawObjectLibrary::AddDrawObject(object, mesh->mHeader->mF78->mName);
    }
    return object;
}

// The meshes of the "UI" resource; the result is how many became draw objects.
int32_t LoadAllUIMeshes() {
    static char file[] = "here2000.txt";
    static char folder[] = "data:ui";
    int32_t count = 0;
    CTPBaseResource* resource = nullptr;
    SetupDirectoryForMeshLoad(file, folder, nullptr);
    if (gGame.mResourceManager->FindResource(gTPGlobals.mF4600, &resource) == 1) {
        CFileInfo info;
        if (resource->FindFirst(gTPGlobals.mF4574, info) == 1) {
            do {
                CFileStorage* storage = resource->pGetCurrentSearchStorage();
                if (TPResource_AddFrameMesh(info, *storage, resource->mPath) != nullptr) ++count;
                else if (TPResource_AddSliderMesh(info, *storage, resource->mPath) != nullptr) ++count;
                else if (TPResource_AddMeshDrawObject(info, *storage, resource->mPath) != nullptr) ++count;
            } while (resource->FindNext(info) != 0);
        }
    }
    return count;
}

// The meshes in the language's folder of the "Langauge" resource, each marked as localised.
// The mesh paths are set up once, from the first file found.
int32_t LoadLocalisedUIMeshes() {
    static char textures[] = "data:ui:textures";
    CTPBaseResource* resource = nullptr;
    if (gGame.mResourceManager->FindResource(gTPGlobals.mF4678, &resource) == 1) {
        CFileInfo info;
        TbDynamicStringTemplate<char> pattern;
        TbDynamicStringTemplate<char> folder;
        bool first = true;
        pattern = gText.GetLanguageName();
        FS_AddDirectorySeparator(pattern);
        pattern += static_cast<const TbStringBase<char>&>(gTPGlobals.mF4574);
        if (resource->FindFirst(pattern, info) == 1) {
            do {
                CFileStorage* storage = resource->pGetCurrentSearchStorage();
                if (first) {
                    folder = resource->mPath;
                    FS_AddDirectorySeparator(folder);
                    folder += gText.GetLanguageName();
                    SetupDirectoryForMeshLoad(info.mName, folder.mData, textures);
                    FS_AddDirectorySeparator(folder);
                    first = false;
                }
                InterfaceDrawEngineMesh* object =
                    TPResource_AddMeshDrawObject(info, *storage, TbDynamicStringTemplate<char>(folder.mData));
                // The original marks the object without looking whether there is one (a write
                // into low memory on the Mac when there is not).
                if (object != nullptr) object->mLocalised = 1;
            } while (resource->FindNext(info) != 0);
        }
    }
    return 0;
}

// Starts the interface: the title text object (made once), the debug font, the UI system
// and its callbacks, the string heap, the UI's heap, the cursors, then every mesh of the
// interface, the title text's style, the help text.
void InitGeneralUI() {
    if (gTitleText == nullptr) gTitleText = new InterfaceTitleTextObject(nullptr);
    gInGameFont.Gen8x8DebugFont(0xff, static_cast<int16_t>(0x4000 / gUIRealScreenWidth), 0,
                                static_cast<int16_t>(0x4800 / gUIRealScreenHeight),
                                static_cast<int16_t>(0x3000 / gUIRealScreenHeight));
    UI_InitSystem(0x80, 0x800, gText.IsHebrew());
    UI_SetClockTickFunction(&gUIClockTick);
    UI_SetPeekCallBack(EffectPeekMsg);
    InterfaceStringHeap::Allocate(0x40, 0x80, 0x10, 0x80);
    HNewHeap(0x100000, 3, 0xd, &gUIHeap);
    gCursorTable.Allocate(0x16);
    InitialiseCOLOURCursors();
    UI_UseCursorTable(&gCursorTable);
    UI_SetStandardCursor(0);
    UI_SetCursor(1);
    UI_HideCursor();
    const int32_t meshes = LoadAllUIMeshes();
    LoadLocalisedUIMeshes();
    TRACE("Loaded %d UI meshes\n", meshes);
    gTitleText->mInLibrary = 1;
    // (The rest of both records is whatever the stack held.)
    DOTextStyle style;
    DOChange change;
    style.mColour[0] = 0xff;
    style.mColour[1] = 0xff;
    style.mColour[2] = 0xff;
    style.mF09 = 0;
    style.mF04 = 1;
    change.mText = &style;
    style.mF05 = 1;
    gTitleText->Change(0xc00, &change);
    InitHelpText();
}

// Three windows the end of the interface deletes. Nothing translated sets them (original names unknown).
InterfaceWindow* gUIGeneralWindowA;   // 0x10380b88
InterfaceWindow* gUIGeneralWindowB;   // 0x10380b84
InterfaceWindow* gUIGeneralWindowC;   // 0x10380b80

// Undoes InitGeneralUI: the cursor table, the UI system, the three windows (A, B, C: the original reaches each destructor
// through the vtable pointer at +0x38 of the Mac's window, slot 2, with the delete flag 1), the string heap, the debug
// font's bitmap and the UI's heap. The title text object, the draw object library and the meshes are left as they are.
void DeInitGeneralUI() {
    gCursorTable.DeAllocate();
    UI_CloseSystem();
    if (gUIGeneralWindowA != nullptr) delete gUIGeneralWindowA;
    if (gUIGeneralWindowB != nullptr) delete gUIGeneralWindowB;
    if (gUIGeneralWindowC != nullptr) delete gUIGeneralWindowC;
    InterfaceStringHeap::DeAllocate();
    gInGameFont.Release();
    HFreeHeap(gUIHeap);
}

// UIInterface::InitGeneral (0x101ef5f8; the class is in the range the inventory files under
// ui_ErrorDisplay.cpp).
void UIInterface::InitGeneral() { InitGeneralUI(); }

// UIInterface::DeInitGeneral (0x101ef64c).
void UIInterface::DeInitGeneral() { DeInitGeneralUI(); }
