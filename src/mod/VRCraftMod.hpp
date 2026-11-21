#pragma once

#include "mod/Config.hpp"

#include <memory>
#include <optional>
#include <pl/Config.hpp>
#include <pl/Mod.hpp>

namespace vrcraft {

class VRCameraController;
class MinecraftRenderBridge;
class EGLRenderPipeline;
class MediaCodecRecorder;
class VRExporter;
class VideoGallery;
class VRCraftUI;

class VRCraftMod {
public:
    static VRCraftMod& instance();

    VRCraftMod();
    ~VRCraftMod();

    VRCraftMod(const VRCraftMod&)            = delete;
    VRCraftMod& operator=(const VRCraftMod&) = delete;

    bool load();
    bool enable();
    bool disable();
    bool unload();

    [[nodiscard]] ll::mod::NativeMod& getSelf() const { return mSelf; }
    [[nodiscard]] VRCraftConfig&      config() { return mConfig->value(); }
    [[nodiscard]] const VRCraftConfig& config() const { return mConfig->value(); }
    void                              saveConfig();

    [[nodiscard]] VRCameraController&    camera() { return *mCamera; }
    [[nodiscard]] MinecraftRenderBridge& renderBridge() { return *mRenderBridge; }
    [[nodiscard]] EGLRenderPipeline&     eglPipeline() { return *mEglPipeline; }
    [[nodiscard]] MediaCodecRecorder&    recorder() { return *mRecorder; }
    [[nodiscard]] VRExporter&            exporter() { return *mExporter; }
    [[nodiscard]] VideoGallery&          gallery() { return *mGallery; }
    [[nodiscard]] VRCraftUI&             ui() { return *mUi; }

private:
    ll::mod::NativeMod&                            mSelf;
    std::optional<pl::config::ConfigFile<VRCraftConfig>> mConfig;

    std::unique_ptr<VRCameraController>    mCamera;
    std::unique_ptr<MinecraftRenderBridge> mRenderBridge;
    std::unique_ptr<EGLRenderPipeline>     mEglPipeline;
    std::unique_ptr<MediaCodecRecorder>    mRecorder;
    std::unique_ptr<VRExporter>            mExporter;
    std::unique_ptr<VideoGallery>          mGallery;
    std::unique_ptr<VRCraftUI>             mUi;
};

} // namespace vrcraft
