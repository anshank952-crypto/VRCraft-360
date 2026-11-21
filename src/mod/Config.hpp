#pragma once

#include <pl/Config.hpp>
#include <string>
#include <string_view>

namespace vrcraft {

enum class CaptureMode {
    Mono360,
    Stereo360,
    VR180Stereo,
};

enum class ResolutionPreset {
    Res2K_2560x1280,
    Res2K_2880x1440,
    Res4K_3840x1920,
    VR180_2880x2880,
};

enum class VideoCodecType {
    H264,
    H265,
};

enum class VideoQualityPreset {
    Standard,
    High,
    Ultra,
};

struct CameraSettings {
    float offsetX         = 0.0f;
    float offsetY         = 0.0f;
    float offsetZ         = 0.0f;
    float eyeHeightMeters = 1.62f;
    float sensitivity     = 1.0f;
    float smoothing       = 0.18f;
    float fovDegrees      = 90.0f;
    float ipdMillimeters  = 63.0f;
    bool  touchLookActive = true;
    bool  livePreview     = true;
};

struct RecordingSettings {
    CaptureMode        mode           = CaptureMode::Mono360;
    ResolutionPreset   resolution     = ResolutionPreset::Res2K_2560x1280;
    int                fps            = 30;
    VideoCodecType     codec          = VideoCodecType::H264;
    VideoQualityPreset quality        = VideoQualityPreset::High;
    bool               recordAudio    = false;
    std::string        storageSubdir  = "VRCraft360/Recordings";
    std::string        exportSubdir   = "/sdcard/Movies/VRCraft360";
    int                cubemapFacePx  = 1024;
    bool               asyncGpuSync   = true;
};

struct VRCraftConfig {
    int               version = 1;
    bool              enabled = true;
    CameraSettings    camera;
    RecordingSettings recording;
};

struct ResolvedVideoDimensions {
    int width  = 2560;
    int height = 1280;
    int bitrateBps = 28'000'000;
};

ResolvedVideoDimensions resolveVideoDimensions(const RecordingSettings& settings);

} // namespace vrcraft

template <>
struct pl::config::Schema<vrcraft::VRCraftConfig> {
    static constexpr std::string_view title       = "VRCraft 360° Configuration";
    static constexpr std::string_view description = "Persistent VR camera, recording, and export settings for VRCraft 360°.";

    static constexpr FieldSchema field(std::string_view name) {
        if (name == "version") {
            return {.title = "Config Version", .readOnly = true};
        }
        if (name == "enabled") {
            return {.title = "Enable VRCraft 360°", .description = "Enables the VRCraft HUD, camera controller, and recording pipeline."};
        }
        if (name == "camera") {
            return {.title = "VR Camera", .description = "First-person VR camera position, height, sensitivity, FOV, and IPD."};
        }
        if (name == "recording") {
            return {.title = "Recording & Export", .description = "360°/VR180 mode, resolution, FPS, codec, quality, audio, and storage paths."};
        }
        return {};
    }
};
