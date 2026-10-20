// DrawObjectLibrary::FindDrawObject, main 0x101cd2d4 (name, 160 bytes), 0x101cd3b0 (hash, 124); existing bodies, unit w43.
#include "ui_DrawObject.h"
#include "UI_Base.h"

InterfaceDrawObject* DrawObjectLibrary::FindDrawObject(char* name) {
    InterfaceDrawObject* found = nullptr;
    bool done = false;
    Node* node = gDrawObjectLibrary;
    const uint32_t hash = UI_hash_cakes(name, 0x2f);
    while (node != nullptr && !done) {
        if (node->mHash == hash) {
            done = true;
            found = node->mObject;
        }
        node = node->mNext;
    }
    return found != nullptr ? found->InstanceMe() : nullptr;
}

InterfaceDrawObject* DrawObjectLibrary::FindDrawObject(uint32_t hash) {
    InterfaceDrawObject* found = nullptr;
    bool done = false;
    Node* node = gDrawObjectLibrary;
    while (node != nullptr && !done) {
        if (hash == node->mHash) {
            done = true;
            found = node->mObject;
        }
        node = node->mNext;
    }
    return found != nullptr ? found->InstanceMe() : nullptr;
}
