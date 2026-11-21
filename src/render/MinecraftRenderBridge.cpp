#include "render/MinecraftRenderBridge.hpp"

#include <pl/memory/Hook.hpp>
#include <pl/memory/Signature.hpp>

namespace vrcraft {

bool MinecraftRenderBridge::installHooks() {
    // Verified ARM64 signatures for libminecraftpe.so (Minecraft Bedrock 1.26.66.1) are required:
    //   1. LevelRenderer::renderLevel / GameRenderer::renderCurrentDistortedLevel
    //   2. Camera::setViewMatrix / Camera projection override
    //   3. RenderDragon offscreen cubemap FBO / swapchain present hook
    // Because those proprietary 1.26.66.1 signatures are not bundled with preloader-android,
    // we do not guess offsets or fake 360° capture from 2D screenshots.
    mStatus = RenderHookStatus::MissingSignatures1_26_66_1;
    return false;
}

void MinecraftRenderBridge::removeHooks() {
    mStatus = RenderHookStatus::NotInstalled;
}

std::string MinecraftRenderBridge::statusReason() const {
    switch (mStatus) {
        case RenderHookStatus::Installed:
            return "1.26.66.1 RenderDragon & Camera hooks active";
        case RenderHookStatus::MissingSignatures1_26_66_1:
            return "Missing verified libminecraftpe.so 1.26.66.1 ARM64 camera/render signatures";
        case RenderHookStatus::NotInstalled:
        default:
            return "Render hooks not installed";
    }
}

} // namespace vrcraft
