#include "ui/VRCraftUI.hpp"

#include "camera/VRCameraController.hpp"
#include "gallery/VideoGallery.hpp"
#include "media/MediaCodecRecorder.hpp"
#include "media/VRExporter.hpp"
#include "mod/VRCraftMod.hpp"
#include "render/MinecraftRenderBridge.hpp"

#include <cstdio>
#include <pl/ModMenu.hpp>
#include <string>
#include <vector>

namespace vrcraft {

namespace {
constexpr const char* kModCameraModule   = "vrcraft360.vr_camera";
constexpr const char* kModRecordModule   = "vrcraft360.live_recording";
constexpr const char* kModGalleryModule  = "vrcraft360.video_gallery";
constexpr const char* kModExportModule   = "vrcraft360.vr_export";
constexpr const char* kModSettingsModule = "vrcraft360.settings";

constexpr const char* kBtnRecordToggle   = "vrcraft360.btn.record";
constexpr const char* kBtnPauseResume    = "vrcraft360.btn.pause_resume";
constexpr const char* kBtnResetCamera    = "vrcraft360.btn.reset_camera";
constexpr const char* kBtnExportLatest   = "vrcraft360.btn.export_latest";
} // namespace

VRCraftUI::VRCraftUI(VRCraftMod& mod) : mMod(mod) {}

bool VRCraftUI::registerMenuAndButtons() {
    if (mRegistered) {
        return true;
    }

    const std::string modId = mMod.getSelf().getId();

    const bool okCamera = pl::modmenu::ModuleBuilder(kModCameraModule, "VR Camera")
        .modId(modId)
        .description("First-person VR camera rig, height/position offset, sensitivity, FOV, and IPD.")
        .defaultEnabled(true)
        .config("eyeHeight", "Eye Height (m)", pl::modmenu::ConfigType::SliderFloat, "1.62", "0.5", "3.0")
        .config("fov", "Field of View (deg)", pl::modmenu::ConfigType::SliderFloat, "90", "60", "120")
        .config("sensitivity", "Camera Sensitivity", pl::modmenu::ConfigType::SliderFloat, "1.0", "0.1", "5.0")
        .config("ipd", "IPD (mm)", pl::modmenu::ConfigType::SliderFloat, "63", "45", "80")
        .config("livePreview", "Live Camera Preview", pl::modmenu::ConfigType::Toggle, "true", "", "")
        .registerModule();

    const bool okRecord = pl::modmenu::ModuleBuilder(kModRecordModule, "Start Recording")
        .modId(modId)
        .description("Live gameplay MP4 recording via Android MediaCodec/MediaMuxer.")
        .defaultEnabled(false)
        .onToggle([this](std::string_view, bool enabled) {
            if (enabled) {
                if (!mMod.renderBridge().isHooked()) {
                    mMod.getSelf().getLogger().warn(
                        "Cannot start live 360°/VR recording: {}", mMod.renderBridge().statusReason()
                    );
                    refreshHud();
                    return;
                }
                const auto outPath = mMod.getSelf().getDataDir() / "recordings" / "vrcraft_capture.mp4";
                std::string err;
                if (!mMod.recorder().start(outPath, mMod.config().recording, err)) {
                    mMod.getSelf().getLogger().error("MediaCodecRecorder start failed: {}", err);
                }
            } else {
                mMod.recorder().stop();
                mMod.gallery().refresh();
            }
            refreshHud();
        })
        .registerModule();

    const bool okGallery = pl::modmenu::ModuleBuilder(kModGalleryModule, "Video Gallery")
        .modId(modId)
        .description("Browse, inspect, play, rename, delete, share, and export recorded MP4s.")
        .defaultEnabled(true)
        .onToggle([this](std::string_view, bool enabled) {
            if (enabled) {
                mMod.gallery().refresh();
            }
            refreshHud();
        })
        .registerModule();

    const bool okExport = pl::modmenu::ModuleBuilder(kModExportModule, "VR Export")
        .modId(modId)
        .description("Export MP4 with 360° Mono, 360° Stereo, or VR180 Stereo GSpherical metadata.")
        .defaultEnabled(false)
        .config("mode", "Projection Mode", pl::modmenu::ConfigType::Radio, "Mono360", "Mono360,Stereo360,VR180Stereo", "")
        .config("fps", "Target FPS", pl::modmenu::ConfigType::Radio, "30", "30,60", "")
        .config("codec", "Video Codec", pl::modmenu::ConfigType::Radio, "H264", "H264,H265", "")
        .registerModule();

    const bool okSettings = pl::modmenu::ModuleBuilder(kModSettingsModule, "Settings")
        .modId(modId)
        .description("Persistent VRCraft 360° camera, audio, storage, and performance settings.")
        .defaultEnabled(true)
        .config("audio", "Record Audio", pl::modmenu::ConfigType::Toggle, "false", "", "")
        .config("asyncGpu", "Async GPU Sync", pl::modmenu::ConfigType::Toggle, "true", "", "")
        .onToggle([this](std::string_view, bool) {
            mMod.saveConfig();
            refreshHud();
        })
        .registerModule();

    const bool okBtnRec = pl::modmenu::ButtonBuilder(kBtnRecordToggle, "REC")
        .modId(modId)
        .moduleId(kModRecordModule)
        .label("REC")
        .androidKeyCode(0)
        .behavior(pl::modmenu::ButtonBehavior::Toggle)
        .onEvent([this](std::string_view, pl::modmenu::ButtonEvent ev, float) {
            if (ev != pl::modmenu::ButtonEvent::Click) {
                return;
            }
            if (mMod.recorder().isRecording()) {
                mMod.recorder().stop();
                mMod.gallery().refresh();
            } else if (!mMod.renderBridge().isHooked()) {
                mMod.getSelf().getLogger().warn("Recording blocked: {}", mMod.renderBridge().statusReason());
            } else {
                const auto outPath = mMod.getSelf().getDataDir() / "recordings" / "vrcraft_capture.mp4";
                std::string err;
                mMod.recorder().start(outPath, mMod.config().recording, err);
            }
            refreshHud();
        })
        .registerButton();

    const bool okBtnPause = pl::modmenu::ButtonBuilder(kBtnPauseResume, "Pause/Resume")
        .modId(modId)
        .moduleId(kModRecordModule)
        .label("II")
        .androidKeyCode(0)
        .behavior(pl::modmenu::ButtonBehavior::Click)
        .onEvent([this](std::string_view, pl::modmenu::ButtonEvent ev, float) {
            if (ev != pl::modmenu::ButtonEvent::Click) {
                return;
            }
            if (mMod.recorder().state() == RecordingState::Recording) {
                mMod.recorder().pause();
            } else if (mMod.recorder().state() == RecordingState::Paused) {
                mMod.recorder().resume();
            }
            refreshHud();
        })
        .registerButton();

    const bool okBtnReset = pl::modmenu::ButtonBuilder(kBtnResetCamera, "Reset Cam")
        .modId(modId)
        .moduleId(kModCameraModule)
        .label("CAM0")
        .androidKeyCode(0)
        .behavior(pl::modmenu::ButtonBehavior::Click)
        .onEvent([this](std::string_view, pl::modmenu::ButtonEvent ev, float) {
            if (ev == pl::modmenu::ButtonEvent::Click) {
                mMod.camera().resetCamera();
                refreshHud();
            }
        })
        .registerButton();

    const bool okBtnExport = pl::modmenu::ButtonBuilder(kBtnExportLatest, "Export")
        .modId(modId)
        .moduleId(kModExportModule)
        .label("EXP")
        .androidKeyCode(0)
        .behavior(pl::modmenu::ButtonBehavior::Click)
        .onEvent([this](std::string_view, pl::modmenu::ButtonEvent ev, float) {
            if (ev != pl::modmenu::ButtonEvent::Click) {
                return;
            }
            mMod.gallery().refresh();
            if (!mMod.gallery().entries().empty()) {
                const auto& latest = mMod.gallery().entries().front();
                const auto  dims   = resolveVideoDimensions(mMod.config().recording);
                const auto  dest   = std::filesystem::path(mMod.config().recording.exportSubdir) / latest.fileName;
                mMod.exporter().startExport(
                    latest.path,
                    dest,
                    mMod.config().recording.mode,
                    dims.width,
                    dims.height,
                    true
                );
            }
            refreshHud();
        })
        .registerButton();

    mRegistered = okCamera && okRecord && okGallery && okExport && okSettings &&
                  okBtnRec && okBtnPause && okBtnReset && okBtnExport;
    return mRegistered;
}

void VRCraftUI::unregisterMenuAndButtons() {
    if (!mRegistered) {
        return;
    }
    pl::modmenu::submitDrawCommands(mMod.getSelf().getId(), {});
    pl::modmenu::unregisterButton(kBtnExportLatest);
    pl::modmenu::unregisterButton(kBtnResetCamera);
    pl::modmenu::unregisterButton(kBtnPauseResume);
    pl::modmenu::unregisterButton(kBtnRecordToggle);

    pl::modmenu::unregisterModule(kModSettingsModule);
    pl::modmenu::unregisterModule(kModExportModule);
    pl::modmenu::unregisterModule(kModGalleryModule);
    pl::modmenu::unregisterModule(kModRecordModule);
    pl::modmenu::unregisterModule(kModCameraModule);
    mRegistered = false;
}

void VRCraftUI::refreshHud() {
    if (!mRegistered) {
        return;
    }

    const auto dims = resolveVideoDimensions(mMod.config().recording);
    const auto freeBytes = mMod.recorder().availableStorageBytes(mMod.getSelf().getDataDir());
    const double freeGb  = static_cast<double>(freeBytes) / (1024.0 * 1024.0 * 1024.0);

    char statusBuf[256];
    if (!mMod.renderBridge().isHooked()) {
        std::snprintf(
            statusBuf,
            sizeof(statusBuf),
            "VRCraft 360 | %dx%d @ %dFPS | Free: %.1fGB | HOOK_MISSING_1_26_66_1",
            dims.width,
            dims.height,
            mMod.config().recording.fps,
            freeGb
        );
    } else if (mMod.recorder().isRecording()) {
        const double sec = mMod.recorder().elapsedSeconds();
        const int    mm  = static_cast<int>(sec) / 60;
        const int    ss  = static_cast<int>(sec) % 60;
        const char*  tag = (mMod.recorder().state() == RecordingState::Paused) ? "PAUSED" : "REC";
        std::snprintf(
            statusBuf,
            sizeof(statusBuf),
            "%s %02d:%02d | %dx%d @ %dFPS | Free: %.1fGB",
            tag,
            mm,
            ss,
            dims.width,
            dims.height,
            mMod.config().recording.fps,
            freeGb
        );
    } else {
        std::snprintf(
            statusBuf,
            sizeof(statusBuf),
            "VRCraft 360 READY | %dx%d @ %dFPS | Gallery: %zu | Free: %.1fGB",
            dims.width,
            dims.height,
            mMod.config().recording.fps,
            mMod.gallery().entries().size(),
            freeGb
        );
    }

    const std::vector<pl::modmenu::DrawCommand> cmds = {
        {
            .type = pl::modmenu::DrawCommandType::Text,
            .x    = 16.0f,
            .y    = 16.0f,
            .text = statusBuf,
        },
    };
    pl::modmenu::submitDrawCommands(kModRecordModule, cmds);
}

} // namespace vrcraft
