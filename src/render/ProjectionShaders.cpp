#include "render/ProjectionShaders.hpp"

namespace vrcraft::shaders {

const char* kFullscreenQuadVertexShader = R"(#version 300 es
precision highp float;
out vec2 vUv;
void main() {
    vec2 pos = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    vUv = pos;
    gl_Position = vec4(pos * 2.0 - 1.0, 0.0, 1.0);
}
)";

// 360° Mono Equirectangular (2:1 aspect ratio)
const char* kEquirectMono360FragmentShader = R"(#version 300 es
precision highp float;
in vec2 vUv;
uniform samplerCube uCubeLeft;
out vec4 fragColor;

const float PI = 3.14159265359;

vec3 equirectDir(vec2 uv) {
    float lon = (uv.x - 0.5) * 2.0 * PI;
    float lat = (uv.y - 0.5) * PI;
    return vec3(cos(lat) * sin(lon), sin(lat), cos(lat) * cos(lon));
}

void main() {
    fragColor = texture(uCubeLeft, equirectDir(vUv));
}
)";

// 360° Stereo Top-Bottom Equirectangular (Top = Left Eye, Bottom = Right Eye)
const char* kEquirectStereo360TBFragmentShader = R"(#version 300 es
precision highp float;
in vec2 vUv;
uniform samplerCube uCubeLeft;
uniform samplerCube uCubeRight;
out vec4 fragColor;

const float PI = 3.14159265359;

vec3 equirectDir(vec2 uv) {
    float lon = (uv.x - 0.5) * 2.0 * PI;
    float lat = (uv.y - 0.5) * PI;
    return vec3(cos(lat) * sin(lon), sin(lat), cos(lat) * cos(lon));
}

void main() {
    if (vUv.y >= 0.5) {
        vec2 eyeUv = vec2(vUv.x, (vUv.y - 0.5) * 2.0);
        fragColor = texture(uCubeLeft, equirectDir(eyeUv));
    } else {
        vec2 eyeUv = vec2(vUv.x, vUv.y * 2.0);
        fragColor = texture(uCubeRight, equirectDir(eyeUv));
    }
}
)";

// VR180 Stereo Side-by-Side Equirectangular (Left half = Left Eye, Right half = Right Eye)
const char* kVR180StereoSBSFragmentShader = R"(#version 300 es
precision highp float;
in vec2 vUv;
uniform samplerCube uCubeLeft;
uniform samplerCube uCubeRight;
out vec4 fragColor;

const float PI = 3.14159265359;

vec3 vr180Dir(vec2 eyeUv) {
    float lon = (eyeUv.x - 0.5) * PI; // [-pi/2, +pi/2]
    float lat = (eyeUv.y - 0.5) * PI; // [-pi/2, +pi/2]
    return vec3(cos(lat) * sin(lon), sin(lat), cos(lat) * cos(lon));
}

void main() {
    if (vUv.x < 0.5) {
        vec2 eyeUv = vec2(vUv.x * 2.0, vUv.y);
        fragColor = texture(uCubeLeft, vr180Dir(eyeUv));
    } else {
        vec2 eyeUv = vec2((vUv.x - 0.5) * 2.0, vUv.y);
        fragColor = texture(uCubeRight, vr180Dir(eyeUv));
    }
}
)";

} // namespace vrcraft::shaders
