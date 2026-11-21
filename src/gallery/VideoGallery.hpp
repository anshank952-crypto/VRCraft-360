#pragma once

#include <cstdint>
#include <filesystem>
#include <jni.h>
#include <string>
#include <vector>

namespace vrcraft {

struct VideoEntry {
    std::filesystem::path path;
    std::string           fileName;
    uint64_t              sizeBytes  = 0;
    double                durationMs = 0.0;
    int                   width      = 0;
    int                   height     = 0;
};

class VideoGallery {
public:
    VideoGallery(std::filesystem::path recordingsDir, JavaVM* jvm);

    void refresh();
    [[nodiscard]] const std::vector<VideoEntry>& entries() const { return mEntries; }

    bool renameVideo(size_t index, const std::string& newName);
    bool deleteVideo(size_t index);
    bool exportToAndroidStorage(size_t index, const std::filesystem::path& targetDir, std::filesystem::path& outPath);
    bool shareVideoViaAndroid(size_t index);
    bool playVideoViaAndroid(size_t index);

private:
    void probeMediaMetadata(VideoEntry& entry);
    bool launchAndroidUriIntent(const std::filesystem::path& filePath, const char* action);

    std::filesystem::path  mRecordingsDir;
    JavaVM*                mJvm = nullptr;
    std::vector<VideoEntry> mEntries;
};

} // namespace vrcraft
