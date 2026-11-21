#include "gallery/VideoGallery.hpp"

#include <algorithm>
#include <fcntl.h>
#include <media/NdkMediaExtractor.h>
#include <media/NdkMediaFormat.h>
#include <unistd.h>

namespace vrcraft {

VideoGallery::VideoGallery(std::filesystem::path recordingsDir, JavaVM* jvm)
    : mRecordingsDir(std::move(recordingsDir)), mJvm(jvm) {}

void VideoGallery::refresh() {
    mEntries.clear();
    std::error_code ec;
    if (!std::filesystem::exists(mRecordingsDir, ec)) {
        return;
    }

    for (const auto& item : std::filesystem::directory_iterator(mRecordingsDir, ec)) {
        if (!item.is_regular_file(ec)) {
            continue;
        }
        if (item.path().extension() != ".mp4") {
            continue;
        }
        VideoEntry entry{};
        entry.path      = item.path();
        entry.fileName  = item.path().filename().string();
        entry.sizeBytes = item.file_size(ec);
        probeMediaMetadata(entry);
        mEntries.push_back(std::move(entry));
    }

    std::sort(mEntries.begin(), mEntries.end(), [](const VideoEntry& a, const VideoEntry& b) {
        return a.fileName > b.fileName;
    });
}

void VideoGallery::probeMediaMetadata(VideoEntry& entry) {
    const int fd = ::open(entry.path.c_str(), O_RDONLY);
    if (fd < 0) {
        return;
    }
    AMediaExtractor* extractor = AMediaExtractor_new();
    if (extractor && AMediaExtractor_setDataSourceFd(extractor, fd, 0, static_cast<off64_t>(entry.sizeBytes)) == AMEDIA_OK) {
        const size_t tracks = AMediaExtractor_getTrackCount(extractor);
        for (size_t i = 0; i < tracks; ++i) {
            AMediaFormat* fmt = AMediaExtractor_getTrackFormat(extractor, i);
            const char*   mime = nullptr;
            if (fmt && AMediaFormat_getString(fmt, AMEDIAFORMAT_KEY_MIME, &mime) && mime &&
                std::string_view(mime).rfind("video/", 0) == 0) {
                int64_t durationUs = 0;
                if (AMediaFormat_getInt64(fmt, AMEDIAFORMAT_KEY_DURATION, &durationUs)) {
                    entry.durationMs = static_cast<double>(durationUs) / 1000.0;
                }
                AMediaFormat_getInt32(fmt, AMEDIAFORMAT_KEY_WIDTH, &entry.width);
                AMediaFormat_getInt32(fmt, AMEDIAFORMAT_KEY_HEIGHT, &entry.height);
                AMediaFormat_delete(fmt);
                break;
            }
            if (fmt) {
                AMediaFormat_delete(fmt);
            }
        }
    }
    if (extractor) {
        AMediaExtractor_delete(extractor);
    }
    ::close(fd);
}

bool VideoGallery::renameVideo(size_t index, const std::string& newName) {
    if (index >= mEntries.size() || newName.empty()) {
        return false;
    }
    std::filesystem::path target = mEntries[index].path.parent_path() / newName;
    if (target.extension() != ".mp4") {
        target.replace_extension(".mp4");
    }
    std::error_code ec;
    std::filesystem::rename(mEntries[index].path, target, ec);
    if (ec) {
        return false;
    }
    refresh();
    return true;
}

bool VideoGallery::deleteVideo(size_t index) {
    if (index >= mEntries.size()) {
        return false;
    }
    std::error_code ec;
    const bool removed = std::filesystem::remove(mEntries[index].path, ec);
    if (removed) {
        refresh();
    }
    return removed;
}

bool VideoGallery::exportToAndroidStorage(
    size_t                       index,
    const std::filesystem::path& targetDir,
    std::filesystem::path&       outPath
) {
    if (index >= mEntries.size()) {
        return false;
    }
    std::error_code ec;
    std::filesystem::create_directories(targetDir, ec);
    outPath = targetDir / mEntries[index].fileName;
    return std::filesystem::copy_file(
        mEntries[index].path, outPath, std::filesystem::copy_options::overwrite_existing, ec
    );
}

bool VideoGallery::shareVideoViaAndroid(size_t index) {
    if (index >= mEntries.size()) {
        return false;
    }
    return launchAndroidUriIntent(mEntries[index].path, "android.intent.action.SEND");
}

bool VideoGallery::playVideoViaAndroid(size_t index) {
    if (index >= mEntries.size()) {
        return false;
    }
    return launchAndroidUriIntent(mEntries[index].path, "android.intent.action.VIEW");
}

bool VideoGallery::launchAndroidUriIntent(const std::filesystem::path& filePath, const char* action) {
    if (!mJvm) {
        return false;
    }
    JNIEnv* env     = nullptr;
    bool    attached = false;
    if (mJvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) == JNI_EDETACHED) {
        if (mJvm->AttachCurrentThread(&env, nullptr) != JNI_OK) {
            return false;
        }
        attached = true;
    }
    if (!env) {
        return false;
    }

    jclass intentCls = env->FindClass("android/content/Intent");
    jclass uriCls    = env->FindClass("android/net/Uri");
    jclass actThread = env->FindClass("android/app/ActivityThread");
    if (!intentCls || !uriCls || !actThread) {
        if (attached) mJvm->DetachCurrentThread();
        return false;
    }

    jmethodID curAppMid = env->GetStaticMethodID(actThread, "currentApplication", "()Landroid/app/Application;");
    jobject   appObj    = curAppMid ? env->CallStaticObjectMethod(actThread, curAppMid) : nullptr;
    if (!appObj) {
        if (attached) mJvm->DetachCurrentThread();
        return false;
    }

    jstring jAction = env->NewStringUTF(action);
    jstring jMime   = env->NewStringUTF("video/mp4");
    jstring jUriStr = env->NewStringUTF(("file://" + filePath.string()).c_str());

    jmethodID parseMid = env->GetStaticMethodID(uriCls, "parse", "(Ljava/lang/String;)Landroid/net/Uri;");
    jobject   uriObj   = env->CallStaticObjectMethod(uriCls, parseMid, jUriStr);

    jmethodID intentCtor = env->GetMethodID(intentCls, "<init>", "(Ljava/lang/String;)V");
    jobject   intentObj  = env->NewObject(intentCls, intentCtor, jAction);

    jmethodID setDataTypeMid = env->GetMethodID(
        intentCls, "setDataAndType", "(Landroid/net/Uri;Ljava/lang/String;)Landroid/content/Intent;"
    );
    env->CallObjectMethod(intentObj, setDataTypeMid, uriObj, jMime);

    jmethodID addFlagsMid = env->GetMethodID(intentCls, "addFlags", "(I)Landroid/content/Intent;");
    constexpr jint kFlagActivityNewTask = 0x10000000;
    env->CallObjectMethod(intentObj, addFlagsMid, kFlagActivityNewTask);

    jclass    ctxCls        = env->FindClass("android/content/Context");
    jmethodID startActMid   = env->GetMethodID(ctxCls, "startActivity", "(Landroid/content/Intent;)V");
    env->CallVoidMethod(appObj, startActMid, intentObj);

    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        if (attached) mJvm->DetachCurrentThread();
        return false;
    }

    if (attached) {
        mJvm->DetachCurrentThread();
    }
    return true;
}

} // namespace vrcraft
