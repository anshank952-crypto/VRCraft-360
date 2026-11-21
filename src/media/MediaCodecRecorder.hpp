#pragma once

#include "mod/Config.hpp"

#include <android/native_window.h>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <media/NdkMediaCodec.h>
#include <media/NdkMediaMuxer.h>
#include <mutex>
#include <string>
#include <thread>

namespace vrcraft {

struct DeviceEncoderCapabilities {
    bool supportsH264      = false;
    bool supportsH265      = false;
    int  maxSupportedWidth = 1920;
    int  maxSupportedFps   = 30;
};

enum class RecordingState {
    Idle,
    Recording,
    Paused,
    Stopping,
};

class MediaCodecRecorder {
public:
    MediaCodecRecorder() = default;
    ~MediaCodecRecorder();

    void probeDeviceCapabilities();
    [[nodiscard]] const DeviceEncoderCapabilities& capabilities() const { return mCapabilities; }

    bool start(const std::filesystem::path& outputMp4Path, const RecordingSettings& settings, std::string& outError);
    void pause();
    void resume();
    bool stop();

    [[nodiscard]] RecordingState state() const { return mState.load(); }
    [[nodiscard]] bool           isRecording() const {
        const auto s = mState.load();
        return s == RecordingState::Recording || s == RecordingState::Paused;
    }
    [[nodiscard]] ANativeWindow* inputSurface() const { return mInputWindow; }
    [[nodiscard]] double         elapsedSeconds() const;
    [[nodiscard]] uint64_t       encodedFrameCount() const { return mEncodedFrames.load(); }
    [[nodiscard]] uint64_t       availableStorageBytes(const std::filesystem::path& dir) const;
    [[nodiscard]] std::filesystem::path currentOutputPath() const { return mOutputPath; }

private:
    void drainLoop();

    DeviceEncoderCapabilities mCapabilities{};
    std::atomic<RecordingState> mState{RecordingState::Idle};
    std::atomic<uint64_t>       mEncodedFrames{0};
    std::atomic<bool>           mStopRequested{false};

    AMediaCodec*   mVideoCodec  = nullptr;
    AMediaMuxer*   mMuxer       = nullptr;
    ANativeWindow* mInputWindow = nullptr;
    int            mOutputFd    = -1;
    ssize_t        mVideoTrack  = -1;
    bool           mMuxerStarted = false;

    std::filesystem::path mOutputPath;
    std::thread           mDrainThread;
    mutable std::mutex    mTimeMutex;

    std::chrono::steady_clock::time_point mStartTime{};
    std::chrono::steady_clock::time_point mPauseStartTime{};
    std::chrono::nanoseconds              mAccumulatedPause{0};
};

} // namespace vrcraft
