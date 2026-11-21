#include "media/MediaCodecRecorder.hpp"

#include <fcntl.h>
#include <media/NdkMediaFormat.h>
#include <sys/statvfs.h>
#include <unistd.h>

namespace vrcraft {

namespace {
constexpr const char* kMimeAvc  = "video/avc";
constexpr const char* kMimeHevc = "video/hevc";
constexpr int32_t     kColorFormatSurface = 0x7F000789; // COLOR_FormatSurface

bool testEncoderConfig(const char* mime, int width, int height, int fps, int bitrate) {
    AMediaCodec* codec = AMediaCodec_createEncoderByType(mime);
    if (!codec) {
        return false;
    }
    AMediaFormat* fmt = AMediaFormat_new();
    AMediaFormat_setString(fmt, AMEDIAFORMAT_KEY_MIME, mime);
    AMediaFormat_setInt32(fmt, AMEDIAFORMAT_KEY_WIDTH, width);
    AMediaFormat_setInt32(fmt, AMEDIAFORMAT_KEY_HEIGHT, height);
    AMediaFormat_setInt32(fmt, AMEDIAFORMAT_KEY_COLOR_FORMAT, kColorFormatSurface);
    AMediaFormat_setInt32(fmt, AMEDIAFORMAT_KEY_BIT_RATE, bitrate);
    AMediaFormat_setInt32(fmt, AMEDIAFORMAT_KEY_FRAME_RATE, fps);
    AMediaFormat_setInt32(fmt, AMEDIAFORMAT_KEY_I_FRAME_INTERVAL, 1);

    const media_status_t status = AMediaCodec_configure(codec, fmt, nullptr, nullptr, AMEDIACODEC_CONFIGURE_FLAG_ENCODE);
    AMediaFormat_delete(fmt);
    AMediaCodec_delete(codec);
    return status == AMEDIA_OK;
}
} // namespace

MediaCodecRecorder::~MediaCodecRecorder() {
    stop();
}

void MediaCodecRecorder::probeDeviceCapabilities() {
    mCapabilities.supportsH264 = testEncoderConfig(kMimeAvc, 2560, 1280, 30, 20'000'000);
    mCapabilities.supportsH265 = testEncoderConfig(kMimeHevc, 2560, 1280, 30, 16'000'000);

    const char* primaryMime = mCapabilities.supportsH265 ? kMimeHevc : kMimeAvc;
    if (testEncoderConfig(primaryMime, 3840, 1920, 60, 50'000'000)) {
        mCapabilities.maxSupportedWidth = 3840;
        mCapabilities.maxSupportedFps   = 60;
    } else if (testEncoderConfig(primaryMime, 3840, 1920, 30, 40'000'000)) {
        mCapabilities.maxSupportedWidth = 3840;
        mCapabilities.maxSupportedFps   = 30;
    } else if (testEncoderConfig(primaryMime, 2880, 1440, 60, 32'000'000)) {
        mCapabilities.maxSupportedWidth = 2880;
        mCapabilities.maxSupportedFps   = 60;
    } else if (testEncoderConfig(primaryMime, 2560, 1280, 30, 20'000'000)) {
        mCapabilities.maxSupportedWidth = 2560;
        mCapabilities.maxSupportedFps   = 30;
    } else {
        mCapabilities.maxSupportedWidth = 1920;
        mCapabilities.maxSupportedFps   = 30;
    }
}

bool MediaCodecRecorder::start(
    const std::filesystem::path& outputMp4Path,
    const RecordingSettings&     settings,
    std::string&                 outError
) {
    if (isRecording()) {
        outError = "Recording already in progress";
        return false;
    }

    const auto dims = resolveVideoDimensions(settings);
    const char* mime = (settings.codec == VideoCodecType::H265 && mCapabilities.supportsH265) ? kMimeHevc : kMimeAvc;

    std::error_code ec;
    std::filesystem::create_directories(outputMp4Path.parent_path(), ec);

    mOutputFd = ::open(outputMp4Path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (mOutputFd < 0) {
        outError = "Failed to open output MP4 file for writing";
        return false;
    }

    mMuxer = AMediaMuxer_new(mOutputFd, AMEDIAMUXER_OUTPUT_FORMAT_MPEG_4);
    if (!mMuxer) {
        ::close(mOutputFd);
        mOutputFd = -1;
        outError  = "Failed to create AMediaMuxer";
        return false;
    }

    mVideoCodec = AMediaCodec_createEncoderByType(mime);
    if (!mVideoCodec) {
        AMediaMuxer_delete(mMuxer);
        mMuxer = nullptr;
        ::close(mOutputFd);
        mOutputFd = -1;
        outError  = "Failed to create AMediaCodec encoder";
        return false;
    }

    AMediaFormat* fmt = AMediaFormat_new();
    AMediaFormat_setString(fmt, AMEDIAFORMAT_KEY_MIME, mime);
    AMediaFormat_setInt32(fmt, AMEDIAFORMAT_KEY_WIDTH, dims.width);
    AMediaFormat_setInt32(fmt, AMEDIAFORMAT_KEY_HEIGHT, dims.height);
    AMediaFormat_setInt32(fmt, AMEDIAFORMAT_KEY_COLOR_FORMAT, kColorFormatSurface);
    AMediaFormat_setInt32(fmt, AMEDIAFORMAT_KEY_BIT_RATE, dims.bitrateBps);
    AMediaFormat_setInt32(fmt, AMEDIAFORMAT_KEY_FRAME_RATE, settings.fps);
    AMediaFormat_setInt32(fmt, AMEDIAFORMAT_KEY_I_FRAME_INTERVAL, 1);

    if (AMediaCodec_configure(mVideoCodec, fmt, nullptr, nullptr, AMEDIACODEC_CONFIGURE_FLAG_ENCODE) != AMEDIA_OK) {
        AMediaFormat_delete(fmt);
        AMediaCodec_delete(mVideoCodec);
        AMediaMuxer_delete(mMuxer);
        ::close(mOutputFd);
        mVideoCodec = nullptr;
        mMuxer      = nullptr;
        mOutputFd   = -1;
        outError    = "AMediaCodec_configure rejected requested resolution/FPS/codec";
        return false;
    }
    AMediaFormat_delete(fmt);

    if (AMediaCodec_createInputSurface(mVideoCodec, &mInputWindow) != AMEDIA_OK || !mInputWindow) {
        AMediaCodec_delete(mVideoCodec);
        AMediaMuxer_delete(mMuxer);
        ::close(mOutputFd);
        mVideoCodec = nullptr;
        mMuxer      = nullptr;
        mOutputFd   = -1;
        outError    = "Failed to create MediaCodec input ANativeWindow";
        return false;
    }

    if (AMediaCodec_start(mVideoCodec) != AMEDIA_OK) {
        ANativeWindow_release(mInputWindow);
        AMediaCodec_delete(mVideoCodec);
        AMediaMuxer_delete(mMuxer);
        ::close(mOutputFd);
        mInputWindow = nullptr;
        mVideoCodec  = nullptr;
        mMuxer       = nullptr;
        mOutputFd    = -1;
        outError     = "AMediaCodec_start failed";
        return false;
    }

    mOutputPath       = outputMp4Path;
    mVideoTrack       = -1;
    mMuxerStarted     = false;
    mEncodedFrames    = 0;
    mStopRequested    = false;
    mAccumulatedPause = std::chrono::nanoseconds{0};
    mStartTime        = std::chrono::steady_clock::now();
    mState            = RecordingState::Recording;

    mDrainThread = std::thread(&MediaCodecRecorder::drainLoop, this);
    return true;
}

void MediaCodecRecorder::pause() {
    if (mState.load() == RecordingState::Recording) {
        std::lock_guard lock(mTimeMutex);
        mPauseStartTime = std::chrono::steady_clock::now();
        mState          = RecordingState::Paused;
    }
}

void MediaCodecRecorder::resume() {
    if (mState.load() == RecordingState::Paused) {
        std::lock_guard lock(mTimeMutex);
        mAccumulatedPause += std::chrono::steady_clock::now() - mPauseStartTime;
        mState             = RecordingState::Recording;
    }
}

bool MediaCodecRecorder::stop() {
    if (mState.load() == RecordingState::Idle) {
        return true;
    }
    mState         = RecordingState::Stopping;
    mStopRequested = true;

    if (mVideoCodec) {
        AMediaCodec_signalEndOfInputStream(mVideoCodec);
    }
    if (mDrainThread.joinable()) {
        mDrainThread.join();
    }
    if (mVideoCodec) {
        AMediaCodec_stop(mVideoCodec);
        AMediaCodec_delete(mVideoCodec);
        mVideoCodec = nullptr;
    }
    if (mInputWindow) {
        ANativeWindow_release(mInputWindow);
        mInputWindow = nullptr;
    }
    if (mMuxer) {
        if (mMuxerStarted) {
            AMediaMuxer_stop(mMuxer);
        }
        AMediaMuxer_delete(mMuxer);
        mMuxer = nullptr;
    }
    if (mOutputFd >= 0) {
        ::close(mOutputFd);
        mOutputFd = -1;
    }

    mMuxerStarted = false;
    mState        = RecordingState::Idle;
    return true;
}

void MediaCodecRecorder::drainLoop() {
    AMediaCodecBufferInfo info{};
    while (true) {
        const ssize_t index = AMediaCodec_dequeueOutputBuffer(mVideoCodec, &info, 10'000);
        if (index == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
            if (!mMuxerStarted) {
                AMediaFormat* outFmt = AMediaCodec_getOutputFormat(mVideoCodec);
                mVideoTrack          = AMediaMuxer_addTrack(mMuxer, outFmt);
                AMediaFormat_delete(outFmt);
                if (mVideoTrack >= 0 && AMediaMuxer_start(mMuxer) == AMEDIA_OK) {
                    mMuxerStarted = true;
                }
            }
        } else if (index >= 0) {
            size_t   bufSize = 0;
            uint8_t* buf     = AMediaCodec_getOutputBuffer(mVideoCodec, static_cast<size_t>(index), &bufSize);
            if (buf && info.size > 0 && (info.flags & AMEDIACODEC_BUFFER_FLAG_CODEC_CONFIG) == 0 && mMuxerStarted) {
                AMediaMuxer_writeSampleData(mMuxer, static_cast<size_t>(mVideoTrack), buf, &info);
                ++mEncodedFrames;
            }
            AMediaCodec_releaseOutputBuffer(mVideoCodec, static_cast<size_t>(index), false);
            if (info.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) {
                break;
            }
        } else if (mStopRequested.load() && index == AMEDIACODEC_INFO_TRY_AGAIN_LATER) {
            continue;
        }
    }
}

double MediaCodecRecorder::elapsedSeconds() const {
    const auto s = mState.load();
    if (s == RecordingState::Idle) {
        return 0.0;
    }
    std::lock_guard lock(mTimeMutex);
    const auto now = (s == RecordingState::Paused) ? mPauseStartTime : std::chrono::steady_clock::now();
    const auto dur = now - mStartTime - mAccumulatedPause;
    return std::chrono::duration<double>(dur).count();
}

uint64_t MediaCodecRecorder::availableStorageBytes(const std::filesystem::path& dir) const {
    struct statvfs fs{};
    if (::statvfs(dir.c_str(), &fs) != 0) {
        return 0;
    }
    return static_cast<uint64_t>(fs.f_bavail) * static_cast<uint64_t>(fs.f_frsize);
}

} // namespace vrcraft
