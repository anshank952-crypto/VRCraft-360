#include "media/SphericalMetadata.hpp"

#include <array>
#include <cstring>
#include <fstream>
#include <sstream>

namespace vrcraft {

namespace {
constexpr std::array<uint8_t, 16> kSphericalUuid = {
    0xFF, 0xCC, 0x82, 0x63, 0xF8, 0x55, 0x4A, 0x93,
    0x88, 0x14, 0x58, 0x7A, 0x02, 0x52, 0x1F, 0xDD
};

uint32_t readU32BE(const uint8_t* p) {
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | uint32_t(p[3]);
}

void writeU32BE(uint8_t* p, uint32_t v) {
    p[0] = uint8_t((v >> 24) & 0xFF);
    p[1] = uint8_t((v >> 16) & 0xFF);
    p[2] = uint8_t((v >> 8) & 0xFF);
    p[3] = uint8_t(v & 0xFF);
}

uint64_t readU64BE(const uint8_t* p) {
    return (uint64_t(readU32BE(p)) << 32) | uint64_t(readU32BE(p + 4));
}

void writeU64BE(uint8_t* p, uint64_t v) {
    writeU32BE(p, uint32_t(v >> 32));
    writeU32BE(p + 4, uint32_t(v & 0xFFFFFFFFULL));
}

bool tagEquals(const uint8_t* p, const char* tag) {
    return std::memcmp(p, tag, 4) == 0;
}

void shiftChunkOffsets(std::vector<uint8_t>& buf, size_t start, size_t end, int64_t delta) {
    size_t pos = start;
    while (pos + 8 <= end) {
        const uint32_t size = readU32BE(&buf[pos]);
        if (size < 8 || pos + size > end) {
            break;
        }
        const uint8_t* type = &buf[pos + 4];
        if (tagEquals(type, "trak") || tagEquals(type, "mdia") || tagEquals(type, "minf") || tagEquals(type, "stbl")) {
            shiftChunkOffsets(buf, pos + 8, pos + size, delta);
        } else if (tagEquals(type, "stco") && pos + 16 <= end) {
            const uint32_t count = readU32BE(&buf[pos + 12]);
            for (uint32_t i = 0; i < count && (pos + 16 + (i + 1) * 4) <= pos + size; ++i) {
                uint8_t* entry = &buf[pos + 16 + i * 4];
                writeU32BE(entry, static_cast<uint32_t>(static_cast<int64_t>(readU32BE(entry)) + delta));
            }
        } else if (tagEquals(type, "co64") && pos + 16 <= end) {
            const uint32_t count = readU32BE(&buf[pos + 12]);
            for (uint32_t i = 0; i < count && (pos + 16 + (i + 1) * 8) <= pos + size; ++i) {
                uint8_t* entry = &buf[pos + 16 + i * 8];
                writeU64BE(entry, static_cast<uint64_t>(static_cast<int64_t>(readU64BE(entry)) + delta));
            }
        }
        pos += size;
    }
}
} // namespace

std::string SphericalMetadata::buildGSphericalXml(CaptureMode mode, int width, int height) {
    std::ostringstream xml;
    xml << "<?xml version=\"1.0\"?>\n"
        << "<rdf:SphericalVideo xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\" "
        << "xmlns:GSpherical=\"http://ns.google.com/videos/1.0/spherical/\">\n"
        << "  <GSpherical:Spherical>true</GSpherical:Spherical>\n"
        << "  <GSpherical:Stitched>true</GSpherical:Stitched>\n"
        << "  <GSpherical:StitchingSoftware>VRCraft 360</GSpherical:StitchingSoftware>\n"
        << "  <GSpherical:ProjectionType>equirectangular</GSpherical:ProjectionType>\n";

    if (mode == CaptureMode::Stereo360) {
        xml << "  <GSpherical:StereoMode>top-bottom</GSpherical:StereoMode>\n";
    } else if (mode == CaptureMode::VR180Stereo) {
        const int eyeWidth = width / 2;
        xml << "  <GSpherical:StereoMode>left-right</GSpherical:StereoMode>\n"
            << "  <GSpherical:CroppedAreaImageWidthPixels>" << eyeWidth << "</GSpherical:CroppedAreaImageWidthPixels>\n"
            << "  <GSpherical:CroppedAreaImageHeightPixels>" << height << "</GSpherical:CroppedAreaImageHeightPixels>\n"
            << "  <GSpherical:FullPanoWidthPixels>" << (eyeWidth * 2) << "</GSpherical:FullPanoWidthPixels>\n"
            << "  <GSpherical:FullPanoHeightPixels>" << height << "</GSpherical:FullPanoHeightPixels>\n"
            << "  <GSpherical:CroppedAreaLeftPixels>" << (eyeWidth / 2) << "</GSpherical:CroppedAreaLeftPixels>\n"
            << "  <GSpherical:CroppedAreaTopPixels>0</GSpherical:CroppedAreaTopPixels>\n";
    } else {
        xml << "  <GSpherical:StereoMode>mono</GSpherical:StereoMode>\n";
    }

    xml << "</rdf:SphericalVideo>\n";
    return xml.str();
}

std::vector<uint8_t> SphericalMetadata::buildUuidBox(const std::string& xml) {
    const uint32_t totalSize = static_cast<uint32_t>(8 + 16 + xml.size());
    std::vector<uint8_t> box(totalSize);
    writeU32BE(box.data(), totalSize);
    std::memcpy(box.data() + 4, "uuid", 4);
    std::memcpy(box.data() + 8, kSphericalUuid.data(), 16);
    std::memcpy(box.data() + 24, xml.data(), xml.size());
    return box;
}

bool SphericalMetadata::injectIntoMp4(
    const std::filesystem::path& inputMp4,
    const std::filesystem::path& outputMp4,
    CaptureMode                  mode,
    int                          width,
    int                          height,
    std::string&                 outError
) {
    std::ifstream in(inputMp4, std::ios::binary | std::ios::ate);
    if (!in) {
        outError = "Cannot open input MP4";
        return false;
    }
    const auto fileSize = static_cast<size_t>(in.tellg());
    in.seekg(0);
    std::vector<uint8_t> data(fileSize);
    if (!in.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(fileSize))) {
        outError = "Failed to read input MP4";
        return false;
    }

