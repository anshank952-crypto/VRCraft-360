#include "render/EGLRenderPipeline.hpp"

#include "render/ProjectionShaders.hpp"

namespace vrcraft {

EGLRenderPipeline::~EGLRenderPipeline() {
    shutdown();
}

bool EGLRenderPipeline::initializeForEncoderWindow(
    ANativeWindow* encoderSurface,
    int            outWidth,
    int            outHeight,
    CaptureMode    mode
) {
    shutdown();
    if (!encoderSurface || outWidth <= 0 || outHeight <= 0) {
        return false;
    }

    mDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (mDisplay == EGL_NO_DISPLAY || !eglInitialize(mDisplay, nullptr, nullptr)) {
        return false;
    }

    const EGLint configAttribs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
        EGL_RED_SIZE,        8,
        EGL_GREEN_SIZE,      8,
        EGL_BLUE_SIZE,       8,
        EGL_ALPHA_SIZE,      8,
        EGL_RECORDABLE_ANDROID, EGL_TRUE,
        EGL_NONE
    };

    EGLint numConfigs = 0;
    if (!eglChooseConfig(mDisplay, configAttribs, &mConfig, 1, &numConfigs) || numConfigs < 1) {
        shutdown();
        return false;
    }

    const EGLint ctxAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    mContext = eglCreateContext(mDisplay, mConfig, EGL_NO_CONTEXT, ctxAttribs);
    if (mContext == EGL_NO_CONTEXT) {
        shutdown();
        return false;
    }

    mSurface = eglCreateWindowSurface(mDisplay, mConfig, encoderSurface, nullptr);
    if (mSurface == EGL_NO_SURFACE) {
        shutdown();
        return false;
    }

    if (!eglMakeCurrent(mDisplay, mSurface, mSurface, mContext)) {
        shutdown();
        return false;
    }

    mEglPresentationTimeAndroid = reinterpret_cast<PFNEGLPRESENTATIONTIMEANDROIDPROC>(
        eglGetProcAddress("eglPresentationTimeANDROID")
    );

    if (!compilePrograms()) {
        shutdown();
        return false;
    }

    glGenVertexArrays(1, &mVao);
    mCubeLeft  = createCubemapTexture(1024);
    mCubeRight = createCubemapTexture(1024);

    mWidth       = outWidth;
    mHeight      = outHeight;
    mMode        = mode;
    mInitialized = true;
    return true;
}

bool EGLRenderPipeline::renderEquirectangularFrame(int64_t presentationTimeNs) {
    if (!mInitialized) {
        return false;
    }

    if (!eglMakeCurrent(mDisplay, mSurface, mSurface, mContext)) {
        return false;
    }

    glViewport(0, 0, mWidth, mHeight);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    GLuint activeProg = mProgMono360;
    if (mMode == CaptureMode::Stereo360) {
        activeProg = mProgStereo360;
    } else if (mMode == CaptureMode::VR180Stereo) {
        activeProg = mProgVR180;
    }

    glUseProgram(activeProg);
    glBindVertexArray(mVao);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, mCubeLeft);
    glUniform1i(glGetUniformLocation(activeProg, "uCubeLeft"), 0);

    if (mMode != CaptureMode::Mono360) {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_CUBE_MAP, mCubeRight);
        glUniform1i(glGetUniformLocation(activeProg, "uCubeRight"), 1);
    }

    glDrawArrays(GL_TRIANGLES, 0, 3);

    if (mEglPresentationTimeAndroid) {
        mEglPresentationTimeAndroid(mDisplay, mSurface, presentationTimeNs);
    }

    return eglSwapBuffers(mDisplay, mSurface) == EGL_TRUE;
}

void EGLRenderPipeline::shutdown() {
    if (mDisplay != EGL_NO_DISPLAY) {
        eglMakeCurrent(mDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    }
    if (mVao) {
        glDeleteVertexArrays(1, &mVao);
        mVao = 0;
    }
    if (mCubeLeft) {
        glDeleteTextures(1, &mCubeLeft);
        mCubeLeft = 0;
    }
    if (mCubeRight) {
        glDeleteTextures(1, &mCubeRight);
        mCubeRight = 0;
    }
    if (mProgMono360) {
        glDeleteProgram(mProgMono360);
        mProgMono360 = 0;
    }
    if (mProgStereo360) {
        glDeleteProgram(mProgStereo360);
        mProgStereo360 = 0;
    }
    if (mProgVR180) {
        glDeleteProgram(mProgVR180);
        mProgVR180 = 0;
    }
    if (mSurface != EGL_NO_SURFACE) {
        eglDestroySurface(mDisplay, mSurface);
        mSurface = EGL_NO_SURFACE;
    }
    if (mContext != EGL_NO_CONTEXT) {
        eglDestroyContext(mDisplay, mContext);
        mContext = EGL_NO_CONTEXT;
    }
    if (mDisplay != EGL_NO_DISPLAY) {
        eglTerminate(mDisplay);
        mDisplay = EGL_NO_DISPLAY;
    }
    mInitialized = false;
}

bool EGLRenderPipeline::compilePrograms() {
    mProgMono360   = createProgram(shaders::kFullscreenQuadVertexShader, shaders::kEquirectMono360FragmentShader);
    mProgStereo360 = createProgram(shaders::kFullscreenQuadVertexShader, shaders::kEquirectStereo360TBFragmentShader);
    mProgVR180     = createProgram(shaders::kFullscreenQuadVertexShader, shaders::kVR180StereoSBSFragmentShader);
    return mProgMono360 != 0 && mProgStereo360 != 0 && mProgVR180 != 0;
}

GLuint EGLRenderPipeline::compileShader(GLenum type, const char* source) {
    const GLuint shader = glCreateShader(type);
    if (!shader) {
        return 0;
    }
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint EGLRenderPipeline::createProgram(const char* vsSrc, const char* fsSrc) {
    const GLuint vs = compileShader(GL_VERTEX_SHADER, vsSrc);
    const GLuint fs = compileShader(GL_FRAGMENT_SHADER, fsSrc);
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return 0;
    }
    const GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok = GL_FALSE;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        glDeleteProgram(prog);
        return 0;
    }
    return prog;
}

GLuint EGLRenderPipeline::createCubemapTexture(int faceSizePx) {
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_CUBE_MAP, tex);
    for (int i = 0; i < 6; ++i) {
        glTexImage2D(
            GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
            0,
            GL_RGBA8,
            faceSizePx,
            faceSizePx,
            0,
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            nullptr
        );
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    return tex;
}

} // namespace vrcraft
