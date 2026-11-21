#include "mod/VRCraftMod.hpp"

#include "camera/VRCameraController.hpp"
#include "gallery/VideoGallery.hpp"
#include "media/MediaCodecRecorder.hpp"
#include "media/VRExporter.hpp"
#include "render/EGLRenderPipeline.hpp"
#include "render/MinecraftRenderBridge.hpp"
#include "ui/VRCraftUI.hpp"

#include <filesystem>

namespace vrcraft {

VRCraftMod& VRCraftMod::instance() {
    static VRCraftMod sInstance;
    return sInstance;
}

VRCraftMod::VRCraftMod() : mSelf(*ll::mod::NativeMod::current()) {}

VRCraftMod::~VRCraftMod() = default;

bool VRCraftMod::load() {
    auto& logger = getSelf().getLogger();
    logger.info("Loading {} v{}", getSelf().getName(), getSelf().getVersion());

    std::error_code ec;
    std::filesystem::create_directories(getSelf().getConfigDir(), ec);
    std::filesystem::create_directories(getSelf().getDataDir(), ec);

    mConfig.emplace();
    if (!mConfig->load()) {
        logger.warn("Failed to load config; using defaults");
        return false;
    }

    const auto recordingsDir = getSelf().getDataDir() / "recordings";
    std::filesystem::create_directories(recordingsDir, ec);

    mCamera       = std::make_unique<VRCameraController>();
    mRenderBridge = std::make_unique<MinecraftRenderBridge>();
    mEglPipeline  = std::make_unique<EGLRenderPipeline>();
    mRecorder     = std::make_unique<MediaCodecRecorder>();
    mExporter     = std::make_unique<VRExporter>();
    mGallery      = std::make_unique<VideoGallery>(recordingsDir, getSelf().getJavaVM());
    mUi           = std::make_unique<VRCraftUI>(*this);

    mCamera->applySettings(mConfig->value().camera);
    mRecorder->probeDeviceCapabilities();
    mGallery->refresh();

    logger.info(
        "VRCraft 360° loaded. H.264={}, H.265={}, MaxWidth={}, MaxFPS={}",
        mRecorder->capabilities().supportsH264,
        mRecorder->capabilities().supportsH265,
        mRecorder->capabilities().maxSupportedWidth,
        mRecorder->capabilities().maxSupportedFps
    );
    return true;
}

bool VRCraftMod::enable() {
    auto& logger = getSelf().getLogger();
    if (!mConfig || !mConfig->value().enabled) {
        logger.info("VRCraft 360° is disabled in config.");
        return true;
    }

    const bool hooked = mRenderBridge->installHooks();
    if (!hooked) {
        logger.warn(
            "Minecraft 1.26.66.1 RenderDragon/Camera hooks are not yet verified/installed ({}). "
            "Live 360°/stereo world capture is locked until verified signatures are supplied.",
            mRenderBridge->statusReason()
        );
    }

    if (!mUi->registerMenuAndButtons()) {
        logger.error("Failed to register VRCraft ModMenu UI.");
        return false;
    }

    mUi->refreshHud();
    return true;
}

bool VRCraftMod::disable() {
    if (mRecorder && mRecorder->isRecording()) {
        mRecorder->stop();
    }
    if (mExporter && mExporter->isExporting()) {
        mExporter->cancel();
    }
    if (mUi) {
        mUi->unregisterMenuAndButtons();
    }
    if (mEglPipeline) {
        mEglPipeline->shutdown();
    }
    if (mRenderBridge) {
        mRenderBridge->removeHooks();
    }
    saveConfig();
    return true;
}

bool VRCraftMod::unload() {
    mUi.reset();
    mGallery.reset();
    mExporter.reset();
    mRecorder.reset();
    mEglPipeline.reset();
    mRenderBridge.reset();
    mCamera.reset();
    return true;
}

void VRCraftMod::saveConfig() {
    if (mConfig) {
        mConfig->save();
    }
}

} // namespace vrcraft
