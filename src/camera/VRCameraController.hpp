#pragma once

#include "mod/Config.hpp"

#include <array>

namespace vrcraft {

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Mat4 {
    std::array<float, 16> m{
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };
};

enum class Eye {
    Mono  = 0,
    Left  = 1,
    Right = 2,
};

enum class CubemapFace : int {
    PositiveX = 0,
    NegativeX = 1,
    PositiveY = 2,
    NegativeY = 3,
    PositiveZ = 4,
    NegativeZ = 5,
};

class VRCameraController {
public:
    void applySettings(const CameraSettings& settings);
    [[nodiscard]] const CameraSettings& settings() const { return mSettings; }

    void setBasePlayerPose(const Vec3& playerPos, float playerYawDeg, float playerPitchDeg);
    void addTouchLookDelta(float deltaX, float deltaY);
    void update(float deltaSeconds);
    void resetCamera();

    [[nodiscard]] Vec3  computeEyePosition(Eye eye) const;
    [[nodiscard]] Mat4  computeCubemapViewMatrix(Eye eye, CubemapFace face) const;
    [[nodiscard]] Mat4  computeCubemapProjectionMatrix(float nearPlane = 0.05f, float farPlane = 512.0f) const;
    [[nodiscard]] float yawDegrees() const { return mSmoothedYaw; }
    [[nodiscard]] float pitchDegrees() const { return mSmoothedPitch; }

private:
    CameraSettings mSettings{};
    Vec3           mPlayerPos{};
    float          mBaseYaw      = 0.0f;
    float          mBasePitch    = 0.0f;
    float          mOffsetYaw    = 0.0f;
    float          mOffsetPitch  = 0.0f;
    float          mSmoothedYaw  = 0.0f;
    float          mSmoothedPitch = 0.0f;
};

} // namespace vrcraft
