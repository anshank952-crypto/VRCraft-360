cmake_minimum_required(VERSION 3.22)

project(VRCraft360 VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Mod metadata (overrideable via -D flags)
set(MOD_ID "vrcraft360" CACHE STRING "LeviLauncher mod ID")
set(MOD_NAME "VRCraft 360°" CACHE STRING "Mod display name")
set(MOD_AUTHOR "VRCraft360 Contributors" CACHE STRING "Mod author")
set(MOD_VERSION "${PROJECT_VERSION}" CACHE STRING "Mod version")
set(MOD_LIBRARY_NAME "vrcraft360" CACHE STRING "Native shared library name")
set(MOD_MINECRAFT_VERSIONS "[\"1.26.66.1\"]" CACHE STRING "Target Minecraft versions JSON array")

include(FetchContent)

# Fetch Boost.PFR (header-only, required by pl/Config.hpp)
FetchContent_Declare(
    boost_pfr
    GIT_REPOSITORY https://github.com/boostorg/pfr.git
    GIT_TAG 2.2.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(boost_pfr)

# Fetch magic_enum (required by pl/Config.hpp)
FetchContent_Declare(
    magic_enum
    GIT_REPOSITORY https://github.com/Neargye/magic_enum.git
    GIT_TAG v0.9.7
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(magic_enum)

# Fetch nlohmann/json (required by pl/Config.hpp)
FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
FetchContent_MakeAvailable(nlohmann_json)

# Fetch official LeviLauncher Android Preloader SDK 0.2.3 (matches LeviLauncher 1.7.1)
FetchContent_Declare(
    preloader_android
    GIT_REPOSITORY https://github.com/LiteLDev/preloader-android.git
    GIT_TAG 0.2.3
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(preloader_android)

# Collect VRCraft 360° C++20 sources
set(VRCRAFT_SOURCES
    src/main.cpp
    src/mod/Config.cpp
    src/mod/VRCraftMod.cpp
    src/camera/VRCameraController.cpp
    src/render/ProjectionShaders.cpp
    src/render/EGLRenderPipeline.cpp
    src/render/MinecraftRenderBridge.cpp
    src/media/MediaCodecRecorder.cpp
    src/media/SphericalMetadata.cpp
    src/media/VRExporter.cpp
    src/gallery/VideoGallery.cpp
    src/ui/VRCraftUI.cpp
)

add_library(${MOD_LIBRARY_NAME} SHARED ${VRCRAFT_SOURCES})

target_include_directories(${MOD_LIBRARY_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${boost_pfr_SOURCE_DIR}/include
)

target_compile_options(${MOD_LIBRARY_NAME}
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -fvisibility=hidden
)

# Link Preloader SDK + dependencies + Android NDK Media, EGL, GLESv3, Audio & JNI libraries
target_link_libraries(${MOD_LIBRARY_NAME}
    PRIVATE
        preloader
        magic_enum::magic_enum
        nlohmann_json::nlohmann_json
        android
        log
        EGL
        GLESv3
        mediandk
        aaudio
)

# Configure LeviLauncher manifest.json
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/manifest.json.in
    ${CMAKE_CURRENT_BINARY_DIR}/package/manifest.json
    @ONLY
)

# Packaging target: creates VRCraft360-<version>-<abi>.levipack (ZIP format)
if(DEFINED ANDROID_ABI)
    set(LEVIPACK_ABI "${ANDROID_ABI}")
else()
    set(LEVIPACK_ABI "arm64-v8a")
endif()

set(LEVIPACK_NAME "VRCraft360-${MOD_VERSION}-${LEVIPACK_ABI}.levipack")
set(LEVIPACK_STAGE_DIR "${CMAKE_CURRENT_BINARY_DIR}/package")

add_custom_target(levi_package
    DEPENDS ${MOD_LIBRARY_NAME}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${LEVIPACK_STAGE_DIR}/lib/${LEVIPACK_ABI}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${LEVIPACK_STAGE_DIR}/config"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:${MOD_LIBRARY_NAME}>"
        "${LEVIPACK_STAGE_DIR}/lib/${LEVIPACK_ABI}/lib${MOD_LIBRARY_NAME}.so"
    COMMAND ${CMAKE_COMMAND} -E chdir "${LEVIPACK_STAGE_DIR}"
        ${CMAKE_COMMAND} -E tar "cf" "${CMAKE_CURRENT_BINARY_DIR}/${LEVIPACK_NAME}" --format=zip .
    COMMENT "Packaging ${LEVIPACK_NAME} for LeviLauncher Android"
)
