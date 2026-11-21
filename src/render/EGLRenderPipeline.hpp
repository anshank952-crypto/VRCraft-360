#pragma once

#include "mod/Config.hpp"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <android/native_window.h>

namespace vrcraft {

class EGLRenderPipeline {
public:
    EGLRenderPipeline() = default;
    ~EGLRenderPipeline();

    bool initializeForEncoderWindow(ANativeWindow* encoderSurface, int outWidth, int outHeight, CaptureMode mode);
    bool renderEquirectangularFrame(int64_t presentationTimeNs);
    void shutdown();

    [[nodiscard]] GLuint leftCubemapTexture() const { return mCubeLeft; }
    [[nodiscard]] GLuint rightCubemapTexture() const { return mCubeRight; }
    [[nodiscard]] bool   isInitialized() const { return mInitialized; }

private:
    bool compilePrograms();
    GLuint compileShader(GLenum type, const char* source);
    GLuint createProgram(const char* vsSrc, const char* fsSrc);
    GLuint createCubemapTexture(int faceSizePx);

    EGLDisplay mDisplay = EGL_NO_DISPLAY;
    EGLContext mContext = EGL_NO_CONTEXT;
    EGLSurface mSurface = EGL_NO_SURFACE;
    EGLConfig  mConfig  = nullptr;

    GLuint mVao            = 0;
    GLuint mProgMono360    = 0;
    GLuint mProgStereo360  = 0;
    GLuint mProgVR180      = 0;
    GLuint mCubeLeft       = 0;
    GLuint mCubeRight      = 0;

    int         mWidth       = 0;
    int         mHeight      = 0;
    CaptureMode mMode        = CaptureMode::Mono360;
    bool        mInitialized = false;

    PFNEGLPRESENTATIONTIMEANDROIDPROC mEglPresentationTimeAndroid = nullptr;
};

} // namespace vrcraft
