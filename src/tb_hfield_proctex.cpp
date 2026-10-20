// CHField::LoadProcTextures (engine 0x1000d96c, 480 bytes).
#include "tb_hfield.h"

#include <new>

#include "platform/samsutils.h"
#include "tb_error.h"
#include "tb_resource_system.h"
#include "tb_resource_texture.h"

int32_t CHField::LoadProcTextures(char* name, int32_t size) {
    if (gei_System.mRenderer != 1 && gei_System.mRenderer != 3) return 1;
    if (SamsUtilities::gLogger == nullptr) SamsUtilities::gLogger = new (std::nothrow) SamsUtilities::ULogManager;
    SamsUtilities::gLogger->SetContext("engine", "tb_hfield.cpp", 0x139, 0x80000000);
    if (SamsUtilities::gLogger == nullptr) SamsUtilities::gLogger = new (std::nothrow) SamsUtilities::ULogManager;
    SamsUtilities::gLogger->Log("Attempting to initialize procedural textures");
    if (name == nullptr) return 1;
    if (mProcTextures != nullptr) {
        delete mProcTextures;
        mProcTextures = nullptr;
    }
    if (mProcTextures == nullptr) {
        mProcTextures = new (std::nothrow) CProcTextureHandler;
        if (mProcTextures == nullptr) {
            gei_ErrorHandler.Push(2, 0xb0754000u);
            return 0;
        }
    }
    if (mProcTextures->Init(gei_TextureSettings.mProcTextureParam, size) == 0) return 0;
    if (!mProcTextures->Load(name)) {
        delete mProcTextures;
        mProcTextures = nullptr;
        return 0;
    }
    return 1;
}
