#pragma once

#include "mod/Config.hpp"

#include <atomic>
#include <filesystem>
#include <string>
#include <thread>

namespace vrcraft {

enum class ExportState {
    Idle,
    Exporting,
    Completed,
    Cancelled,
    Failed,
};

class VRExporter {
public:
    VRExporter() = default;
    ~VRExporter();

    bool startExport(
        const std::filesystem::path& sourceMp4,
        const std::filesystem::path& destMp4,
        CaptureMode                  mode,
        int                          width,
        int                          height,
        bool                         injectSphericalMetadata
    );
    void cancel();

    [[nodiscard]] bool        isExporting() const { return mState.load() == ExportState::Exporting; }
    [[nodiscard]] ExportState state() const { return mState.load(); }
    [[nodiscard]] float       progress() const { return mProgress.load(); }
    [[nodiscard]] std::string lastError() const { return mLastError; }

private:
    std::atomic<ExportState> mState{ExportState::Idle};
    std::atomic<float>       mProgress{0.0f};
    std::atomic<bool>        mCancelRequested{false};
    std::string              mLastError;
    std::thread              mWorker;
};

} // namespace vrcraft
