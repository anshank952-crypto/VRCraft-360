#include "media/VRExporter.hpp"

#include "media/SphericalMetadata.hpp"

namespace vrcraft {

VRExporter::~VRExporter() {
    cancel();
    if (mWorker.joinable()) {
        mWorker.join();
    }
}

bool VRExporter::startExport(
    const std::filesystem::path& sourceMp4,
    const std::filesystem::path& destMp4,
    CaptureMode                  mode,
    int                          width,
    int                          height,
    bool                         injectSphericalMetadata
) {
    if (isExporting()) {
        mLastError = "An export is already running";
        return false;
    }
    if (mWorker.joinable()) {
        mWorker.join();
    }

    mCancelRequested = false;
    mProgress        = 0.05f;
    mLastError.clear();
    mState = ExportState::Exporting;

    mWorker = std::thread([this, sourceMp4, destMp4, mode, width, height, injectSphericalMetadata]() {
        if (mCancelRequested.load()) {
            mState = ExportState::Cancelled;
            return;
        }

        mProgress = 0.25f;
        std::string err;
        bool ok = false;
        if (injectSphericalMetadata) {
            ok = SphericalMetadata::injectIntoMp4(sourceMp4, destMp4, mode, width, height, err);
        } else {
            std::error_code ec;
            std::filesystem::create_directories(destMp4.parent_path(), ec);
            ok = std::filesystem::copy_file(
                sourceMp4, destMp4, std::filesystem::copy_options::overwrite_existing, ec
            );
            if (!ok) {
                err = ec.message();
            }
        }

        if (mCancelRequested.load()) {
            std::error_code ec;
            std::filesystem::remove(destMp4, ec);
            mState = ExportState::Cancelled;
            return;
        }

        if (!ok) {
            mLastError = err;
            mState     = ExportState::Failed;
            return;
        }

        mProgress = 1.0f;
        mState    = ExportState::Completed;
    });

    return true;
}

void VRExporter::cancel() {
    mCancelRequested = true;
}

} // namespace vrcraft
