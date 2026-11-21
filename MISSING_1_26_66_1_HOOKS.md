# Missing Minecraft Bedrock 1.26.66.1 Rendering Interfaces

VRCraft 360° uses only verified [LeviLauncher Preloader Android](https://github.com/LiteLDev/preloader-android) (`v0.2.2`) APIs and standard Android NDK libraries (`AMediaCodec`, `AMediaMuxer`, `AMediaExtractor`, `AAudio`, `EGL`, `GLESv3`, `JNI`).

Because `libminecraftpe.so` (Minecraft Bedrock Edition **1.26.66.1**, `arm64-v8a`) is proprietary and not bundled with Preloader, **no unverified Minecraft function signatures, offsets, or RenderDragon hooks have been invented**.

## What Already Works Without Game Hooks
- **Mod Lifecycle & Registration**: `PL_REGISTER_MOD`, `load()`, `enable()`, `disable()`, `unload()` via `<pl/Mod.hpp>`.
- **Persistent Settings**: Typed JSON config & schema via `<pl/Config.hpp>` (`pl::config::ConfigFile<VRCraftConfig>`).
- **Mod Menu & Floating HUD**: Runtime modules, sliders/toggles, floating buttons, and HUD draw commands via `<pl/ModMenu.hpp>`.
- **VR Camera Math**: 3-DOF/6-DOF pose, height/position offsets, sensitivity smoothing, FOV clamping, IPD eye-separation matrices, and 6-face cubemap view matrices (`VRCameraController`).
- **Equirectangular & Stereo Shaders**: OpenGL ES 3.0 shaders for 360° Mono, 360° Stereo (Top/Bottom), and VR180 Stereo (Side-by-Side) (`ProjectionShaders`).
- **Hardware Video Recording & Export**: Android `AMediaCodec` + `AMediaMuxer` MP4 pipeline, capability probing, pause/resume PTS tracking, spherical metadata box injection (`GSpherical` UUID / `st3d` / `sv3d`), and `VideoGallery` management.

## Exact Missing 1.26.66.1 Interfaces Required for Live 360°/Stereo Capture
To connect `MinecraftRenderBridge` (`src/render/MinecraftRenderBridge.cpp`) to live Minecraft world rendering, extract and verify these `arm64-v8a` signatures/symbols from your `libminecraftpe.so` (1.26.66.1) using Ghidra/IDA and register them with `<pl/memory/Signature.hpp>` and `<pl/memory/Hook.hpp>`:

1. **Camera / View Matrix Override (`Camera::setViewMatrix` or `LevelRenderer::renderLevel` camera setup)**
   - Needed to override the player's camera position, height offset, FOV (`90°` per cubemap face), and orientation for each of the 6 cubemap faces (`+X, -X, +Y, -Y, +Z, -Z`) and left/right eye IPD offsets (`±IPD/2` along the camera right vector).
2. **Multi-Pass World Render Entry (`LevelRenderer::renderLevel` / `GameRenderer::renderCurrentDistortedLevel` or RenderDragon `SceneRender` pass)**
   - Needed to trigger 6 cubemap render passes per eye (6 passes for 360° Mono, 12 passes for 360° Stereo, or 6–8 hemisphere passes for VR180 Stereo) into offscreen cubemap FBOs before the final swapchain present.
3. **RenderDragon Graphics Backend Texture / Swapchain Hook (GLES3 `eglSwapBuffers` or Vulkan `vkQueuePresentKHR` + offscreen FBO/Image target)**
   - Needed to bind VRCraft's left/right `GL_TEXTURE_CUBE_MAP` targets during the render passes and composite the final equirectangular frame into `EGLRenderPipeline`'s `AMediaCodec` input surface.

Until `MinecraftRenderBridge::isHooked()` returns `true` with verified 1.26.66.1 hooks, VRCraft 360° explicitly reports `HOOK_MISSING_1_26_66_1` in the HUD and refuses to fake 360°/stereo output by stretching a 2D screenshot.
