#pragma once

#include <string>

namespace vrcraft {

enum class RenderHookStatus {
    NotInstalled,
    MissingSignatures1_26_66_1,
    Installed,
};

// Explicit bridge for Minecraft Bedrock 1.26.66.1 ARM64 RenderDragon / Camera hooks.
// Does NOT invent or guess unverified memory offsets or symbol names.
class MinecraftRenderBridge {
public:
    bool installHooks();
    void removeHooks();

    [[nodiscard]] bool             isHooked() const { return mStatus == RenderHookStatus::Installed; }
    [[nodiscard]] RenderHookStatus status() const { return mStatus; }
    [[nodiscard]] std::string      statusReason() const;

private:
    RenderHookStatus mStatus = RenderHookStatus::NotInstalled;
};

} // namespace vrcraft
