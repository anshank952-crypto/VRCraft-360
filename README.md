# VRCraft 360° (Minecraft Bedrock 1.26.66.1 — Android ARM64)

Original C++20 native mod for **LeviLauncher Android** (`arm64-v8a`) designed for real-time 360° Mono, 360° Stereo, and VR180 Stereo gameplay recording.

## Current Implementation Status
- **Implemented & Verified APIs**: Mod lifecycle (`<pl/Mod.hpp>`), persistent JSON config (`<pl/Config.hpp>`), ModMenu UI & HUD (`<pl/ModMenu.hpp>`), VR camera rig & cubemap math, GLES 3.0 equirectangular/VR180 shaders, Android `AMediaCodec`/`AMediaMuxer` hardware MP4 recorder, spherical metadata writer, and `VideoGallery` file/JNI manager.
- **Pending Game-Specific Hooks**: Live 360°/stereo world capture requires `libminecraftpe.so` (1.26.66.1 ARM64) rendering/camera signatures. See `MISSING_1_26_66_1_HOOKS.md` for the exact interfaces needed.

## Building on GitHub (From Your Android Phone)
1. Create a new GitHub repository and upload/push the contents of this `VRCraft360` folder.
2. Open the **Actions** tab in GitHub on your phone browser and run **Build VRCraft 360° (Android ARM64)** (or push to `main`).
3. Once the workflow finishes, download the **`VRCraft360-arm64-v8a-levipack`** artifact (unzip it once if GitHub wraps the artifact in a `.zip` to get `VRCraft360-0.1.0-arm64-v8a.levipack`).

## Installing in LeviLauncher (Android)
1. Open **LeviLauncher** on your Android phone (with Minecraft Bedrock **1.26.66.1** imported).
2. Go to **Mods** -> **Import Mod** and select `VRCraft360-0.1.0-arm64-v8a.levipack`.
3. Enable **VRCraft 360°** in the mod list and launch Minecraft.

## Upload from Android using Termux

Extract this ZIP, open Termux in the extracted project root, then run:

```bash
pkg update -y
pkg install git gh unzip -y
gh auth login
gh repo create VRCraft-360 --public --source=. --remote=origin --push
```

If you already created the repository on GitHub, instead run:

```bash
git init
git add .
git commit -m "Initial VRCraft 360 source"
git branch -M main
git remote add origin https://github.com/YOUR_GITHUB_USERNAME/VRCraft-360.git
git push -u origin main
```

The GitHub Actions workflow runs on pushes to `main` and can also be started
manually from the repository's Actions tab. A successful build uploads a
`VRCraft360-arm64-v8a-levipack` artifact.

**Important:** the Minecraft 1.26.66.1 camera/RenderDragon hooks are still
missing. A successful native build does not mean genuine 360°/stereo world
capture is operational. Those hooks must be verified and implemented first.
