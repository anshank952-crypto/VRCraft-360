#include "camera/VRCameraController.hpp"

#include <algorithm>
#include <cmath>

namespace vrcraft {

namespace {
constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;

Vec3 normalize(const Vec3& v) {
    const float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if (len < 1e-6f) {
        return {0.0f, 0.0f, 1.0f};
    }
    return {v.x / len, v.y / len, v.z / len};
}

Vec3 cross(const Vec3& a, const Vec3& b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

float dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Mat4 lookAt(const Vec3& eye, const Vec3& target, const Vec3& up) {
    const Vec3 f = normalize({target.x - eye.x, target.y - eye.y, target.z - eye.z});
    const Vec3 s = normalize(cross(f, up));
    const Vec3 u = cross(s, f);

    Mat4 out{};
    out.m = {
        s.x,  u.x, -f.x, 0.0f,
        s.y,  u.y, -f.y, 0.0f,
        s.z,  u.z, -f.z, 0.0f,
        -dot(s, eye), -dot(u, eye), dot(f, eye), 1.0f
    };
    return out;
}
} // namespace

void VRCameraController::applySettings(const CameraSettings& settings) {
    mSettings                = settings;
    mSettings.fovDegrees     = std::clamp(mSettings.fovDegrees, 60.0f, 120.0f);
    mSettings.ipdMillimeters = std::clamp(mSettings.ipdMillimeters, 45.0f, 80.0f);
    mSettings.sensitivity    = std::clamp(mSettings.sensitivity, 0.1f, 5.0f);
    mSettings.smoothing      = std::clamp(mSettings.smoothing, 0.0f, 0.95f);
}

void VRCameraController::setBasePlayerPose(const Vec3& playerPos, float playerYawDeg, float playerPitchDeg) {
    mPlayerPos = playerPos;
    mBaseYaw   = playerYawDeg;
    mBasePitch = playerPitchDeg;
}

void VRCameraController::addTouchLookDelta(float deltaX, float deltaY) {
    if (!mSettings.touchLookActive) {
        return;
    }
    const float scale = 0.15f * mSettings.sensitivity;
    mOffsetYaw += deltaX * scale;
    mOffsetPitch = std::clamp(mOffsetPitch + deltaY * scale, -89.0f, 89.0f);
}

void VRCameraController::update(float deltaSeconds) {
    const float targetYaw   = mBaseYaw + mOffsetYaw;
    const float targetPitch = std::clamp(mBasePitch + mOffsetPitch, -89.0f, 89.0f);

    if (mSettings.smoothing <= 0.01f || deltaSeconds <= 0.0f) {
        mSmoothedYaw   = targetYaw;
        mSmoothedPitch = targetPitch;
        return;
    }

    const float alpha = 1.0f - std::exp(-deltaSeconds * (12.0f * (1.0f - mSettings.smoothing) + 1.0f));
    mSmoothedYaw += (targetYaw - mSmoothedYaw) * alpha;
    mSmoothedPitch += (targetPitch - mSmoothedPitch) * alpha;
}

void VRCameraController::resetCamera() {
    mOffsetYaw     = 0.0f;
    mOffsetPitch   = 0.0f;
    mSmoothedYaw   = mBaseYaw;
    mSmoothedPitch = mBasePitch;
}

Vec3 VRCameraController::computeEyePosition(Eye eye) const {
    Vec3 center{
        mPlayerPos.x + mSettings.offsetX,
        mPlayerPos.y + mSettings.eyeHeightMeters + mSettings.offsetY,
        mPlayerPos.z + mSettings.offsetZ
    };

    if (eye == Eye::Mono) {
        return center;
    }

    const float ipdMeters  = mSettings.ipdMillimeters * 0.001f;
    const float halfIpd    = ipdMeters * 0.5f;
    const float yawRad     = mSmoothedYaw * kDegToRad;
    const Vec3  rightVec   = {std::cos(yawRad), 0.0f, std::sin(yawRad)};
    const float sign       = (eye == Eye::Left) ? -1.0f : 1.0f;

    return {
        center.x + rightVec.x * halfIpd * sign,
        center.y,
        center.z + rightVec.z * halfIpd * sign
    };
}

Mat4 VRCameraController::computeCubemapViewMatrix(Eye eye, CubemapFace face) const {
    const Vec3 eyePos = computeEyePosition(eye);

    Vec3 dir{};
    Vec3 up{};
    switch (face) {
        case CubemapFace::PositiveX: dir = { 1.0f,  0.0f,  0.0f}; up = {0.0f, -1.0f,  0.0f}; break;
        case CubemapFace::NegativeX: dir = {-1.0f,  0.0f,  0.0f}; up = {0.0f, -1.0f,  0.0f}; break;
        case CubemapFace::PositiveY: dir = { 0.0f,  1.0f,  0.0f}; up = {0.0f,  0.0f,  1.0f}; break;
        case CubemapFace::NegativeY: dir = { 0.0f, -1.0f,  0.0f}; up = {0.0f,  0.0f, -1.0f}; break;
        case CubemapFace::PositiveZ: dir = { 0.0f,  0.0f,  1.0f}; up = {0.0f, -1.0f,  0.0f}; break;
        case CubemapFace::NegativeZ: dir = { 0.0f,  0.0f, -1.0f}; up = {0.0f, -1.0f,  0.0f}; break;
    }

    // Rotate cubemap face direction by smoothed camera yaw for forward alignment
    const float yawRad = mSmoothedYaw * kDegToRad;
    const float c      = std::cos(yawRad);
    const float s      = std::sin(yawRad);

    const Vec3 rotDir{dir.x * c - dir.z * s, dir.y, dir.x * s + dir.z * c};
    const Vec3 rotUp{up.x * c - up.z * s, up.y, up.x * s + up.z * c};

    return lookAt(eyePos, {eyePos.x + rotDir.x, eyePos.y + rotDir.y, eyePos.z + rotDir.z}, rotUp);
}

Mat4 VRCameraController::computeCubemapProjectionMatrix(float nearPlane, float farPlane) const {
    // 90° FOV, 1:1 aspect ratio required for seamless cubemap faces
    const float f = 1.0f / std::tan(45.0f * kDegToRad);
    Mat4 proj{};
    proj.m = {
        f, 0.0f, 0.0f, 0.0f,
        0.0f, f, 0.0f, 0.0f,
        0.0f, 0.0f, (farPlane + nearPlane) / (nearPlane - farPlane), -1.0f,
        0.0f, 0.0f, (2.0f * farPlane * nearPlane) / (nearPlane - farPlane), 0.0f
    };
    return proj;
}

} // namespace vrcraft
