#include "mod/Config.hpp"

namespace vrcraft {

ResolvedVideoDimensions resolveVideoDimensions(const RecordingSettings& settings) {
    ResolvedVideoDimensions out{};

    if (settings.mode == CaptureMode::VR180Stereo || settings.resolution == ResolutionPreset::VR180_2880x2880) {
        out.width  = 2880;
        out.height = 1440;
        if (settings.mode == CaptureMode::VR180Stereo && settings.resolution == ResolutionPreset::VR180_2880x2880) {
            out.width  = 2880;
            out.height = 2880;
        }
    } else if (settings.mode == CaptureMode::Stereo360) {
        // 360° Stereo Top-Bottom uses 1:1 square container (two 2:1 equirectangular eyes stacked)
        switch (settings.resolution) {
            case ResolutionPreset::Res4K_3840x1920:
                out.width  = 3840;
                out.height = 3840;
                break;
            case ResolutionPreset::Res2K_2880x1440:
                out.width  = 2880;
                out.height = 2880;
                break;
            default:
                out.width  = 2560;
                out.height = 2560;
                break;
        }
    } else {
        // 360° Mono uses strict 2:1 equirectangular aspect ratio
        switch (settings.resolution) {
            case ResolutionPreset::Res4K_3840x1920:
                out.width  = 3840;
                out.height = 1920;
                break;
            case ResolutionPreset::Res2K_2880x1440:
                out.width  = 2880;
                out.height = 1440;
                break;
            default:
                out.width  = 2560;
                out.height = 1280;
                break;
        }
    }

    int baseBitrate = 20'000'000;
    if (out.width >= 3840) {
        baseBitrate = 45'000'000;
    } else if (out.width >= 2880) {
        baseBitrate = 28'000'000;
    }

    if (settings.fps >= 60) {
        baseBitrate = static_cast<int>(baseBitrate * 1.45f);
    }

    switch (settings.quality) {
        case VideoQualityPreset::Standard:
            out.bitrateBps = static_cast<int>(baseBitrate * 0.70f);
            break;
        case VideoQualityPreset::High:
            out.bitrateBps = baseBitrate;
            break;
        case VideoQualityPreset::Ultra:
            out.bitrateBps = static_cast<int>(baseBitrate * 1.40f);
            break;
    }

    if (settings.codec == VideoCodecType::H265) {
        out.bitrateBps = static_cast<int>(out.bitrateBps * 0.75f);
    }

    return out;
}

} // namespace vrcraft
