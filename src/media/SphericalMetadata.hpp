#pragma once

#include "mod/Config.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace vrcraft {

class SphericalMetadata {
public:
    static std::string buildGSphericalXml(CaptureMode mode, int width, int height);
    static std::vector<uint8_t> buildUuidBox(const std::string& xml);

    // Injects the GSpherical UUID box into an MP4 file's video trak atom,
    // updating parent box sizes and stco/co64 chunk offsets if moov precedes mdat.
    static bool injectIntoMp4(
        const std::filesystem::path& inputMp4,
        const std::filesystem::path& outputMp4,
        CaptureMode                  mode,
        int                          width,
        int                          height,
        std::string&                 outError
    );
};

} // namespace vrcraft