    size_t moovOffset = SIZE_MAX;
    size_t moovSize   = 0;
    size_t mdatOffset = SIZE_MAX;

    size_t pos = 0;
    while (pos + 8 <= data.size()) {
        const uint32_t sz = readU32BE(&data[pos]);
        if (sz < 8 || pos + sz > data.size()) {
            break;
        }
        if (tagEquals(&data[pos + 4], "moov")) {
            moovOffset = pos;
            moovSize   = sz;
        } else if (tagEquals(&data[pos + 4], "mdat")) {
            mdatOffset = pos;
        }
        pos += sz;
    }

    if (moovOffset == SIZE_MAX) {
        outError = "MP4 moov atom not found";
        return false;
    }

    // Find first trak inside moov
    size_t trakOffset = SIZE_MAX;
    size_t trakSize   = 0;
    pos               = moovOffset + 8;
    while (pos + 8 <= moovOffset + moovSize) {
        const uint32_t sz = readU32BE(&data[pos]);
        if (sz < 8 || pos + sz > moovOffset + moovSize) {
            break;
        }
        if (tagEquals(&data[pos + 4], "trak")) {
            trakOffset = pos;
            trakSize   = sz;
            break;
        }
        pos += sz;
    }

    if (trakOffset == SIZE_MAX) {
        outError = "MP4 video trak atom not found";
        return false;
    }

    const auto uuidBox = buildUuidBox(buildGSphericalXml(mode, width, height));
    const auto delta   = static_cast<int64_t>(uuidBox.size());

    // Update trak and moov sizes
    writeU32BE(&data[trakOffset], static_cast<uint32_t>(trakSize + uuidBox.size()));
    writeU32BE(&data[moovOffset], static_cast<uint32_t>(moovSize + uuidBox.size()));

    // Insert UUID box at end of first trak
    const size_t insertPos = trakOffset + trakSize;
    data.insert(data.begin() + static_cast<std::ptrdiff_t>(insertPos), uuidBox.begin(), uuidBox.end());

    // If moov precedes mdat, shift chunk offsets inside moov
    if (mdatOffset != SIZE_MAX && moovOffset < mdatOffset) {
        shiftChunkOffsets(data, moovOffset + 8, moovOffset + moovSize + uuidBox.size(), delta);
    }

    std::error_code ec;
    std::filesystem::create_directories(outputMp4.parent_path(), ec);
    std::ofstream out(outputMp4, std::ios::binary | std::ios::trunc);
    if (!out) {
        outError = "Cannot open export MP4 for writing";
        return false;
    }
    out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    if (!out.good()) {
        outError = "Failed writing exported MP4";
        return false;
    }
    return true;
}

} // namespace vrcraft
